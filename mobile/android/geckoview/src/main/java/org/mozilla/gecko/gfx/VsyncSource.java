/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.HandlerThread;
import android.os.Process;
import android.util.Log;
import android.view.Choreographer;
import android.view.Display;
import java.util.concurrent.CountDownLatch;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.mozglue.JNIObject;

/**
 * This class receives HW vsync events through a {@link Choreographer}.
 */
public final class VsyncSource extends JNIObject implements Choreographer.FrameCallback {
    private static final String LOGTAG = "GeckoVsyncSource";

    // Vsync was introduced since JB (API 16~18) but inaccurate. Enable only for KK and later.
    public static final boolean SUPPORT_VSYNC =
        android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.KITKAT;

    private static VsyncSource sInstance;

    private Choreographer mChoreographer;
    private CountDownLatch mInitLatch = new CountDownLatch(1);

    private volatile boolean mObservingVsync;

    @WrapForJNI
    public static boolean isVsyncSupported() { return SUPPORT_VSYNC; }

    @WrapForJNI
    public static synchronized VsyncSource getInstance() {
        if (!SUPPORT_VSYNC) {
            Log.w(LOGTAG, "HW vsync avaiable only for KK and later.");
            return null;
        }
         if (sInstance == null) {
           sInstance = new VsyncSource();

            HandlerThread thread = new HandlerThread(LOGTAG, Process.THREAD_PRIORITY_DISPLAY) {
                protected void onLooperPrepared() {
                    sInstance.mChoreographer = Choreographer.getInstance();
                    sInstance.mInitLatch.countDown();
                }
            };
            thread.start();

            // Wait until the choreographer singleton is created in its thread.
            try {
                sInstance.mInitLatch.await();
            } catch (InterruptedException e) { /* do nothing */ }
        }

        return sInstance;
    }

    private VsyncSource() {}

    @WrapForJNI(stubName = "NotifyVsync")
    private static native void nativeNotifyVsync();

    @Override // JNIObject
    protected native void disposeNative();

    // Choreographer callback implementation.
    public void doFrame(long frameTimeNanos) {
        if (mObservingVsync) {
            mChoreographer.postFrameCallback(this);
            nativeNotifyVsync();
        }
    }

    /**
     * Start/stop observing Vsync event.
     * @param enable true to start observing; false to stop.
     * @return true if observing and false if not.
     */
    @WrapForJNI
    public synchronized boolean observeVsync(boolean enable) {
        if (mObservingVsync != enable) {
            mObservingVsync = enable;
            if (enable) {
                mChoreographer.postFrameCallback(this);
            } else {
                mChoreographer.removeFrameCallback(this);
            }
        }
        return mObservingVsync;
    }

    /** Gets the refresh rate of default display in frames per second. */
    @WrapForJNI
    public float getRefreshRate() {
        DisplayManager dm = (DisplayManager)
            GeckoAppShell.getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
        return dm.getDisplay(Display.DEFAULT_DISPLAY).getRefreshRate();
    }
}
