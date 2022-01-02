/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.util.Log;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.geckoview.BuildConfig;

public class GeckoHLSResourceWrapper {
  private static final String LOGTAG = "GeckoHLSResourceWrapper";
  private static final boolean DEBUG = !BuildConfig.MOZILLA_OFFICIAL;
  private BaseHlsPlayer mPlayer = null;
  private boolean mDestroy = false;

  public static class Callbacks extends JNIObject implements BaseHlsPlayer.ResourceCallbacks {
    @WrapForJNI(calledFrom = "gecko")
    Callbacks() {}

    @Override
    @WrapForJNI
    public native void onLoad(String mediaUrl);

    @Override
    @WrapForJNI
    public native void onDataArrived();

    @Override
    @WrapForJNI
    public native void onError(int errorCode);

    @Override // JNIObject
    protected void disposeNative() {
      throw new UnsupportedOperationException();
    }
  } // Callbacks

  private GeckoHLSResourceWrapper(
      final String url, final BaseHlsPlayer.ResourceCallbacks callback) {
    if (DEBUG) Log.d(LOGTAG, "GeckoHLSResourceWrapper created with url = " + url);
    assertTrue(callback != null);

    mPlayer = GeckoPlayerFactory.getPlayer();
    try {
      mPlayer.init(url, callback);
    } catch (final Exception e) {
      Log.e(LOGTAG, "Failed to create GeckoHlsResourceWrapper !", e);
      callback.onError(BaseHlsPlayer.ResourceError.UNKNOWN.code());
    }
  }

  @WrapForJNI(calledFrom = "gecko")
  public static GeckoHLSResourceWrapper create(
      final String url, final BaseHlsPlayer.ResourceCallbacks callback) {
    return new GeckoHLSResourceWrapper(url, callback);
  }

  @WrapForJNI(calledFrom = "gecko")
  public int getPlayerId() {
    // GeckoHLSResourceWrapper should always be created before others
    assertTrue(!mDestroy);
    assertTrue(mPlayer != null);
    return mPlayer.getId();
  }

  @WrapForJNI(calledFrom = "gecko")
  public void suspend() {
    if (DEBUG) Log.d(LOGTAG, "GeckoHLSResourceWrapper suspend");
    if (mPlayer != null) {
      mPlayer.suspend();
    }
  }

  @WrapForJNI(calledFrom = "gecko")
  public void resume() {
    if (DEBUG) Log.d(LOGTAG, "GeckoHLSResourceWrapper resume");
    if (mPlayer != null) {
      mPlayer.resume();
    }
  }

  @WrapForJNI(calledFrom = "gecko")
  public void play() {
    if (DEBUG) Log.d(LOGTAG, "GeckoHLSResourceWrapper mediaelement played");
    if (mPlayer != null) {
      mPlayer.play();
    }
  }

  @WrapForJNI(calledFrom = "gecko")
  public void pause() {
    if (DEBUG) Log.d(LOGTAG, "GeckoHLSResourceWrapper mediaelement paused");
    if (mPlayer != null) {
      mPlayer.pause();
    }
  }

  private static void assertTrue(final boolean condition) {
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
      GeckoPlayerFactory.removePlayer(mPlayer);
      mPlayer.release();
      mPlayer = null;
    }
  }
}
