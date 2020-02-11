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

import java.util.ArrayList;
import java.util.List;

import android.Manifest;
import android.app.Activity;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.Size;
import android.hardware.Camera;
import android.util.Log;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.WebRTCJNITarget;
import org.mozilla.gecko.permissions.Permissions;

public class VideoCaptureDeviceInfoAndroid {
  private final static String TAG = "WEBRTC-JC";

  private static boolean isFrontFacing(CameraInfo info) {
    return info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT;
  }

  private static String deviceUniqueName(int index, CameraInfo info) {
    return "Camera " + index +", Facing " +
        (isFrontFacing(info) ? "front" : "back") +
        ", Orientation "+ info.orientation;
  }

  @WebRTCJNITarget
  public static List<int[]> getFpsRangesRobust(Camera.Parameters parameters) {
      List<int[]> supportedFpsRanges = null;
      if (android.os.Build.VERSION.SDK_INT >= 9) {
          supportedFpsRanges = parameters.getSupportedPreviewFpsRange();
      }
      // getSupportedPreviewFpsRange doesn't actually work on a bunch
      // of Gingerbread devices.
      if (supportedFpsRanges == null) {
          supportedFpsRanges = new ArrayList<int[]>();
          List<Integer> frameRates = parameters.getSupportedPreviewFrameRates();
          if (frameRates != null) {
              for (Integer rate: frameRates) {
                  int[] range = new int[2];
                  // minFPS = maxFPS, convert to milliFPS
                  range[0] = rate * 1000;
                  range[1] = rate * 1000;
                  supportedFpsRanges.add(range);
              }
          } else {
              Log.e(TAG, "Camera doesn't know its own framerate, guessing 30fps.");
              int[] range = new int[2];
              // Your guess is as good as mine
              range[0] = 30 * 1000;
              range[1] = 30 * 1000;
              supportedFpsRanges.add(range);
          }
      }
      return supportedFpsRanges;
  }

  // Returns information about all cameras on the device.
  // Since this reflects static information about the hardware present, there is
  // no need to call this function more than once in a single process.  It is
  // marked "private" as it is only called by native code.
  @WebRTCJNITarget
  private static CaptureCapabilityAndroid[] getDeviceInfo() {
      final boolean hasPermissions = Permissions.has(
              GeckoAppShell.getApplicationContext(), Manifest.permission.CAMERA);

      if (hasPermissions) {
          return createDeviceList();
      } else {
          return new CaptureCapabilityAndroid[0];
      }
  }

  private static CaptureCapabilityAndroid[] createDeviceList() {
      ArrayList<CaptureCapabilityAndroid> allDevices = new ArrayList<CaptureCapabilityAndroid>();
      int numCameras = 1;
      if (android.os.Build.VERSION.SDK_INT >= 9) {
          numCameras = Camera.getNumberOfCameras();
      }
      for (int i = 0; i < numCameras; ++i) {
          String uniqueName = null;
          CameraInfo info = null;
          if (android.os.Build.VERSION.SDK_INT >= 9) {
              info = new CameraInfo();
              Camera.getCameraInfo(i, info);
              uniqueName = deviceUniqueName(i, info);
          } else {
              uniqueName = "Camera 0, Facing back, Orientation 90";
          }

          List<Size> supportedSizes = null;
          List<int[]> supportedFpsRanges = null;
          try {
              Camera camera = null;
              if (android.os.Build.VERSION.SDK_INT >= 9) {
                  camera = Camera.open(i);
              } else {
                  camera = Camera.open();
              }
              Parameters parameters = camera.getParameters();
              supportedSizes = parameters.getSupportedPreviewSizes();
              supportedFpsRanges = getFpsRangesRobust(parameters);
              camera.release();
              Log.d(TAG, uniqueName);
          } catch (RuntimeException e) {
              Log.e(TAG, "Failed to open " + uniqueName + ", skipping due to: "
                    + e.getLocalizedMessage());
              continue;
          }

          boolean is30fpsRange = false;
          boolean is15fpsRange = false;
          // If there is constant 30 fps mode, but no 15 fps - add 15 fps
          // mode to the list of supported ranges. Frame drop will be done
          // in software.
          for (int[] range : supportedFpsRanges) {
              if (range[0] == 30000 &&
                  range[1] == 30000) {
                  is30fpsRange = true;
              }
              if (range[0] == 15000 &&
                  range[1] == 15000) {
                  is15fpsRange = true;
              }
          }
          if (is30fpsRange && !is15fpsRange) {
              Log.d(TAG, "Adding 15 fps support");
              int[] range = new int[2];
              range[0] = 15 * 1000;
              range[1] = 15 * 1000;
              supportedFpsRanges.add(range);
          }

          CaptureCapabilityAndroid device = new CaptureCapabilityAndroid();

          int sizeLen = supportedSizes.size();
          device.width  = new int[sizeLen];
          device.height = new int[sizeLen];

          int j = 0;
          for (Size size : supportedSizes) {
                device.width[j] = size.width;
                device.height[j] = size.height;
                j++;
          }

          // Android SDK deals in integral "milliframes per second"
          // (i.e. fps*1000, instead of floating-point frames-per-second) so we
          // preserve that through the Java->C++->Java round-trip.
          int[] mfps = supportedFpsRanges.get(supportedFpsRanges.size() - 1);
          device.name = uniqueName;
          if (android.os.Build.VERSION.SDK_INT >= 9) {
              device.frontFacing = isFrontFacing(info);
              device.orientation = info.orientation;
              device.minMilliFPS = mfps[Parameters.PREVIEW_FPS_MIN_INDEX];
              device.maxMilliFPS = mfps[Parameters.PREVIEW_FPS_MAX_INDEX];
          } else {
              device.frontFacing = false;
              device.orientation = 90;
              device.minMilliFPS = mfps[0];
              device.maxMilliFPS = mfps[1];
          }
          allDevices.add(device);
      }
      return allDevices.toArray(new CaptureCapabilityAndroid[0]);
  }
}
