package com.infinitetakes.stream.videoSDK.Video;

import android.hardware.Camera;
import android.util.Log;

import com.infinitetakes.stream.videoSDK.Video.gles.Texture2dProgram;

import java.util.List;

/**
 * Camera-related utility functions.
 */
public class CameraUtils {
    private static final String TAG = "CameraUtils";

    /**
     * Attempts to find a preview size that matches the provided width and height (which
     * specify the dimensions of the encoded video).  If it fails to find a match it just
     * uses the default preview size for video.
     * <p>
     * TODO: should do a best-fit match, e.g.
     * https://github.com/commonsguy/cwac-camera/blob/master/camera/src/com/commonsware/cwac/camera/CameraUtils.java
     */
    public static void choosePreviewSize(Camera.Parameters parms, int width, int height) {
        // We should make sure that the requested MPEG size is less than the preferred
        // size, and has the same aspect ratio.
        Camera.Size ppsfv = parms.getPreferredPreviewSizeForVideo();
        if (ppsfv != null) {
            Log.d(TAG, "Camera preferred preview size for video is " +
                    ppsfv.width + "x" + ppsfv.height);
        }

        //for (Camera.Size size : parms.getSupportedPreviewSizes()) {
        //    Log.d(TAG, "supported: " + size.width + "x" + size.height);
        //}

        for (Camera.Size size : parms.getSupportedPreviewSizes()) {
            if (size.width == width && size.height == height) {
                parms.setPreviewSize(width, height);
                return;
            }
        }

        Log.w(TAG, "Unable to set preview size to " + width + "x" + height);
        if (ppsfv != null) {
            parms.setPreviewSize(ppsfv.width, ppsfv.height);
        }
        // else use whatever the default size is
    }

    /**
     * Attempts to find a fixed preview frame rate that matches the desired frame rate.
     * <p>
     * It doesn't seem like there's a great deal of flexibility here.
     * <p>
     * TODO: follow the recipe from http://stackoverflow.com/questions/22639336/#22645327
     *
     * @return The expected frame rate, in thousands of frames per second.
     */
    public static int chooseFixedPreviewFps(Camera.Parameters parms, int desiredThousandFps) {
        List<int[]> supported = parms.getSupportedPreviewFpsRange();

        for (int[] entry : supported) {
            //Log.d(TAG, "entry: " + entry[0] + " - " + entry[1]);
            if ((entry[0] == entry[1]) && (entry[0] == desiredThousandFps)) {
                parms.setPreviewFpsRange(entry[0], entry[1]);
                return entry[0];
            }
        }

        int[] tmp = new int[2];
        parms.getPreviewFpsRange(tmp);
        int guess;
        if (tmp[0] == tmp[1]) {
            guess = tmp[0];
        } else {
            guess = tmp[1] / 2;     // shrug
        }

        Log.d(TAG, "Couldn't find match for " + desiredThousandFps + ", using " + guess);
        return guess;
    }

    /**
     * This is a utility class that gives the appropriate program, kernel, and color from the "filter".
     * Filters are just Textures that are applied with a specific program.
     */
    public static class Filter {
        // Camera filters; must match up with cameraFilterNames in strings.xml
        static final int FILTER_NONE = 0;
        static final int FILTER_BLACK_WHITE = 1;
        static final int FILTER_BLUR = 2;
        static final int FILTER_SHARPEN = 3;
        static final int FILTER_EDGE_DETECT = 4;
        static final int FILTER_EMBOSS = 5;

        public Texture2dProgram.ProgramType programType;
        public float[] kernel = null;
        public float colorAdj = 0.0f;

        public Filter(int newFilter) {
            switch (newFilter) {
                case FILTER_NONE:
                    programType = Texture2dProgram.ProgramType.TEXTURE_EXT;
                    break;
                case FILTER_BLACK_WHITE:
                    // (In a previous version the TEXTURE_EXT_BW variant was enabled by a flag called
                    // ROSE_COLORED_GLASSES, because the shader set the red channel to the B&W color
                    // and green/blue to zero.)
                    programType = Texture2dProgram.ProgramType.TEXTURE_EXT_BW;
                    break;
                case FILTER_BLUR:
                    programType = Texture2dProgram.ProgramType.TEXTURE_EXT_FILT;
                    kernel = new float[]{
                            1f / 16f, 2f / 16f, 1f / 16f,
                            2f / 16f, 4f / 16f, 2f / 16f,
                            1f / 16f, 2f / 16f, 1f / 16f};
                    break;
                case FILTER_SHARPEN:
                    programType = Texture2dProgram.ProgramType.TEXTURE_EXT_FILT;
                    kernel = new float[]{
                            0f, -1f, 0f,
                            -1f, 5f, -1f,
                            0f, -1f, 0f};
                    break;
                case FILTER_EDGE_DETECT:
                    programType = Texture2dProgram.ProgramType.TEXTURE_EXT_FILT;
                    kernel = new float[]{
                            -1f, -1f, -1f,
                            -1f, 8f, -1f,
                            -1f, -1f, -1f};
                    break;
                case FILTER_EMBOSS:
                    programType = Texture2dProgram.ProgramType.TEXTURE_EXT_FILT;
                    kernel = new float[]{
                            2f, 0f, 0f,
                            0f, -1f, 0f,
                            0f, 0f, -1f};
                    colorAdj = 0.5f;
                    break;
                default:
                    throw new RuntimeException("Unknown filter mode " + newFilter);
            }
        }
    }
}
