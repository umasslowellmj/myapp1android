package com.infinitetakes.stream.videoSDK.Video;

import android.hardware.Camera;

@SuppressWarnings("deprecation")
public class VideoConfig {
    public enum VIDEO_SIZE{
        RES_720p, RES_480p
    }

    private VIDEO_SIZE mPreviewSize = VIDEO_SIZE.RES_720p;
    private int mCameraChoice = Camera.CameraInfo.CAMERA_FACING_FRONT;

    public VideoConfig(VIDEO_SIZE previewSize, int cameraChoice){
        mPreviewSize = previewSize;
        mCameraChoice = cameraChoice;
    }

    public VIDEO_SIZE getPreviewSize(){
        return mPreviewSize;
    }

    public int getCameraType(){
        return mCameraChoice;
    }
}
