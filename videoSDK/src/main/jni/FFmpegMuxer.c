#include "FFmpegMuxer.h"

/**
 * Take the two given input stream and mux them packet-by-packet into the given output context.
 */
void writeInterleaved(AVFormatContext *fmtA, AVFormatContext *fmtB,
                      AVFormatContext *outFmtCtx, bool performEncoding){
    //  Codec used to encode the video, if necessary.
    AVCodecContext *encoder = NULL;
    //  Find which of the two formats is video
    AVFormatContext *videoFormat, *audioFormat;
    if(isVideoStream(fmtA->streams[0])){
        videoFormat = fmtA;
        audioFormat = fmtB;
    }
    else{
        videoFormat = fmtB;
        audioFormat = fmtA;
    }
    if(VERBOSE) LOGI("\tWriting video file: %s\n"
                     "\tWriting audio file: %s\n",
                     (char*)(videoFormat->filename),
                     (char*)(audioFormat->filename));
    //  Make our life easier by holding the stream
    AVStream *inVideoStream, *inAudioStream;
    inVideoStream = videoFormat->streams[0];
    inAudioStream = audioFormat->streams[0];
    //  If encoding is needed, we need to allocate the encoder codec (MPEG-4).
    if(performEncoding){
        getEncoderCodec(encoder, inVideoStream);
    }
    //  Find the video stream in the output format
    AVStream *outputVideoStream, *outputAudioStream;
    if(isVideoStream(outFmtCtx->streams[0])){
        outputVideoStream = outFmtCtx->streams[0];
        outputAudioStream = outFmtCtx->streams[1];
    }
    else{
        outputVideoStream = outFmtCtx->streams[1];
        outputAudioStream = outFmtCtx->streams[0];
    }
    //  If the two files are of different length, let's right-align the files.
    int64_t videoDuration = getMsFromPts(videoFormat->duration, AV_TIME_BASE_Q);
    int64_t audioDuration = getMsFromPts(audioFormat->duration, AV_TIME_BASE_Q);
    int64_t skipVideoMs = 0;
    int64_t skipAudioMs = 0;
    if(VERBOSE) LOGE("Input video duration is %" PRId64 "\n", videoDuration);
    if(VERBOSE) LOGE("Input audio duration is %" PRId64 "\n", audioDuration);
    if(videoDuration > audioDuration){
        skipVideoMs = videoDuration - audioDuration;
    }
    else{
        skipAudioMs = audioDuration - videoDuration;
    }
    //  We have two packets and we compare the timestamps and write to the output file in order.
    AVPacket videoPacket, audioPacket;
    av_init_packet(&videoPacket);
    av_init_packet(&audioPacket);
    //  Position of new file will start at 0.
    int64_t currentTimeAudio, currentTimeVideo;
    currentTimeAudio = av_rescale_q(outFmtCtx->duration,
                                    AV_TIME_BASE_Q, audioFormat->streams[0]->time_base);
    currentTimeVideo = av_rescale_q(outFmtCtx->duration,
                                    AV_TIME_BASE_Q, videoFormat->streams[0]->time_base);
    //  Keep looping until either of the two streams are done.
    bool hasAudio = false, hasVideo = false, audioEOF = false, videoEOF = false;
    do{
        //  If video pts < audio pts or there is only video frames remaining.
        if((hasVideo && hasAudio &&
                comparePts(&videoPacket, &audioPacket, inVideoStream, inAudioStream) < 0) ||
                (!hasAudio && hasVideo)){
            int gotFrame = 0;
            if(performEncoding){
                gotFrame = reEncodePacket(encoder, videoFormat->streams[0], &videoPacket);
            }
            if(gotFrame >= 0){
                if(VERBOSE) LOGE("Writing video at time %" PRId64 ", duration %" PRId32 ".\n",
                                 getMsFromPts(currentTimeVideo, outputVideoStream->time_base),
                                 videoPacket.duration);
                writePacketInTime(&videoPacket, &currentTimeVideo, skipVideoMs,
                                  videoFormat, outputVideoStream, outFmtCtx);
            }
            hasVideo = false;
        }
        // If audio pts < video pts or there is only audio frames remaining.
        else if((hasVideo &&
                comparePts(&videoPacket, &audioPacket, inVideoStream, inAudioStream) > 0) ||
                (!hasVideo && hasAudio)){
            if(VERBOSE) LOGE("Writing audio at time %" PRId64 ", duration %" PRId32 ".\n",
                             getMsFromPts(currentTimeAudio, outputAudioStream->time_base),
                             audioPacket.duration);
            writePacketInTime(&audioPacket, &currentTimeAudio, skipAudioMs,
                              audioFormat, outputAudioStream, outFmtCtx);
            hasAudio = false;
        }
        //  Queue up the next audio and video frame if necessary.
        if(!hasVideo){
            hasVideo = (av_read_frame(videoFormat, &videoPacket) == 0);
            if(hasVideo){
                videoPacket.stream_index = outputVideoStream->index;
            }
            else{
                videoEOF = true;
            }
        }
        if(!hasAudio){
            hasAudio = (av_read_frame(audioFormat, &audioPacket) == 0);
            if(hasAudio){
                audioPacket.stream_index = outputAudioStream->index;
            }
            else{
                audioEOF = true;
            }
        }
    } while(!audioEOF && !videoEOF);
    if(audioEOF){
        outFmtCtx->duration = av_rescale_q(currentTimeAudio,
                                            audioFormat->streams[0]->time_base, AV_TIME_BASE_Q);
    }
    if(videoEOF){
        outFmtCtx->duration = av_rescale_q(currentTimeVideo,
                                            videoFormat->streams[0]->time_base, AV_TIME_BASE_Q);
    }

    if(VERBOSE) LOGE("Final audio time: %" PRId64 ", video time: %" PRId64 ".\n",
                     getMsFromPts(currentTimeAudio, outputAudioStream->time_base),
                     getMsFromPts(currentTimeVideo, outputVideoStream->time_base));
}

