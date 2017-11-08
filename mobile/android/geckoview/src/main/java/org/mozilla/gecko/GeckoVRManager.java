/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoVRManager {
    /**
     * GeckoView applications implement this interface to provide GVR support for WebVR.
     */
    public interface GVRDelegate {
        /**
         * Creates non-presenting context. Will be invoked in the compositor thread.
         */
        long createNonPresentingContext();
        /**
         * Destroys non-presenting context. Will be invoked in the compositor thread.
         */
        void destroyNonPresentingContext();
        /**
         * Called when WebVR needs a presenting context. Will be invoked in the UI thread.
         */
        boolean enableVRMode();
        /**
         * Called when WebVR has finished presenting. Will be invoked in the UI thread.
         */
        void disableVRMode();
    }

    private static GVRDelegate mGVRDelegate;

    /**
     * Set the GVR Delegate for GeckoView.
     * @param delegate GVRDelegate instance or null to unset.
     */
    public static void setGVRDelegate(GVRDelegate delegate) {
        mGVRDelegate = delegate;
    }

    /**
     * Set the GVR paused state.
     * @param aPaused True if the application is being paused, False if the
     * application is resuming.
     */
    @WrapForJNI(calledFrom = "ui")
    public static native void setGVRPaused(final boolean aPaused);

    /**
     * Set the GVR presenting context.
     * @param aContext GVR context to use when WebVR starts to present. Pass in
     * zero to stop presenting.
     */
    @WrapForJNI(calledFrom = "ui")
    public static native void setGVRPresentingContext(final long aContext);

    /**
     * Inform WebVR that the non-presenting context needs to be destroyed.
     */
    @WrapForJNI(calledFrom = "ui")
    public static native void cleanupGVRNonPresentingContext();

    @WrapForJNI
    /* package */ static boolean isGVRPresent() {
        return mGVRDelegate != null;
    }

    @WrapForJNI
    /* package */ static long createGVRNonPresentingContext() {
        if (mGVRDelegate == null) {
            return 0;
        }
        return mGVRDelegate.createNonPresentingContext();
    }

    @WrapForJNI
    /* package */ static void destroyGVRNonPresentingContext() {
        if (mGVRDelegate == null) {
            return;
        }
        mGVRDelegate.destroyNonPresentingContext();
    }

    @WrapForJNI
    /* package */ static void enableVRMode() {
        if (!ThreadUtils.isOnUiThread()) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    enableVRMode();
                }
            });
            return;
        }

        if (mGVRDelegate == null) {
            return;
        }

        mGVRDelegate.enableVRMode();
    }

    @WrapForJNI
    /* package */ static void disableVRMode() {
        if (!ThreadUtils.isOnUiThread()) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    disableVRMode();
                }
            });
            return;
        }

        if (mGVRDelegate == null) {
            return;
        }

        mGVRDelegate.disableVRMode();
    }
}
