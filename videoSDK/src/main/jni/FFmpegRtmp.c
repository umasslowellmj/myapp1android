#include "FFmpegRtmp.h"

#ifdef ANDROID
JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_init(JNIEnv *env, jobject  __unused instance,
                                                          jobject jOpts){
    av_register_all();
    avformat_network_init();
    avcodec_register_all();

    // Get the values passed in from java side and populate the struct.
    populate_metadata_from_java(env, jOpts);
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_start(JNIEnv  __unused *env,
                                                           jobject  __unused instance) {
    //  Malloc the packet early for later use.
    if (!packet) {
        packet = av_malloc(sizeof(AVPacket));
        av_init_packet(packet);
    }
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_writePacketInterleaved(JNIEnv *env,
                                                                            jobject  __unused instance,
                                                                            jobject jData,
                                                                            jint jIsVideo,
                                                                            jint jSize,
                                                                            jlong jPts,
                                                                            jint jIsKeyFrame,
                                                                            jint jIsConfigFrame) {
    // Get the Byte array backing the Java ByteBuffer.
    uint8_t *data = (*env)->GetDirectBufferAddress(env, jData);

    // Put the necessary data from ByteBuffer into the AVPacket.
    packet->size = jSize;
    packet->data = data;

    //  Wait for config frame to come, since we need this to open the connection.
    if(jIsConfigFrame){
        foundConfigFrame = true;
        initConnection(env);
    }

    if(!isConnectionOpen){
        if(!foundConfigFrame){
            return;
        }
        //  Open the connection and write the header.
        LOGE("Reopening connection.");
        openConnection(env);
        //  If it fails, openConnection() will call releaseResources().
        if(!isConnectionOpen){
            return;
        }
    }

    //  Get the proper stream from the stream index.
    AVStream *stream = (jIsVideo == JNI_TRUE) ? videoStream : audioStream;
    if(!stream){
        return;
    }
    packet->stream_index = stream->index;

    //  Rescale the Android PTS to the stream's timebase.
    packet->pts = av_rescale_q((int64_t) jPts, androidSourceTimebase, stream->time_base);
    packet->dts = packet->pts;
    packet->duration = packet->pts - lastPts[packet->stream_index];
    lastPts[packet->stream_index] = packet->pts;

    //  If keyframe, set the flag.
    if (jIsKeyFrame == 1){
        packet->flags |= AV_PKT_FLAG_KEY;
        foundKeyFrame = true;
    }

    //  Let's make the first frame sent to be a KeyFrame, so things are smooth.
    if(!foundKeyFrame) {
        return;
    }
    if (av_write_frame(outputFormatContext, packet) < 0) {
        //  Fire the callback that connection has been lost to java.
        jclass thisClass = (*env)->GetObjectClass(env, instance);
        jmethodID connectionCallback = (*env)
                                        ->GetMethodID(env, thisClass, "onConnectionDropped", "()V");
        if (connectionCallback) {
            (*env)->CallVoidMethod(env, instance, connectionCallback);
        }
    }

    frameCount++;
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_stop(JNIEnv  __unused *env,
                                                          jobject  __unused instance) {
    release_resources();
}
#endif

void initConnection(JNIEnv *env) {
    int error = 0;
    AVCodec *audio_codec, *video_codec;

    isConnectionOpen = 0;

    if ((error = avformat_alloc_output_context2(&outputFormatContext, NULL,
                                                metadata.outputFormatName,
                                                metadata.outputFile)) < 0 || !outputFormatContext) {
        LOGE("Couldn't allocate the output context.");
        char errorStr[1024];
        get_error_string(error, errorStr);
        jclass exc = (*env)->FindClass(env, "java/lang/Exception");
        (*env)->ThrowNew(env, exc, errorStr);
        release_resources();
        return;
    }

    AVOutputFormat *fmt = outputFormatContext->oformat;
    if (fmt->audio_codec != AV_CODEC_ID_NONE && AUDIO_CODEC_ID != AV_CODEC_ID_NONE) {
        audioStream = add_stream(outputFormatContext, &audio_codec, AUDIO_CODEC_ID);
    }

    if (fmt->video_codec != AV_CODEC_ID_NONE && VIDEO_CODEC_ID != AV_CODEC_ID_NONE) {
        videoStream = add_stream(outputFormatContext, &video_codec, VIDEO_CODEC_ID);
    }


    if((fmt->video_codec != AV_CODEC_ID_NONE && VIDEO_CODEC_ID != AV_CODEC_ID_NONE && !videoStream)
        || (fmt->audio_codec != AV_CODEC_ID_NONE
                && AUDIO_CODEC_ID != AV_CODEC_ID_NONE && !audioStream)){
        jclass exc = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
        (*env)->ThrowNew(env, exc, "Couldn't create the stream.");
        release_resources();
        return;
    }

    // Debug the output format
    av_dump_format(outputFormatContext, 0, NULL, 1);

    // Verify that all the parameters have been set, or throw an IllegalArgumentException to Java.
    if (!metadata.videoHeight || !metadata.videoWidth || !metadata.audioSampleRate ||
        !metadata.videoBitrate || !metadata.audioBitRate ||
        !metadata.numAudioChannels || !metadata.outputFormatName || !metadata.outputFile) {
        jclass exc = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
        (*env)->ThrowNew(env, exc, "Make sure all the Metadata parameters have been passed.");
        release_resources();
        return;
    }
}


/**
 * Take the Java Metadata object and populate the C struct declared above with these parameters.
 */
void populate_metadata_from_java(JNIEnv *env, jobject jOpts) {
    jclass jMetadataClass = (*env)->GetObjectClass(env, jOpts);
    jfieldID jVideoHeightId = (*env)->GetFieldID(env, jMetadataClass, "videoHeight", "I");
    jfieldID jVideoWidthId = (*env)->GetFieldID(env, jMetadataClass, "videoWidth", "I");
    jfieldID jVideoBitrate = (*env)->GetFieldID(env, jMetadataClass, "videoBitrate", "I");
    jfieldID jAudioBitRateId = (*env)->GetFieldID(env, jMetadataClass, "audioBitRate", "I");
    jfieldID jAudioSampleRateId = (*env)->GetFieldID(env, jMetadataClass, "audioSampleRate", "I");
    jfieldID jNumAudioChannelsId = (*env)->GetFieldID(env, jMetadataClass, "numAudioChannels", "I");
    jfieldID jOutputFormatName = (*env)->GetFieldID(env, jMetadataClass, "outputFormatName",
                                                    "Ljava/lang/String;");
    jfieldID jOutputFile = (*env)->GetFieldID(env, jMetadataClass, "outputFile",
                                              "Ljava/lang/String;");
    jstring jStrOutputFormatName = (*env)->GetObjectField(env, jOpts, jOutputFormatName);
    jstring jStrOutputFile = (*env)->GetObjectField(env, jOpts, jOutputFile);

    metadata.videoHeight = (*env)->GetIntField(env, jOpts, jVideoHeightId);
    metadata.videoWidth = (*env)->GetIntField(env, jOpts, jVideoWidthId);
    metadata.videoBitrate = (*env)->GetIntField(env, jOpts, jVideoBitrate);
    metadata.audioBitRate = (*env)->GetIntField(env, jOpts, jAudioBitRateId);
    metadata.audioSampleRate = (*env)->GetIntField(env, jOpts, jAudioSampleRateId);
    metadata.numAudioChannels = (*env)->GetIntField(env, jOpts, jNumAudioChannelsId);
    metadata.outputFormatName = (*env)->GetStringUTFChars(env, jStrOutputFormatName, 0);
    metadata.outputFile = (*env)->GetStringUTFChars(env, jStrOutputFile, 0);
}

/**
 * Add an output stream to the given AVFormatContext.
 */
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id) {
    AVCodecContext *codecContext = NULL;
    AVStream *st = NULL;

    //  This will be null for video stream, since we're not encoding the video, only the audio.
    *codec = avcodec_find_encoder(codec_id);
    st = avformat_new_stream(oc, *codec);
    if (!st) {
        LOGE("Couldn't create the %s stream.", codec_id == VIDEO_CODEC_ID ? "video" : "audio");
        return NULL;
    }

    codecContext = st->codec;
    avcodec_get_context_defaults3(codecContext, *codec);
    st->time_base = flvDestTimebase;

    //  Add according stream parameters
    if (codec_id == VIDEO_CODEC_ID) {
        codecContext->codec_id = VIDEO_CODEC_ID;
        codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
        codecContext->bit_rate = metadata.videoBitrate;
        codecContext->width = metadata.videoWidth;
        codecContext->height = metadata.videoHeight;
        codecContext->pix_fmt = VIDEO_PIX_FMT;
        codecContext->framerate = (AVRational){30,1};
        av_opt_set(codecContext->priv_data, "profile", "baseline", 0);
        st->codec->codec_tag = 7;
        //  H.264 Annex-B requires you send SPS+PPS in extra data.
        codecContext->extradata = (uint8_t*)av_mallocz(packet->size);
        codecContext->extradata_size = packet->size;
        int i;
        for(i = 0; i < packet->size; i++){
            codecContext->extradata[i] = packet->data[i];
        }
    } else if (codec_id == AUDIO_CODEC_ID) {
        codecContext->codec_id = AUDIO_CODEC_ID;
        codecContext->codec_type = AVMEDIA_TYPE_AUDIO;
        codecContext->sample_fmt = AUDIO_SAMPLE_FMT;
        codecContext->sample_rate = metadata.audioSampleRate;
        codecContext->bit_rate = metadata.audioBitRate;
        codecContext->channels = metadata.numAudioChannels;
        /*codecContext->extradata = (uint8_t*)av_mallocz(2);
        codecContext->extradata_size = 2;
        //  Extra data for AAC LC should be 0x11 0x90.
        codecContext->extradata[0] = 0x11;
        codecContext->extradata[1] = 0x90;*/
        PutBitContext pb;
        codecContext->extradata = (uint8_t*)av_mallocz(2);
        codecContext->extradata_size = 2;
        init_put_bits(&pb, codecContext->extradata, codecContext->extradata_size);
        put_bits(&pb, 5, 2); //object type - AAC-LC
        put_bits(&pb, 4, 4); //sample rate index (44100hz)
        put_bits(&pb, 4, 2);

        flush_put_bits(&pb);
        st->codec->codec_tag = 10;
    }

    //  Some of the things we do aren't totally kosher.
    outputFormatContext->strict_std_compliance = FF_COMPLIANCE_STRICT;

    // Some formats want stream headers to be separate.
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

/**
 * Get the underlying message from an AVError.
 */
char *get_error_string(int errorNum, char *errBuf) {
    int ret = AVERROR(errorNum);
    av_strerror(ret, errBuf, 1024 * sizeof(char));
    LOGE("%s", errBuf);
    return errBuf;
}

/**
 * This function is called when an exception is thrown and we want to quit everything, or
 * when stop is called. It will try to release all the allocated resources, even if we're in a
 * bad state.
 */
void release_resources() {
    LOGI("Releasing resources.");
    isConnectionOpen = 0;
    frameCount = 0;
    foundKeyFrame = false;
    foundConfigFrame = false;
    lastPts[0] = 0;
    lastPts[1] = 1;

    //  Write the trailer to the file or stream.
    //av_write_trailer(outputFormatContext);

    if (videoStream) {
        LOGI("Closing video stream.");
        avcodec_close(videoStream->codec);
    }
    if (audioStream) {
        LOGI("Closing audio stream.");
        avcodec_close(audioStream->codec);
    }
    if (outputFormatContext) {
        LOGI("Freeing output context.");
        if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE))
            avio_close(outputFormatContext->pb);
        avformat_free_context(outputFormatContext);
    }
    if (packet) {
        LOGI("Freeing packet.");
        av_free_packet(packet);
    }
    LOGI("De-initializing network.");
    avformat_network_deinit();
}

int openConnection(JNIEnv *env){
    if (isConnectionOpen == 0) {
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (!avio_open(&outputFormatContext->pb, metadata.outputFile, AVIO_FLAG_WRITE)){
                LOGI("Opened connection success.");
                isConnectionOpen = 1;
            } else{
                jclass exc = (*env)->FindClass(env, "java/lang/Exception");
                (*env)->ThrowNew(env, exc, "Internet connection is not available.");
                release_resources();
                return 1;
            }
        } else {
            isConnectionOpen = 1;
        }
    }

    // Write the header to the stream.
    if(avformat_write_header(outputFormatContext, NULL) < 0){
        jclass exc = (*env)->FindClass(env, "java/lang/Exception");
        (*env)->ThrowNew(env, exc, "Couldn't write header to file.");
        release_resources();
        return 1;
    }

    return 0;
}