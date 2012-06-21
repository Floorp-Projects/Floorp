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
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;

public class VideoCaptureAndroid implements PreviewCallback, Callback {

    private Camera camera;
    private AndroidVideoCaptureDevice currentDevice = null;
    public ReentrantLock previewBufferLock = new ReentrantLock();
    private int PIXEL_FORMAT = ImageFormat.NV21;
    PixelFormat pixelFormat = new PixelFormat();
    // True when the C++ layer has ordered the camera to be started.
    private boolean isRunning=false;

    private final int numCaptureBuffers = 3;
    private int expectedFrameSize = 0;
    private int orientation = 0;
    private int id = 0;
    // C++ callback context variable.
    private long context = 0;
    private SurfaceHolder localPreview = null;
    // True if this class owns the preview video buffers.
    private boolean ownsBuffers = false;

    // Set this to 2 for VERBOSE logging. 1 for DEBUG
    private static int LOGLEVEL = 0;
    private static boolean VERBOSE = LOGLEVEL > 2;
    private static boolean DEBUG = LOGLEVEL > 1;

    CaptureCapabilityAndroid currentCapability = null;

    public static
    void DeleteVideoCaptureAndroid(VideoCaptureAndroid captureAndroid) {
        if(DEBUG) Log.d("*WEBRTC*", "DeleteVideoCaptureAndroid");

        captureAndroid.StopCapture();
        captureAndroid.camera.release();
        captureAndroid.camera = null;
        captureAndroid.context = 0;

        if(DEBUG) Log.v("*WEBRTC*", "DeleteVideoCaptureAndroid ended");

    }

    public VideoCaptureAndroid(int in_id,
            long in_context,
            Camera in_camera,
            AndroidVideoCaptureDevice in_device) {
        id = in_id;
        context = in_context;
        camera = in_camera;
        currentDevice = in_device;
    }

    public int StartCapture(int width, int height, int frameRate) {
        if(DEBUG) Log.d("*WEBRTC*", "StartCapture width" + width +
                " height " + height +" frame rate " + frameRate);
        try {
            if (camera == null) {
                Log.e("*WEBRTC*",
                        String.format(Locale.US,"Camera not initialized %d",id));
                return -1;
            }
            currentCapability = new CaptureCapabilityAndroid();
            currentCapability.width = width;
            currentCapability.height = height;
            currentCapability.maxFPS = frameRate;
            PixelFormat.getPixelFormatInfo(PIXEL_FORMAT, pixelFormat);

            Camera.Parameters parameters = camera.getParameters();
            parameters.setPreviewSize(currentCapability.width,
                    currentCapability.height);
            parameters.setPreviewFormat(PIXEL_FORMAT );
            parameters.setPreviewFrameRate(currentCapability.maxFPS);
            camera.setParameters(parameters);

            // Get the local preview SurfaceHolder from the static render class
            localPreview = ViERenderer.GetLocalRenderer();
            if(localPreview != null) {
                localPreview.addCallback(this);
            }

            int bufSize = width * height * pixelFormat.bitsPerPixel / 8;
            if(android.os.Build.VERSION.SDK_INT >= 7) {
                // According to Doc addCallbackBuffer belongs to API level 8.
                // But it seems like it works on Android 2.1 as well.
                // At least SE X10 and Milestone
                byte[] buffer = null;
                for (int i = 0; i < numCaptureBuffers; i++) {
                    buffer = new byte[bufSize];
                    camera.addCallbackBuffer(buffer);
                }

                camera.setPreviewCallbackWithBuffer(this);
                ownsBuffers = true;
            }
            else {
                camera.setPreviewCallback(this);
            }

            camera.startPreview();
            previewBufferLock.lock();
            expectedFrameSize = bufSize;
            isRunning = true;
            previewBufferLock.unlock();
        }
        catch (Exception ex) {
            Log.e("*WEBRTC*", "Failed to start camera");
            return -1;
        }
        return 0;
    }

    public int StopCapture() {
        if(DEBUG) Log.d("*WEBRTC*", "StopCapture");
        try {
            previewBufferLock.lock();
            isRunning = false;
            previewBufferLock.unlock();

            camera.stopPreview();

            if(android.os.Build.VERSION.SDK_INT > 7) {
                camera.setPreviewCallbackWithBuffer(null);
            }
            else {
                camera.setPreviewCallback(null);
            }
        }
        catch (Exception ex) {
            Log.e("*WEBRTC*", "Failed to stop camera");
            return -1;
        }

        if(DEBUG) {
            Log.d("*WEBRTC*", "StopCapture ended");
        }
        return 0;
    }

    native void ProvideCameraFrame(byte[] data,int length, long captureObject);

    public void onPreviewFrame(byte[] data, Camera camera) {
        previewBufferLock.lock();

        if(VERBOSE) {
            Log.v("*WEBRTC*",
                    String.format(Locale.US, "preview frame length %d context %x",
                            data.length, context));
        }
        if(isRunning) {
            // If StartCapture has been called but not StopCapture
            // Call the C++ layer with the captured frame
            if (data.length == expectedFrameSize) {
                ProvideCameraFrame(data, expectedFrameSize, context);
                if (VERBOSE) {
                    Log.v("*WEBRTC*", String.format(Locale.US, "frame delivered"));
                }
                if(ownsBuffers) {
                    // Give the video buffer to the camera service again.
                    camera.addCallbackBuffer(data);
                }
            }
        }
        previewBufferLock.unlock();
    }


    public void surfaceChanged(SurfaceHolder holder,
            int format, int width, int height) {

        try {
            if(camera != null) {
                camera.setPreviewDisplay(localPreview);
            }
        } catch (IOException e) {
            Log.e("*WEBRTC*",
                    String.format(Locale.US,
                            "Failed to set Local preview. " + e.getMessage()));
        }
    }

    // Sets the rotation of the preview render window.
    // Does not affect the captured video image.
    public void SetPreviewRotation(int rotation) {
        if(camera != null) {
            previewBufferLock.lock();
            final boolean running = isRunning;
            int width = 0;
            int height = 0;
            int framerate = 0;

            if(running) {
                width = currentCapability.width;
                height = currentCapability.height;
                framerate = currentCapability.maxFPS;

                StopCapture();
            }

            int resultRotation = 0;
            if(currentDevice.frontCameraType ==
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
            if(android.os.Build.VERSION.SDK_INT>7) {
                camera.setDisplayOrientation(resultRotation);
            }
            else {
                // Android 2.1 and previous
                // This rotation unfortunately does not seems to work.
                // http://code.google.com/p/android/issues/detail?id=1193
                Camera.Parameters parameters = camera.getParameters();
                parameters.setRotation(resultRotation);
                camera.setParameters(parameters);
            }

            if(running) {
                StartCapture(width, height, framerate);
            }
            previewBufferLock.unlock();
        }
    }

    public void surfaceCreated(SurfaceHolder holder) {
    }


    public void surfaceDestroyed(SurfaceHolder holder) {
    }

}
