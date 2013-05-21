/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.videoengine;

import java.io.IOException;
import java.util.Locale;
import java.util.concurrent.locks.ReentrantLock;

import org.webrtc.videoengine.CaptureCapabilityAndroid;
import org.webrtc.videoengine.VideoCaptureDeviceInfoAndroid.AndroidVideoCaptureDevice;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.ThreadUtils;

public class VideoCaptureAndroid implements PreviewCallback, Callback {

    private final static String TAG = "WEBRTC-JC";

    private Camera camera;
    private int cameraId;
    private AndroidVideoCaptureDevice currentDevice = null;
    public ReentrantLock previewBufferLock = new ReentrantLock();
    // This lock takes sync with StartCapture and SurfaceChanged
    private ReentrantLock captureLock = new ReentrantLock();
    private int PIXEL_FORMAT = ImageFormat.NV21;
    PixelFormat pixelFormat = new PixelFormat();
    // True when the C++ layer has ordered the camera to be started.
    private boolean isCaptureStarted = false;
    private boolean isCaptureRunning = false;
    private boolean isSurfaceReady = false;
    private SurfaceHolder surfaceHolder = null;

    private final int numCaptureBuffers = 3;
    private int expectedFrameSize = 0;
    private int orientation = 0;
    private int id = 0;
    // C++ callback context variable.
    private long context = 0;
    private SurfaceHolder localPreview = null;
    // True if this class owns the preview video buffers.
    private boolean ownsBuffers = false;

    private int mCaptureWidth = -1;
    private int mCaptureHeight = -1;
    private int mCaptureFPS = -1;

