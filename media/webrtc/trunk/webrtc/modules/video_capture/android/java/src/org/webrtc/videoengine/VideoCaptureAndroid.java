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
import java.util.List;
import java.util.concurrent.locks.ReentrantLock;

import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoAppShell.AppStateListener;
import org.mozilla.gecko.mozglue.WebRTCJNITarget;

// Wrapper for android Camera, with support for direct local preview rendering.
// Threading notes: this class is called from ViE C++ code, and from Camera &
// SurfaceHolder Java callbacks.  Since these calls happen on different threads,
// the entry points to this class are all synchronized.  This shouldn't present
// a performance bottleneck because only onPreviewFrame() is called more than
// once (and is called serially on a single thread), so the lock should be
// uncontended.
public class VideoCaptureAndroid implements PreviewCallback, Callback {
  private final static String TAG = "WEBRTC-JC";

  private Camera camera;  // Only non-null while capturing.
  private Camera.CameraInfo info = null;
  private final int id;
  private final long native_capturer;  // |VideoCaptureAndroid*| in C++.
  private SurfaceHolder localPreview;
  private SurfaceTexture dummySurfaceTexture;
  // Arbitrary queue depth.  Higher number means more memory allocated & held,
  // lower number means more sensitivity to processing time in the client (and
  // potentially stalling the capturer if it runs out of buffers to write to).
  private final int numCaptureBuffers = 3;
  // Needed to start/stop/rotate camera.
  private AppStateListener mAppStateListener = null;
  private int mCaptureRotation = 0;
  private int mCaptureWidth = 0;
  private int mCaptureHeight = 0;
  private int mCaptureMinFPS = 0;
  private int mCaptureMaxFPS = 0;
  // Are we being told to start/stop the camera, or just suspending/resuming
  // due to the application being backgrounded.
  private boolean mResumeCapture = false;

  @WebRTCJNITarget
  public VideoCaptureAndroid(int id, long native_capturer) {
    this.id = id;
    this.native_capturer = native_capturer;
    if(android.os.Build.VERSION.SDK_INT>8) {
      this.info = new Camera.CameraInfo();
      Camera.getCameraInfo(id, info);
    }
    mCaptureRotation = GetRotateAmount();
  }

  private void LinkAppStateListener() {
    mAppStateListener = new AppStateListener() {
      @Override
      public void onPause() {
        if (camera != null) {
          mResumeCapture = true;
          stopCapture();
        }
      }
      @Override
      public void onResume() {
        if (mResumeCapture) {
          startCapture(mCaptureWidth, mCaptureHeight, mCaptureMinFPS, mCaptureMaxFPS);
          mResumeCapture = false;
        }
      }
      @Override
      public void onOrientationChanged() {
        mCaptureRotation = GetRotateAmount();
      }
    };
    GeckoAppShell.getGeckoInterface().addAppStateListener(mAppStateListener);
  }

  private void RemoveAppStateListener() {
      GeckoAppShell.getGeckoInterface().removeAppStateListener(mAppStateListener);
  }

