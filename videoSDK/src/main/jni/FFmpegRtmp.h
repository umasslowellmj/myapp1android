#ifndef FFMPEG_RTMP_H
#define FFMPEG_RTMP_H

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#endif

#include <time.h>
#include <stdbool.h>
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "put_bits.h"

typedef struct metadata_t {
    //  Video codec options
    int videoWidth;
    int videoHeight;
    int videoBitrate;
    //  Audio codec options
    int audioSampleRate;
    int audioBitRate;
    int numAudioChannels;
    //  Format options
    const char *outputFormatName;
    const char *outputFile;
} Metadata;


Metadata metadata;

enum AVPixelFormat VIDEO_PIX_FMT = AV_PIX_FMT_YUV420P;
enum AVCodecID VIDEO_CODEC_ID = AV_CODEC_ID_H264;
enum AVCodecID AUDIO_CODEC_ID = AV_CODEC_ID_AAC;
enum AVSampleFormat AUDIO_SAMPLE_FMT = AV_SAMPLE_FMT_S16;

// Create a timebase that will convert Android timestamps to FFMpeg encoder timestamps
#define androidSourceTimebase (AVRational) {1, 1000000}
//  This will be automatically set after calling write_header(), but we init it to something.
#define flvDestTimebase (AVRational) {1, 1000000}

AVFormatContext *outputFormatContext;
AVStream *audioStream = NULL, *videoStream = NULL;
AVPacket *packet = NULL;

int isConnectionOpen = 0;
int frameCount = 0;
bool foundKeyFrame = false;
bool foundConfigFrame = false;
int64_t lastPts[2];

void release_resources();
char *get_error_string(int errorNum, char *errBuf);
void populate_metadata_from_java(JNIEnv *env, jobject jOpts);
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);
extern void initConnection(JNIEnv *env);
int openConnection(JNIEnv *env);

#ifndef ANDROID
#define LOGE(...)  printf(__VA_ARGS__)
#define LOGI(...)  printf(stderr, __VA_ARGS__)
#else
#define LOG_TAG "FFmpegRtmp"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#endif /* FFMPEGRTMP_H */