/**
 * Compare the pts of packetA from streamA to the pts of packetB from streamB and return -1
 * if A < B, and return 1 if B > A.
 */
int comparePts(AVPacket *packetA, AVPacket *packetB, AVStream *streamA, AVStream *streamB){
    return getMsFromPts(packetA->pts, streamA->time_base) <
            getMsFromPts(packetB->pts, streamB->time_base) ? -1 : 1;
}

/**
 * Helper function to convert the pts to a valid ms.
 */
int64_t getMsFromPts(int64_t pts, AVRational time_base){
    return av_rescale_q(pts, time_base, (AVRational) {1, 1000});
}

/**
 * Takes the given packet and writes it to the given output format A pointer to the current
 * timestamp is passed and updated accordingly. Also, a time offset in ms is passed.
 */
void writePacketInTime(AVPacket* packet, int64_t *currentTime, int64_t offsetTimeMs,
                       AVFormatContext* inFmt,
                       AVStream* outStream, AVFormatContext* outFmt){
    if(getMsFromPts(packet->pts, inFmt->streams[0]->time_base) >= offsetTimeMs){
        packet->pts = *currentTime;
        packet->dts = *currentTime;
        packet->duration = av_rescale_q(packet->duration,
                                        inFmt->streams[0]->time_base,
                                        outStream->time_base);
        (*currentTime) += packet->duration;
        av_write_frame(outFmt, packet);
    }
    else{
        if(VERBOSE) LOGE("Dropping packet at time %" PRId64 " because of offset.\n",
                         getMsFromPts(packet->pts, outStream->time_base));
    }
}

/**
 * Take the given video packet, decode it to a frame, re-encode it using the given encoder.
 */
int reEncodePacket(AVCodecContext *encoder, AVStream *inVideoStream, AVPacket *videoPacket){
    //  Allocate the frame needed to decode the video packet
    AVFrame *frame = av_frame_alloc();
    //  Decode the video packet into the frame.
    int gotFrame;
    int ret = avcodec_decode_video2(inVideoStream->codec, frame, &gotFrame, videoPacket);
    if(ret >= 0 && gotFrame >= 0){
        //  Re-encode it in the new format
        avcodec_encode_video2(inVideoStream->codec, videoPacket, frame, &gotFrame);
    }
    return gotFrame;
}

/**
 * Take all the input files and stitch them together into the output file.
 * Pass it the number of files, the list of files, the output file, and whether to perform encoding.
 */
