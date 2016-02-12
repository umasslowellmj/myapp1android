package com.infinitetakes.stream.videoSDK;

import android.content.Context;
import android.opengl.GLSurfaceView;

import com.infinitetakes.stream.videoSDK.Video.CameraManager;
import com.infinitetakes.stream.videoSDK.Video.EncoderManager;
import com.infinitetakes.stream.videoSDK.Video.GLManager;
import com.infinitetakes.stream.videoSDK.Video.VideoConfig;

public class AVRecorder {
    GLManager glManager;
    CameraManager cameraManager;
    EncoderManager encoderManager;

    boolean previewStarted = false; // First onResume, just set the pre

    public AVRecorder(VideoConfig config, GLSurfaceView glView, RecorderCallback callback) {
        //  Constructs the object but doesn't do anything.
        glManager = new GLManager();
        cameraManager = new CameraManager(config);
        encoderManager = new EncoderManager();
        //  Tell them about each other
        glManager.setCameraManager(cameraManager);
        glManager.setEncoderManager(encoderManager);
        cameraManager.setGLManager(glManager);
        glManager.setSurfaceView(glView);
    }

    public void onDestroy() {
        cameraManager.onDestroy();
        glManager.onDestroy();
    }

    public void onResume(){
        if(!previewStarted){
            previewStarted = true;
            glManager.startPreview();
        }
        else{
            cameraManager.onResume();
            glManager.onResume();
        }
    }

    public void onPause(){
        cameraManager.onPause();
        glManager.onPause();
    }

    public interface RecorderCallback {

    }
}
