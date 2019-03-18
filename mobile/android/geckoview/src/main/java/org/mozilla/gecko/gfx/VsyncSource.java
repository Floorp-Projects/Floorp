/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.RequiresApi;
import android.view.Choreographer;
import android.view.Display;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.GeckoAppShell;

/**
 * This class receives HW vsync events through a {@link Choreographer}.
 */
/* package */ final class VsyncSource implements Choreographer.FrameCallback {
    private static final String LOGTAG = "GeckoVsyncSource";

    @WrapForJNI
    private static final VsyncSource INSTANCE = new VsyncSource();

    /* package */ Choreographer mChoreographer;
    private volatile boolean mObservingVsync;

    private VsyncSource() {
        Handler mainHandler = new Handler(Looper.getMainLooper());
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                mChoreographer = Choreographer.getInstance();
                if (mObservingVsync) {
                    mChoreographer.postFrameCallback(VsyncSource.this);
                }
            }
        });
    }

    @WrapForJNI(stubName = "NotifyVsync")
    private static native void nativeNotifyVsync();

    // Choreographer callback implementation.
    public void doFrame(final long frameTimeNanos) {
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
    public synchronized boolean observeVsync(final boolean enable) {
        if (mObservingVsync != enable) {
            mObservingVsync = enable;

            if (mChoreographer != null) {
                if (enable) {
                    mChoreographer.postFrameCallback(this);
                } else {
                    mChoreographer.removeFrameCallback(this);
                }
            }
        }
        return mObservingVsync;
    }

    /**
     * Gets the refresh rate of default display in frames per second.
     *
     * The {@link DisplayManager} used by this method to determine the refresh rate
     * was introduced in API level 17.
     *
     * @return the refresh rate of default display in frames per second.
     **/
    @RequiresApi(api = Build.VERSION_CODES.JELLY_BEAN_MR1)
    @WrapForJNI
    public float getRefreshRate() {
        DisplayManager dm = (DisplayManager)
            GeckoAppShell.getApplicationContext().getSystemService(Context.DISPLAY_SERVICE);
        return dm.getDisplay(Display.DEFAULT_DISPLAY).getRefreshRate();
    }
}
