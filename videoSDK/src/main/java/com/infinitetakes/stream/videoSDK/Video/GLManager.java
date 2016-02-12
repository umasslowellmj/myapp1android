package com.infinitetakes.stream.videoSDK.Video;

import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.util.Log;

import com.infinitetakes.stream.videoSDK.Video.gles.FullFrameRect;
import com.infinitetakes.stream.videoSDK.Video.gles.Texture2dProgram;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;


/**
 * All OpenGL related stuff has to go through this class, to keep things clean.
 * Here is how everything works:
 * * Construct a GLManager().
 * * Register onPause/onResume in activity lifecycle.
 * * setCameraManager().
 * * setEncoderManager().
 * * setSurfaceView().
 * * startPreview().
 * * Call onPause() if you want to pause the rendering thread.
 * <p/>
 * - No need for OnDestroy since we kill everything in onPause anyway.
 * - Everything inside SurfaceRenderer will happen with the GLThread (not UI thread), so be warned!
 * - All calls for GLManager can be done on any thread (we delegate to appropriate one).
 */
public class GLManager {
    private static final String TAG = "GLManager";

    private CameraManager mCameraManager; //  Camera operations run on another thread.
    private EncoderManager mEncoderManager; //  Encoding operations run on another thread.
    private GLSurfaceView mGLView; //  SurfaceView to render things for display.
    private SurfaceRenderer mRenderer; // Renderer class for gl surface callbacks.

    private int mCurrentFilter;
    private int mNewFilter;

    public boolean mEncoderContextNeedsUpdate = true;
    public boolean mIncomingSizeUpdated = false;

    public void setCameraManager(CameraManager cameraManager) {
        mCameraManager = cameraManager;
    }

    public void setEncoderManager(EncoderManager encoderManager) {
        mEncoderManager = encoderManager;
    }

    public void setSurfaceView(GLSurfaceView surfaceView) {
        mGLView = surfaceView;
    }

    public void setFilter(int filter) {
        mNewFilter = filter;
    }

    public void onResume() {
        //  In case onResume is called before initialization setters.
        if (mRenderer != null) {
            //  Resume the render thread
            mGLView.onResume();
            mIncomingSizeUpdated = true;
        }
    }

    public void onPause() {
        //  Stop camera preview and wait for it to stop.
        mCameraManager.releaseCameraSync();
        if (mRenderer != null) {
            //  Tell renderer thread to kill the surface and textures
            mGLView.queueEvent(new Runnable() {
                @Override
                public void run() {
                    mRenderer.onPause();
                }
            });
            //  Pause the renderer thread.
            mGLView.onPause();
        }
    }

    public void onDestroy(){

    }


    /**
     * Configure GLES version 2.0, attach the renderer to it, and start the render thread.
     */
    public void startPreview() {
        //  Initialization needs to be done first.
        if (mGLView != null && mCameraManager != null && mEncoderManager != null) {
            //  Need to open camera because we attach the surface texture to camera.
            if(!mCameraManager.isCameraOpen()){
                mCameraManager.openCameraSync();
            }
            mGLView.setEGLContextClientVersion(2);
            mRenderer = new SurfaceRenderer();
            mGLView.setRenderer(mRenderer);
            mGLView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        } else {
            Log.e(TAG, "Make sure to initialize stuff first!");
        }
    }

    /**
     * No need for handler here, because requestRender can be run from any thread.
     */
    public void requestRender() {
        mGLView.requestRender();
    }

    class SurfaceRenderer implements GLSurfaceView.Renderer {
        private FullFrameRect mFullScreen;
        private int mTextureId;
        private SurfaceTexture mSurfaceTexture;
        private final float[] mSTMatrix = new float[16];

        @Override
        public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {

        }

        //  Called when surface changes size. This shouldn't happen.
        @Override
        public void onSurfaceChanged(GL10 gl10, int w, int h) {
            // We're starting up or coming back.  Either way we've got a new EGLContext that will
            // need to be shared with the encoder if its already in progress.
            if (mEncoderManager.isRecording()) {
                mEncoderManager.updateGLContext(EGL14.eglGetCurrentContext());
                mEncoderContextNeedsUpdate = false;
            }

            //  Setup the texture program for only the display (not the recording).
            mFullScreen = new FullFrameRect(
                    new Texture2dProgram(Texture2dProgram.ProgramType.TEXTURE_EXT));
            mTextureId = mFullScreen.createTextureObject();
            mEncoderManager.setTextureID(mTextureId);

            // Create a SurfaceTexture, with an external texture, in this EGL context.  We don't
            // have a Looper in this thread -- GLSurfaceView doesn't create one -- so the frame
            // available messages will arrive on the main thread.
            mSurfaceTexture = new SurfaceTexture(mTextureId);

            //  Tell camera thread the texture is ready to receive frames.
            mCameraManager.setSurfaceTexture(mSurfaceTexture);
        }

        @Override
        public void onDrawFrame(GL10 gl10) {
            GLES20.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            // Update display texture with the latest image from SurfaceTexture from camera.
            mSurfaceTexture.updateTexImage();
            GLES20.glFinish();

            //  Update encoder with latest EGLContext if necessary
            if (mEncoderContextNeedsUpdate) {
                mEncoderManager.updateGLContext(EGL14.eglGetCurrentContext());
                mEncoderContextNeedsUpdate = false;
            }
            CameraManager.CameraSize previewSize = mCameraManager.getPreviewSize();
            if (previewSize.width <= 0) {
                Log.i(TAG, "Camera doesn't have a preview size yet. Skipping.");
                return;
            }

            // Update the filter, if necessary.
            /*if (mCurrentFilter != mNewFilter) {
                CameraUtils.Filter filter = new CameraUtils.Filter(mNewFilter);
                //  Check if we really need a whole new program
                if (filter.programType != mFullScreen.getProgram().getProgramType()) {
                    mFullScreen.changeProgram(new Texture2dProgram(filter.programType));
                    // If we created a new program, we need to re-initialize the texture size
                    mIncomingSizeUpdated = true;
                }
                // Update the filter kernel (if any).
                if (filter.kernel != null) {
                    mFullScreen.getProgram().setKernel(filter.kernel, filter.colorAdj);
                }
                mCurrentFilter = mNewFilter;
            }*/

            //  Set the texture size to camera preview size
            if (mIncomingSizeUpdated) {
                mFullScreen.getProgram().setTexSize(previewSize.width, previewSize.height);
                mIncomingSizeUpdated = false;
            }

            // Draw the video frame.
            mSurfaceTexture.getTransformMatrix(mSTMatrix);
            mFullScreen.drawFrame(mTextureId, mSTMatrix);

            // Tell the video encoder thread that a new frame is available.
            // This will be ignored if we're not actually recording.
            mEncoderManager.frameAvailable(mSurfaceTexture);

        }

        public void onPause() {
            if (mSurfaceTexture != null) {
                mSurfaceTexture.release();
                mSurfaceTexture = null;
            }
            // Assume the GLSurfaceView EGL context is about to be destroyed
            if (mFullScreen != null) {
                mFullScreen.release(false);
                mFullScreen = null;
            }
        }
    }
}
