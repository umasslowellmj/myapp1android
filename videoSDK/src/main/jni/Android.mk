LOCAL_PATH := $(call my-dir)

APP_PLATFORM := android-10

include $(CLEAR_VARS)

LOCAL_MODULE := FFmpegWrapper
LOCAL_C_INCLUDES += ffmpeg/include/

ifeq ($(TARGET_ARCH),x86)
    LOCAL_LDLIBS += -Lffmpeg/lib/x86
else
    LOCAL_LDLIBS += -Lffmpeg/lib/armeabi-v7a
    LOCAL_CFLAGS += -march=armv7-a -mfloat-abi=hardfp -mfpu=neon
endif

LOCAL_LDLIBS += \
-llog -lz -lGLESv1_CM -lavformat -lavcodec -lavdevice -lswresample -lswscale -lavutil -lavfilter

LOCAL_SRC_FILES := \
    FFmpegRtmp.c \
    FFmpegMuxer.c

LOCAL_CFLAGS := -O0 -g -Wall --std=c99

include $(BUILD_SHARED_LIBRARY)
