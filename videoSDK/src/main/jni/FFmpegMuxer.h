#ifndef FFMPEGMUXER_H
#define FFMPEGMUXER_H

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#endif

#include <stdbool.h>
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"

static bool VERBOSE = false;

/**
 * Take the two given input stream and mux them packet-by-packet into the given output context.
 */
void writeInterleaved(AVFormatContext *fmtA, AVFormatContext *fmtB, AVFormatContext *outFmtCtx, bool performEncoding);

/**
 * Takes the given packet and writes it to the given output format A pointer to the current
 * timestamp is passed and updated accordingly. Also, a time offset in ms is passed.
 */
void writePacketInTime(AVPacket* packet, int64_t *currentTime, int64_t offsetTimeMs,
                       AVFormatContext* inFmt,
                       AVStream* outStream, AVFormatContext* outFmt);

/**
 * Take the given video packet, decode it to a frame, re-encode it using the given encoder.
 */
int reEncodePacket(AVCodecContext *encoder, AVStream *inVideoStream, AVPacket *videoPacket);

/**
 * Take all the input files and stitch them together into the output file.
 * Pass it the number of files, the list of files, the output file, and whether to perform encoding.
 */
int stitchFile(int numFiles, char* filesList[], char* outputFilePath, bool performEncoding);

/**
 * Helper function returns whether the given stream is a video stream.
 */
bool isVideoStream(AVStream *avStream);

/**
 * Compare the pts of packetA from streamA to the pts of packetB from streamB and return -1
 * if A < B, and return 1 if B > A.
 */
int comparePts(AVPacket *packetA, AVPacket *packetB, AVStream *streamA, AVStream *streamB);

/**
 * Helper function to convert the pts to a valid ms
 */
int64_t getMsFromPts(int64_t pts, AVRational time_base);

/**
 * Configure an MPEG-4 video encoder with same bitrate, etc. as the original video stream. Pass
 * the reference to the video codec to be initialized.
 * This is only called when encoding is really necessary.
 */
void getEncoderCodec(AVCodecContext *videoCodec, AVStream *videoStream);

/**
 * Takes an input stream and copies its codec and its parameters to the given output format.
 */
void copyStreamToOutput(AVFormatContext *fmtCtx, AVStream *inStream);

/**
 * Get the format for the given file. Pass the AVFormatContext by reference to get filled.
 */
int getOutputFormat(AVFormatContext** fmtCtx, char* outputFile);

/**
 * Get the format for the given file. Pass the AVFormatContext by reference to get filled.
 */
void getInputFormat(AVFormatContext** fmtCtx, char* filePath);

/**
 * Find the stream within the given format. Pass the AVStream by reference to get filled.
 */
void getStream(AVStream* stream, AVFormatContext *fmtCtx);

/**
 * Release a previously allocated format using getInputFormat() function above.
 */
void releaseFormat(AVFormatContext** fmtCtx);

/*
 * Iterate through each video file to make sure the codec are same. If not, we need to do encoding.
 */
bool needsEncoding(int numFiles, char* filesList[]);

/*
 * Stitch together a list of files. Arguments are the number of files to stitch and the array of
 * files that are to be stitched together. Last argument is the output file path.
 * If the format of the input files are not all the same, we will perform encoding to convert them
 * into the desired format.
 */
int muxFiles(int numFiles, char* filesList[], char* outputFileName);

#ifndef ANDROID
#define LOGE(...)  fprintf(stderr,__VA_ARGS__)
#define LOGI(...)  printf(__VA_ARGS__)
#else
#define LOG_TAG "FFmpegMuxer"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#endif /* FFMPEGMUXER_H */