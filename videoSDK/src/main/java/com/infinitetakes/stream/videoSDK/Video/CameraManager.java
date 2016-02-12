package com.infinitetakes.stream.videoSDK.Video;

import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.media.CamcorderProfile;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.List;

@SuppressWarnings("deprecation")
/**
 * All camera related stuff must go through this class so it is run on a single thread.
 * Here is how everything works:
 *      * Construct a CameraManager() and give it the configuration
 *      * setGLManager() to point it to the GLManager
 *      *   the glManager will call openCameraSync to tell it to ready the camera.
 *      *   the glManager will call setSurfaceTexture() once surface is ready.
 *      *   startPreview() on camera will be called and onFrameAvailable() will be called.
 *      *   glManager's renderer's onDrawFrame() will get called.
 */
public class CameraManager implements SurfaceTexture.OnFrameAvailableListener {
    private static final String TAG = "CameraManager";
    private static final boolean VERBOSE = true; // Very noisy.

    private VideoConfig mConfig;
    private CameraThread mCameraThread; //  Handler / Thread for handling camera operations.
    private Camera mCamera; //  Camera (only accessed through camera thread).
    private GLManager mGlManager; //  Reference to the thread handling GL stuff.
    private CameraSize mCurrentPreviewSize = new CameraSize(); //  Set after opening camera.
    private final Object mCameraLock = new Object(); // Synchronize camera operations

    public CameraManager(VideoConfig config) {
        mCameraThread = new CameraThread(this);
        mCameraThread.start(); //  WeakReference to this class is killed in onDestroy().
        //  Wait for the thread and looper to start up.
        synchronized (mCameraThread.mReadyLock) {
            while (mCameraThread.mCamManager == null) {
                try {
                    mCameraThread.mReadyLock.wait();
                } catch (InterruptedException ignored) {
                    Thread.currentThread().interrupt();
                }
            }
        }
        mConfig = config;
    }

    public void setGLManager(GLManager glManager) {
        mGlManager = glManager;
    }

    public CameraSize getPreviewSize() {
        return mCurrentPreviewSize;
    }

    public void onResume() {
        if (mCamera == null) {
            openCameraSync(); //  Open the camera and wait for it to open.
        }
    }

    public void onPause() {
        if (mCamera != null) {
            releaseCameraSync(); // Close camera and wait for it to close.
        }
    }

    public void onDestroy() {
        mCameraThread.mHandler.invalidateHandler(); // Get rid of weak-reference.
    }

    /**
     * Called from GLManager renderer thread when the surface texture is ready to receive frames.
     */
    protected void setSurfaceTexture(SurfaceTexture st) {
        mCameraThread.mHandler.sendMessage(mCameraThread.mHandler.obtainMessage(
                CameraHandler.MSG_SET_SURFACE_TEXTURE, st));
    }

    /**
     * Called from GLManager once GLManager.startPreview() is called.
     */
    protected void openCameraSync() {
        mCameraThread.mHandler.sendMessage(mCameraThread.mHandler.obtainMessage(
                CameraHandler.MSG_OPEN_CAMERA));
        //  Wait for camera to be opened.
        synchronized (mCameraLock) {
            while (mCamera == null) {
                try {
                    mCameraLock.wait();
                } catch (InterruptedException ignored) {
                    Thread.currentThread().interrupt();
                }
            }
        }
    }

    protected boolean isCameraOpen() {
        synchronized (mCameraLock) {
            return mCamera != null;
        }
    }

    /**
     * Close the camera during onPause().
     */
    protected void releaseCameraSync() {
        mCameraThread.mHandler.sendMessage(mCameraThread.mHandler.obtainMessage(
                CameraHandler.MSG_RELEASE_CAMERA));
        //  Wait for camera to be released.
        synchronized (mCameraLock) {
            while (mCamera != null) {
                try {
                    mCameraLock.wait();
                } catch (InterruptedException ignored) {
                    Thread.currentThread().interrupt();
                }
            }
        }
    }

    /**
     * Connects the SurfaceTexture to the Camera preview output, and starts the preview.
     * From now, onFrameAvailable() will be called.
     */
    private void handleSetSurfaceTexture(SurfaceTexture st) {
        st.setOnFrameAvailableListener(this);
        try {
            mCamera.setPreviewTexture(st);
        } catch (IOException ioe) {
            throw new RuntimeException(ioe);
        }
        mCamera.startPreview();
    }

    /**
     * Stop preview and release the camera. onFrameAvailable() will not be called again.
     */
    private void handleReleaseCamera() {
        if (mCamera != null) {
            mCamera.stopPreview();
            mCamera.release();
            mCamera = null;
        }
        synchronized (mCameraLock){
            mCameraLock.notify();
        }
    }

    private void handleOpenCamera() {
        if (mCamera != null) {
            throw new RuntimeException("Camera already initialized");
        }

        Camera.CameraInfo info = new Camera.CameraInfo();

        //  Try to find the right camera.
        int numCameras = Camera.getNumberOfCameras();
        for (int i = 0; i < numCameras; i++) {
            Camera.getCameraInfo(i, info);
            if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
                mCamera = Camera.open(i);
                break;
            }
        }

        if (mCamera == null) {
            throw new RuntimeException("Unable to open camera");
        }

        /* if(targetCameraType == Camera.CameraInfo.CAMERA_FACING_FRONT){
            mNewFilter = Filters.FILTER_MIRROR;
        }
        else{
            mNewFilter = Filters.FILTER_NONE;
        } */

        Camera.Parameters params = mCamera.getParameters();

