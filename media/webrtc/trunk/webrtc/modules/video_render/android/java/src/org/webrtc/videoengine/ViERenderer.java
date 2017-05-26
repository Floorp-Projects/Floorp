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
        ThreadUtils.getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                try {
                } catch (Exception e) {
                    Log.e(TAG, "enableOrientationListener exception: "
                          + e.getLocalizedMessage());
                }
            }
        });
    }

    public static void DestroyLocalRenderer() {
        ThreadUtils.getUiHandler().post(new Runnable() {
            @Override
            public void run() {
                try {
                } catch (Exception e) {
                    Log.e(TAG,
                          "disableOrientationListener exception: " +
                          e.getLocalizedMessage());
                }
            }
        });
    }
}
