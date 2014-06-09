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

import android.content.Context;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.util.ThreadUtils;

public class ViERenderer {
    private final static String TAG = "WEBRTC-ViEREnderer";

    // View used for local rendering that Cameras can use for Video Overlay.
    private static SurfaceHolder g_localRenderer = null;

    public static SurfaceView CreateRenderer(Context context) {
        return CreateRenderer(context, false);
    }

    public static SurfaceView CreateRenderer(Context context,
            boolean useOpenGLES2) {
        if(useOpenGLES2 == true && ViEAndroidGLES20.IsSupported(context))
            return new ViEAndroidGLES20(context);
        else
            return new SurfaceView(context);
    }

    // Creates a SurfaceView to be used by Android Camera
    // service to display a local preview.
    // This needs to be used on Android prior to version 2.1
    // in order to run the camera.
    // Call this function before ViECapture::StartCapture.
    // The created view needs to be added to a visible layout
    // after a camera has been allocated
    // (with the call ViECapture::AllocateCaptureDevice).
    // IE.
    // CreateLocalRenderer
    // ViECapture::AllocateCaptureDevice
    // LinearLayout.addview
    // ViECapture::StartCapture
    public static void CreateLocalRenderer() {
        View cameraView = GeckoAppShell.getGeckoInterface().getCameraView();
        if (cameraView != null && (cameraView instanceof SurfaceView)) {
            SurfaceView localRender = (SurfaceView)cameraView;
            g_localRenderer = localRender.getHolder();
        }

        ThreadUtils.getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                try {
                    GeckoAppShell.getGeckoInterface().enableCameraView();
                } catch (Exception e) {
                    Log.e(TAG, "CreateLocalRenderer enableCameraView exception: "
                          + e.getLocalizedMessage());
                }
            }
        });
    }

    public static void DestroyLocalRenderer() {
        if (g_localRenderer != null) {
            g_localRenderer = null;

            ThreadUtils.getUiHandler().post(new Runnable() {
                @Override
                public void run() {
                    try {
                        GeckoAppShell.getGeckoInterface().disableCameraView();
                    } catch (Exception e) {
                        Log.e(TAG,
                              "DestroyLocalRenderer disableCameraView exception: " +
                              e.getLocalizedMessage());
                    }
                }
            });
        }
    }

    public static SurfaceHolder GetLocalRenderer() {
        return g_localRenderer;
    }

}