        List<String> focusModes = params.getSupportedFocusModes();
        if (focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
            params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
        } else if (focusModes.contains(Camera.Parameters.FOCUS_MODE_AUTO)) {
            params.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);
        }

        List<String> supportedFlashModes = params.getSupportedFlashModes();
        if (supportedFlashModes != null
                && supportedFlashModes.contains(Camera.Parameters.FLASH_MODE_OFF)) {
            params.setFlashMode(Camera.Parameters.FLASH_MODE_OFF);
        }

        params.setRecordingHint(true);

        List<int[]> fpsRanges = params.getSupportedPreviewFpsRange();
        int[] maxFpsRange = null;
        // Get the maximum supported fps not to exceed 30
        for (int x = fpsRanges.size() - 1; x >= 0; x--) {
            maxFpsRange = fpsRanges.get(x);
            if (maxFpsRange[1] / 1000.0 <= 30) break;
        }
        if (maxFpsRange != null) {
            params.setPreviewFpsRange(maxFpsRange[0], maxFpsRange[1]);
        }

        CamcorderProfile profile;
        if (mConfig.getPreviewSize() == VideoConfig.VIDEO_SIZE.RES_480p
                && CamcorderProfile.hasProfile(mConfig.getCameraType(), CamcorderProfile.QUALITY_480P)) {
            profile = CamcorderProfile.get(mConfig.getCameraType(), CamcorderProfile.QUALITY_480P);
        } else if (CamcorderProfile.hasProfile(mConfig.getCameraType(), CamcorderProfile.QUALITY_720P)) {
            profile = CamcorderProfile.get(mConfig.getCameraType(), CamcorderProfile.QUALITY_720P);
        } else {
            profile = CamcorderProfile.get(mConfig.getCameraType(), CamcorderProfile.QUALITY_HIGH);
        }

        CameraUtils.choosePreviewSize(params, profile.videoFrameWidth, profile.videoFrameHeight);

        mCurrentPreviewSize.width = params.getPreviewSize().width;
        mCurrentPreviewSize.height = params.getPreviewSize().height;

        // Front camera fix for devices such as the Nexus 6
        try {
            if (mConfig.getCameraType() == Camera.CameraInfo.CAMERA_FACING_FRONT) {
                if (info.orientation == 90) {
                    mCamera.setDisplayOrientation(180);
                }
            } else if (mConfig.getCameraType() == Camera.CameraInfo.CAMERA_FACING_BACK) {
                if (info.orientation == 270) {
                    mCamera.setDisplayOrientation(180);
                }
            }
        } catch (Exception ignored) {
        }

        // leave the frame rate set to default
        mCamera.setParameters(params);

        int[] fpsRange = new int[2];
        Camera.Size mCameraPreviewSize = params.getPreviewSize();
        params.getPreviewFpsRange(fpsRange);
        String previewFacts = mCameraPreviewSize.width + "x" + mCameraPreviewSize.height;
        if (fpsRange[0] == fpsRange[1]) {
            previewFacts += " @" + (fpsRange[0] / 1000.0) + "fps";
        } else {
            previewFacts += " @" + (fpsRange[0] / 1000.0) + " - " + (fpsRange[1] / 1000.0) + "fps";
        }
        if (VERBOSE) Log.i(TAG, "Camera preview set: " + previewFacts);
        synchronized (mCameraLock) {
            mCameraLock.notifyAll();
        }
    }

    /**
     * New image frame is available from the camera. Called on an "arbitrary" thread.
     * This runs after you attach the listener to surface texture.
     */
    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture) {
        //  Tell the display screen to render this new frame.
        mGlManager.requestRender();
    }

    /**
     * Handles camera operation requests from other threads.  Necessary because the Camera
     * must only be accessed from one thread.
     */
    static class CameraHandler extends Handler {
        public static final int MSG_SET_SURFACE_TEXTURE = 0;
        public static final int MSG_RELEASE_CAMERA = 1;
        public static final int MSG_OPEN_CAMERA = 2;

        public static final String TAG = "CameraHandler";

        // Weak reference to the Activity; only access this from the UI thread.
        private WeakReference<CameraManager> mManager;

        public CameraHandler(CameraManager manager) {
            mManager = new WeakReference<>(manager);
        }

        /**
         * Drop the reference to the activity.  Useful as a paranoid measure to ensure that
         * attempts to access a stale Activity through a handler are caught.
         */
        public void invalidateHandler() {
            mManager.clear();
        }

        @Override
        public void handleMessage(Message inputMessage) {
            int what = inputMessage.what;
            Log.d(TAG, "CameraHandler [" + this + "]: what=" + what);

            CameraManager manager = mManager.get();
            if (manager == null) {
                Log.e(TAG, "CameraHandler.handleMessage: activity is null");
                return;
            }

            switch (what) {
                case MSG_SET_SURFACE_TEXTURE:
                    manager.handleSetSurfaceTexture((SurfaceTexture) inputMessage.obj);
                    break;
                case MSG_RELEASE_CAMERA:
                    manager.handleReleaseCamera();
                case MSG_OPEN_CAMERA:
                    manager.handleOpenCamera();
            }
        }
    }

    class CameraThread extends Thread {
        private CameraHandler mHandler = null;
        private CameraManager mCamManager;
        private final Object mReadyLock = new Object();


        CameraThread(CameraManager cameraManager) {
            mCamManager = cameraManager;
            setName("CameraThread");
        }

        public void run() {
            Looper.prepare();
            synchronized (mReadyLock) {
                mHandler = new CameraHandler(mCamManager);
                mReadyLock.notify();
            }
            Looper.loop();
        }
    }

    public class CameraSize {
        public int width = -1;
        public int height = -1;
    }
}
