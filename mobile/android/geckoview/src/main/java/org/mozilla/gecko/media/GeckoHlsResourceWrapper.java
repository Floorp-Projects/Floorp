/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.util.Log;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

public class GeckoHlsResourceWrapper {
    private static final String LOGTAG = "GeckoHlsResourceWrapper";
    private static final boolean DEBUG = false;
    private GeckoHlsPlayer mPlayer = null;
    private boolean mDestroy = false;

    public static class HlsResourceCallbacks extends JNIObject
    implements GeckoHlsPlayer.ResourceCallbacks {
        @WrapForJNI(calledFrom = "gecko")
        HlsResourceCallbacks() {}

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onDataArrived();

        @Override
        @WrapForJNI(dispatchTo = "gecko")
        public native void onError(int errorCode);

        @Override // JNIObject
        protected void disposeNative() {
            throw new UnsupportedOperationException();
        }
    } // HlsResourceCallbacks

    private GeckoHlsResourceWrapper(String url,
                                    GeckoHlsPlayer.ResourceCallbacks callback) {
        if (DEBUG) Log.d(LOGTAG, "GeckoHlsResourceWrapper created with url = " + url);
        assertTrue(callback != null);

        mPlayer = new GeckoHlsPlayer();
        mPlayer.addResourceWrapperCallbackListener(callback);
        mPlayer.init(url);
    }

    @WrapForJNI(calledFrom = "gecko")
    public static GeckoHlsResourceWrapper create(String url,
                                                 GeckoHlsPlayer.ResourceCallbacks callback) {
        return new GeckoHlsResourceWrapper(url, callback);
    }

    @WrapForJNI(calledFrom = "gecko")
    public GeckoHlsPlayer GetPlayer() {
        // GeckoHlsResourceWrapper should always be created before others
        assertTrue(!mDestroy);
        assertTrue(mPlayer != null);
        return mPlayer;
    }

    private static void assertTrue(boolean condition) {
        if (DEBUG && !condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    @WrapForJNI // Called when native object is mDestroy.
    private void destroy() {
        if (DEBUG) Log.d(LOGTAG, "destroy!! Native object is destroyed.");
        if (mDestroy) {
            return;
        }
        mDestroy = true;
        if (mPlayer != null) {
            mPlayer.release();
            mPlayer = null;
        }
    }
}