int stitchFile(int numFiles, char* filesList[], char* outputFilePath, bool performEncoding) {
    //  The format and the stream for the pair of files (audio and video)
    AVFormatContext *formatA = NULL, *formatB = NULL, *outputFormat = NULL;
    //  Make the format for the output file.
    int ret = getOutputFormat(&outputFormat, outputFilePath);
    if(ret < 0){
        return ret;
    }

    for (int i = 0; i < numFiles; i+=2) {
        //  Get the formats of the audio and video file pair.
        getInputFormat(&formatA, filesList[i]);
        getInputFormat(&formatB, filesList[i+1]);
        //  The first video dictates the format for subsequent streams.
        if(i == 0){
            copyStreamToOutput(outputFormat,formatA->streams[0]);
            copyStreamToOutput(outputFormat,formatB->streams[0]);
            ret = avformat_write_header(outputFormat, NULL);
            if (ret < 0) {
                LOGE("Couldn't write the file header.\n");
                break;
            }
        }
        if(VERBOSE) av_dump_format(outputFormat, 0, filesList[i], 1);
        //  Sequentially write the audio and video to the output file.
        writeInterleaved(formatA, formatB, outputFormat, performEncoding);
        //  Release the allocated formats.
        releaseFormat(&formatA);
        releaseFormat(&formatB);
    }
    //  Release the allocated formats (in case they weren't).
    releaseFormat(&formatA);
    releaseFormat(&formatB);
    //  Write the output file trailer
    av_write_trailer(outputFormat);
    //  Release the allocated output format.
    releaseFormat(&outputFormat);
    return 0;
}

/**
 * Helper function returns whether the given stream is a video stream.
 */
bool isVideoStream(AVStream *avStream){
    return avStream->codec->codec_type == AVMEDIA_TYPE_VIDEO;
}

/**
 * Configure an MPEG-4 video encoder with same bitrate, etc. as the original video stream. Pass
 * the reference to the video codec to be initialized.
 * This is only called when encoding is really necessary.
 */
void getEncoderCodec(AVCodecContext *videoCodec, AVStream *videoStream) {
    AVCodec *encoder;
    encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!encoder) {
        LOGE("MPEG-4 video encoder not found.\n");
        return;
    }
    videoCodec = avcodec_alloc_context3(encoder);
    if (!videoCodec) {
        LOGE("Could not allocate video codec.\n");
        return;
    }
    //  Set the required parameters for the encoder based on input stream.
    videoCodec->bit_rate = videoStream->codec->bit_rate;
    videoCodec->width = videoStream->codec->width;
    videoCodec->height = videoStream->codec->height;
    videoCodec->time_base = (AVRational) {1, 30};
    videoCodec->gop_size = videoStream->codec->gop_size;
    videoCodec->max_b_frames = videoStream->codec->max_b_frames;
    videoCodec->pix_fmt = videoStream->codec->pix_fmt;
    //  Open the encoder for later use.
    if (avcodec_open2(videoCodec, encoder, NULL) < 0) {
        LOGE("Could not start video codec.\n");
        return;
    }
}

/**
 * Takes an input stream and copies its codec and its parameters to the given output format.
 */
void copyStreamToOutput(AVFormatContext *fmtCtx, AVStream *inStream){
    AVStream *newStream = avformat_new_stream(fmtCtx, inStream->codec->codec);
    //  Copy codec parameters from input stream to output stream.
    int ret = avcodec_copy_context(newStream->codec, inStream->codec);
    if (ret < 0) {
        LOGE("Failed to copy input stream to output stream.\n");
    }
    newStream->time_base = inStream->time_base;
    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        newStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    newStream->codec->codec_tag = 0;
    if(VERBOSE) LOGE("Added stream %d to output format.\n", newStream->index);
}

/**
 * Get the format for the given file. Pass the AVFormatContext by reference to get filled.
 */
int getOutputFormat(AVFormatContext** fmtCtx, char* outputFile) {
    avformat_alloc_output_context2(fmtCtx, NULL, NULL, outputFile);
    if (!(*fmtCtx)) {
        return -1;
    }

    //  Open the file for writing if necessary.
    if (!((*fmtCtx)->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&(*fmtCtx)->pb, outputFile, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("Could not open output file '%s'.", outputFile);
            return -1;
        }
        else{
            if(VERBOSE) LOGE("Opened output format for writing.\n");
        }
    }
    return 0;
}

/**
 * Get the format for the given file. Pass the AVFormatContext by reference to get filled.
 */
void getInputFormat(AVFormatContext** fmtCtx, char* filePath){
    if (avformat_open_input(fmtCtx, filePath, NULL, NULL) < 0) {
        LOGE("Could not open file %s\n", filePath);
    }
}