    public static
    void DeleteVideoCaptureAndroid(VideoCaptureAndroid captureAndroid) {
        Log.d(TAG, "DeleteVideoCaptureAndroid");

        captureAndroid.StopCapture();
        captureAndroid.camera.release();
        captureAndroid.camera = null;
        captureAndroid.context = 0;

        GeckoApp.mAppContext.cameraView.getHolder().
            removeCallback(captureAndroid);
        ThreadUtils.getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                try {
                    GeckoApp.mAppContext.disableCameraView();
                } catch (Exception e) {
                    Log.e(TAG,
                          "VideoCaptureAndroid disableCameraView exception: " +
                          e.getLocalizedMessage());
                }
           }
        });
    }

    public VideoCaptureAndroid(int in_id, long in_context, Camera in_camera,
                               AndroidVideoCaptureDevice in_device,
                               int in_cameraId) {
        id = in_id;
        context = in_context;
        camera = in_camera;
        cameraId = in_cameraId;
        currentDevice = in_device;

        try {
            GeckoApp.mAppContext.cameraView.getHolder().addCallback(this);
            ThreadUtils.getUiHandler().post(new Runnable() {
                @Override
                public void run() {
                    try {
                        GeckoApp.mAppContext.enableCameraView();
                    } catch (Exception e) {
                        Log.e(TAG, 
                              "VideoCaptureAndroid enableCameraView exception: "
                               + e.getLocalizedMessage());
                    }
                }
            });
	} catch (Exception ex) {
	  Log.e(TAG, "VideoCaptureAndroid constructor exception: " +
                ex.getLocalizedMessage());
	}
    }

    public int GetRotateAmount() {
        android.hardware.Camera.CameraInfo info =
            new android.hardware.Camera.CameraInfo();
        android.hardware.Camera.getCameraInfo(cameraId, info);
        int rotation = GeckoApp.mAppContext.getWindowManager().getDefaultDisplay().getRotation();
        int degrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0: degrees = 0; break;
            case Surface.ROTATION_90: degrees = 90; break;
            case Surface.ROTATION_180: degrees = 180; break;
            case Surface.ROTATION_270: degrees = 270; break;
        }

        int result;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            result = (info.orientation + degrees) % 360;
        } else {  // back-facing
            result = (info.orientation - degrees + 360) % 360;
        }

        return result;
    }

    private int tryStartCapture(int width, int height, int frameRate) {
        if (camera == null) {
            Log.e(TAG, "Camera not initialized %d" + id);
            return -1;
        }

        Log.d(TAG, "tryStartCapture " + width +
                " height " + height +" frame rate " + frameRate +
                " isCaptureRunning " + isCaptureRunning +
                " isSurfaceReady " + isSurfaceReady +
                " isCaptureStarted " + isCaptureStarted);

        if (isCaptureRunning || !isSurfaceReady || !isCaptureStarted) {
            return 0;
        }

        try {
            camera.setPreviewDisplay(surfaceHolder);

            CaptureCapabilityAndroid currentCapability =
                    new CaptureCapabilityAndroid();
            currentCapability.width = width;
            currentCapability.height = height;
            currentCapability.maxFPS = frameRate;
            PixelFormat.getPixelFormatInfo(PIXEL_FORMAT, pixelFormat);

            Camera.Parameters parameters = camera.getParameters();
            parameters.setPreviewSize(currentCapability.width,
                    currentCapability.height);
            parameters.setPreviewFormat(PIXEL_FORMAT);
            parameters.setPreviewFrameRate(currentCapability.maxFPS);
            camera.setParameters(parameters);

            int bufSize = width * height * pixelFormat.bitsPerPixel / 8;
            byte[] buffer = null;
            for (int i = 0; i < numCaptureBuffers; i++) {
                buffer = new byte[bufSize];
                camera.addCallbackBuffer(buffer);
            }
            camera.setPreviewCallbackWithBuffer(this);
            ownsBuffers = true;

            camera.startPreview();
            previewBufferLock.lock();
            expectedFrameSize = bufSize;
            isCaptureRunning = true;
            previewBufferLock.unlock();

        }
        catch (Exception ex) {
            Log.e(TAG, "Failed to start camera: " + ex.getMessage());
            return -1;
        }

        isCaptureRunning = true;
        return 0;
    }

    public int StartCapture(int width, int height, int frameRate) {
        Log.d(TAG, "StartCapture width " + width +
                " height " + height +" frame rate " + frameRate);
        captureLock.lock();
        isCaptureStarted = true;
        mCaptureWidth = width;
        mCaptureHeight = height;
        mCaptureFPS = frameRate;

        int res = tryStartCapture(mCaptureWidth, mCaptureHeight, mCaptureFPS);

        captureLock.unlock();
        return res;
    }

    public int StopCapture() {
        Log.d(TAG, "StopCapture");
        try {
            previewBufferLock.lock();
            isCaptureRunning = false;
            previewBufferLock.unlock();
            camera.stopPreview();
            camera.setPreviewCallbackWithBuffer(null);
        }
        catch (Exception ex) {
            Log.e(TAG, "Failed to stop camera");
            return -1;
        }

        isCaptureStarted = false;
        return 0;
    }

    native void ProvideCameraFrame(byte[] data, int length, long captureObject);

    public void onPreviewFrame(byte[] data, Camera camera) {
        previewBufferLock.lock();

        // The following line is for debug only
        Log.v(TAG, "preview frame length " + data.length +
              " context" + context);
        if (isCaptureRunning) {
            // If StartCapture has been called but not StopCapture
            // Call the C++ layer with the captured frame
            if (data.length == expectedFrameSize) {
                ProvideCameraFrame(data, expectedFrameSize, context);
                if (ownsBuffers) {
                    // Give the video buffer to the camera service again.
                    camera.addCallbackBuffer(data);
                }
            }
        }
        previewBufferLock.unlock();
    }

    // Sets the rotation of the preview render window.
    // Does not affect the captured video image.
    public void SetPreviewRotation(int rotation) {
        Log.v(TAG, "SetPreviewRotation: " + rotation);

        if (camera != null) {
            previewBufferLock.lock();
            int width = 0;
            int height = 0;
            int framerate = 0;
            boolean wasCaptureRunning = isCaptureRunning;

            if (isCaptureRunning) {
                width = mCaptureWidth;
                height = mCaptureHeight;
                framerate = mCaptureFPS;
                StopCapture();
            }

            int resultRotation = 0;
            if (currentDevice.frontCameraType ==
                    VideoCaptureDeviceInfoAndroid.FrontFacingCameraType.Android23) {
                // this is a 2.3 or later front facing camera.
                // SetDisplayOrientation will flip the image horizontally
                // before doing the rotation.
                resultRotation=(360-rotation) % 360; // compensate the mirror
            }
            else {
                // Back facing or 2.2 or previous front camera
                resultRotation=rotation;
            }
            camera.setDisplayOrientation(resultRotation);

            if (wasCaptureRunning) {
                StartCapture(width, height, framerate);
            }
            previewBufferLock.unlock();
        }
    }

    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.d(TAG, "VideoCaptureAndroid::surfaceChanged");

        captureLock.lock();
        isSurfaceReady = true;
        surfaceHolder = holder;

        tryStartCapture(mCaptureWidth, mCaptureHeight, mCaptureFPS);
        captureLock.unlock();
        return;
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.d(TAG, "VideoCaptureAndroid::surfaceCreated");
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "VideoCaptureAndroid::surfaceDestroyed");
        isSurfaceReady = false;
    }
}