  public int GetRotateAmount() {
    int rotation = GeckoAppShell.getGeckoInterface().getActivity().getWindowManager().getDefaultDisplay().getRotation();
    int degrees = 0;
    switch (rotation) {
      case Surface.ROTATION_0: degrees = 0; break;
      case Surface.ROTATION_90: degrees = 90; break;
      case Surface.ROTATION_180: degrees = 180; break;
      case Surface.ROTATION_270: degrees = 270; break;
    }
    if(android.os.Build.VERSION.SDK_INT>8) {
      int result;
      if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
        result = (info.orientation + degrees) % 360;
      } else {  // back-facing
        result = (info.orientation - degrees + 360) % 360;
      }
      return result;
    } else {
      // Assume 90deg orientation for Froyo devices.
      // Only back-facing cameras are supported in Froyo.
      int orientation = 90;
      int result = (orientation - degrees + 360) % 360;
      return result;
    }
  }

  // Called by native code.  Returns true if capturer is started.
  //
  // Note that this actually opens the camera, which can be a slow operation and
  // thus might be done on a background thread, but ViE API needs a
  // synchronous success return value so we can't do that.
  @WebRTCJNITarget
  private synchronized boolean startCapture(
      int width, int height, int min_mfps, int max_mfps) {
    Log.d(TAG, "startCapture: " + width + "x" + height + "@" +
        min_mfps + ":" + max_mfps);
    if (!mResumeCapture) {
      ViERenderer.CreateLocalRenderer();
    }
    Throwable error = null;
    try {
      if(android.os.Build.VERSION.SDK_INT>8) {
        camera = Camera.open(id);
      } else {
        camera = Camera.open();
      }

      localPreview = ViERenderer.GetLocalRenderer();
      if (localPreview != null) {
        localPreview.addCallback(this);
        if (localPreview.getSurface() != null &&
            localPreview.getSurface().isValid()) {
          camera.setPreviewDisplay(localPreview);
        }
      } else {
        if(android.os.Build.VERSION.SDK_INT>10) {
          // No local renderer (we only care about onPreviewFrame() buffers, not a
          // directly-displayed UI element).  Camera won't capture without
          // setPreview{Texture,Display}, so we create a dummy SurfaceTexture and
          // hand it over to Camera, but never listen for frame-ready callbacks,
          // and never call updateTexImage on it.
          try {
            // "42" because http://goo.gl/KaEn8
            dummySurfaceTexture = new SurfaceTexture(42);
            camera.setPreviewTexture(dummySurfaceTexture);
          } catch (IOException e) {
            throw new RuntimeException(e);
          }
        } else {
          throw new RuntimeException("No preview surface for Camera.");
        }
      }

      Camera.Parameters parameters = camera.getParameters();
      // This wasn't added until ICS MR1.
      if(android.os.Build.VERSION.SDK_INT>14) {
        Log.d(TAG, "isVideoStabilizationSupported: " +
              parameters.isVideoStabilizationSupported());
        if (parameters.isVideoStabilizationSupported()) {
          parameters.setVideoStabilization(true);
        }
      }
      List<String> focusModeList = parameters.getSupportedFocusModes();
      if (focusModeList.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
        parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
      }
      parameters.setPreviewSize(width, height);
      if (android.os.Build.VERSION.SDK_INT>8) {
          parameters.setPreviewFpsRange(min_mfps, max_mfps);
      } else {
          parameters.setPreviewFrameRate(max_mfps / 1000);
      }
      int format = ImageFormat.NV21;
      parameters.setPreviewFormat(format);
      camera.setParameters(parameters);
      int bufSize = width * height * ImageFormat.getBitsPerPixel(format) / 8;
      for (int i = 0; i < numCaptureBuffers; i++) {
        camera.addCallbackBuffer(new byte[bufSize]);
      }
      camera.setPreviewCallbackWithBuffer(this);
      camera.startPreview();
      // Remember parameters we were started with.
      mCaptureWidth = width;
      mCaptureHeight = height;
      mCaptureMinFPS = min_mfps;
      mCaptureMaxFPS = max_mfps;
      // If we are resuming a paused capture, the listener is already active.
      if (!mResumeCapture) {
        LinkAppStateListener();
      }
      return true;
    } catch (IOException e) {
      error = e;
    } catch (RuntimeException e) {
      error = e;
    }
    Log.e(TAG, "startCapture failed", error);
    if (camera != null) {
      stopCapture();
    }
    return false;
  }

  // Called by native code.  Returns true when camera is known to be stopped.
  @WebRTCJNITarget
  private synchronized boolean stopCapture() {
    Log.d(TAG, "stopCapture");
    if (camera == null) {
      throw new RuntimeException("Camera is already stopped!");
    }
    Throwable error = null;
    try {
      camera.setPreviewCallbackWithBuffer(null);
      camera.stopPreview();
      if (localPreview != null) {
        localPreview.removeCallback(this);
        camera.setPreviewDisplay(null);
      } else {
        if(android.os.Build.VERSION.SDK_INT>10) {
          camera.setPreviewTexture(null);
        }
      }
      camera.release();
      camera = null;
      // If we want to resume after onResume, keep the listener in place.
      if (!mResumeCapture) {
        RemoveAppStateListener();
        ViERenderer.DestroyLocalRenderer();
      }
      return true;
    } catch (IOException e) {
      error = e;
    } catch (RuntimeException e) {
      error = e;
    }
    Log.e(TAG, "Failed to stop camera", error);
    return false;
  }

  @WebRTCJNITarget
  private native void ProvideCameraFrame(
    byte[] data, int length, long captureObject, int rotation);

  @WebRTCJNITarget
  public synchronized void onPreviewFrame(byte[] data, Camera camera) {
    if (data != null) {
      ProvideCameraFrame(data, data.length, native_capturer, mCaptureRotation);
      camera.addCallbackBuffer(data);
    }
  }

  @WebRTCJNITarget
  public synchronized void surfaceChanged(
      SurfaceHolder holder, int format, int width, int height) {
    Log.d(TAG, "VideoCaptureAndroid::surfaceChanged ignored: " +
        format + ": " + width + "x" + height);
  }

  @WebRTCJNITarget
  public synchronized void surfaceCreated(SurfaceHolder holder) {
    Log.d(TAG, "VideoCaptureAndroid::surfaceCreated");
    try {
      if (camera != null) {
        camera.setPreviewDisplay(holder);
      }
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  @WebRTCJNITarget
  public synchronized void surfaceDestroyed(SurfaceHolder holder) {
    Log.d(TAG, "VideoCaptureAndroid::surfaceDestroyed");
    try {
      if (camera != null) {
        camera.setPreviewDisplay(null);
      }
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }
}