/**
 * Release a previously allocated format using getInputFormat() function above.
 */
void releaseFormat(AVFormatContext** fmtCtx){
    if(*fmtCtx != NULL){
        avformat_close_input(fmtCtx);
        if (*fmtCtx && !((*fmtCtx)->flags & AVFMT_NOFILE))
            avio_closep(&(*fmtCtx)->pb);
        avformat_free_context(*fmtCtx);
        *fmtCtx = NULL;
    }
}

/*
 * Iterate through each video file to make sure the codec are same. If not, we need to do encoding.
 */
bool needsEncoding(int numFiles, char* filesList[]){
    AVFormatContext *format = NULL; //  Holds the format of the file (eg. mp4).
    AVStream *stream;        //  Holds the stream/codec for the format (eg. H.264).
    enum AVCodecID videoCodecID = AV_CODEC_ID_NONE;
    bool needsEncoding = false;
    for (int i = 0; i < numFiles; i++) {
        if(VERBOSE) LOGE("Inspecting file %s.\n", filesList[i]);
        getInputFormat(&format, filesList[i]);
        avformat_find_stream_info(format, NULL);
        stream = format->streams[0];
        if (stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            //  If codec id doesn't match, we need to do encoding.
            if (videoCodecID != AV_CODEC_ID_NONE && videoCodecID != stream->codec->codec_id) {
                needsEncoding = true;
                releaseFormat(&format);
                break;
            }
        }
        releaseFormat(&format);
    }
    return needsEncoding;
}

/*
 * Stitch together a list of files. Arguments are the number of files to stitch and the array of
 * files that are to be stitched together. Last argument is the output file path.
 * If the format of the input files are not all the same, we will perform encoding to convert them
 * into the desired format.
 */
int muxFiles(int numFiles, char* filesList[], char* outputFileName){
    av_register_all();
    avcodec_register_all();

    //if(VERBOSE) av_log_set_level(AV_LOG_DEBUG);
    if(VERBOSE) LOGE("Muxing %d files into %s", numFiles, outputFileName);

    //  Loop through each file and check if the codecs are the same. If not, we need to encode.
    bool willEncode = needsEncoding(numFiles, filesList);

    if(VERBOSE) LOGI("Output file name: %s\n", outputFileName);
    if(VERBOSE) LOGI("Encoding is %s necessary.\n", willEncode ? "" : "not");

    //  Stitch the files together into an output file.
    stitchFile(numFiles, filesList, outputFileName, willEncode);
    return 0;
}

int main(int argc, char *argv[]) {
    //  Ensure there are at least 2 arguments (for audio and video) and there is a video file for
    //  every audio file that is being stitched together.
    if (argc < 2 || (argc - 2) % 2 != 0) {
        printf("usage: %s <audio file> <video file> ... <output file>\n"
                       "This is a test program to mux multiple mp4 audio and video files."
                       "\n", argv[0]);
        return 1;
    }

    //  Do the actual work. Pass it the number of files, list of files to mux, and the output file.
    int success = muxFiles((argc - 2), &argv[1], argv[argc - 1]);

    //  Return whether it worked.
    return success;
}

#ifdef ANDROID
JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_muxFiles(JNIEnv *env,
                                                              jobject  __unused instance,
                                                              jobjectArray filesArray) {
    //  Convert the java array into the necessary char* array.
    int stringCount = (int) (*env)->GetArrayLength(env, filesArray);
    char **paths = (char**)malloc(sizeof(char*) * stringCount);
    for (int i = 0; i < stringCount; i++) {
        jstring string = (jstring) (*env)->GetObjectArrayElement(env, filesArray, i);
        char *rawString = (char *) (*env)->GetStringUTFChars(env, string, 0);
        paths[i] = (char*)malloc(sizeof(char) * 256);
        strcpy(paths[i], rawString);
        if(VERBOSE) LOGE("Adding file %s", paths[i]);
        (*env)->ReleaseStringUTFChars(env, string, rawString);
    }
    if(VERBOSE) LOGE("Output file %s", paths[stringCount - 1]);
    muxFiles(stringCount - 2, (char**)paths, (char*)paths[stringCount - 1]);
    for (int i = 0; i < stringCount; i++){
        free(paths[i]);
    }
    free(paths);
}
#endif