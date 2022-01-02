/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.util.Log;
import java.util.concurrent.ConcurrentLinkedQueue;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;
import org.mozilla.geckoview.BuildConfig;

public final class GeckoHLSDemuxerWrapper {
  private static final String LOGTAG = "GeckoHLSDemuxerWrapper";
  private static final boolean DEBUG = !BuildConfig.MOZILLA_OFFICIAL;

  // NOTE : These TRACK definitions should be synced with Gecko.
  public enum TrackType {
    UNDEFINED(0),
    AUDIO(1),
    VIDEO(2),
    TEXT(3);
    private int mType;

    private TrackType(final int type) {
      mType = type;
    }

    public int value() {
      return mType;
    }
  }

  private BaseHlsPlayer mPlayer = null;

  public static class Callbacks extends JNIObject implements BaseHlsPlayer.DemuxerCallbacks {
    @WrapForJNI(calledFrom = "gecko")
    Callbacks() {}

    @Override
    @WrapForJNI
    public native void onInitialized(boolean hasAudio, boolean hasVideo);

    @Override
    @WrapForJNI
    public native void onError(int errorCode);

    @Override // JNIObject
    protected void disposeNative() {
      throw new UnsupportedOperationException();
    }
  } // Callbacks

  private static void assertTrue(final boolean condition) {
    if (DEBUG && !condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  private BaseHlsPlayer.TrackType getPlayerTrackType(final int trackType) {
    if (trackType == TrackType.AUDIO.value()) {
      return BaseHlsPlayer.TrackType.AUDIO;
    } else if (trackType == TrackType.VIDEO.value()) {
      return BaseHlsPlayer.TrackType.VIDEO;
    } else if (trackType == TrackType.TEXT.value()) {
      return BaseHlsPlayer.TrackType.TEXT;
    }
    return BaseHlsPlayer.TrackType.UNDEFINED;
  }

  @WrapForJNI
  public long getBuffered() {
    assertTrue(mPlayer != null);
    return mPlayer.getBufferedPosition();
  }

  @WrapForJNI(calledFrom = "gecko")
  public static GeckoHLSDemuxerWrapper create(
      final int id, final BaseHlsPlayer.DemuxerCallbacks callback) {
    return new GeckoHLSDemuxerWrapper(id, callback);
  }

  @WrapForJNI
  public int getNumberOfTracks(final int trackType) {
    assertTrue(mPlayer != null);
    final int tracks = mPlayer.getNumberOfTracks(getPlayerTrackType(trackType));
    if (DEBUG) Log.d(LOGTAG, "[GetNumberOfTracks] type : " + trackType + ", num = " + tracks);
    return tracks;
  }

  @WrapForJNI
  public GeckoAudioInfo getAudioInfo(final int index) {
    assertTrue(mPlayer != null);
    if (DEBUG) Log.d(LOGTAG, "[getAudioInfo] formatIndex : " + index);
    final GeckoAudioInfo aInfo = mPlayer.getAudioInfo(index);
    return aInfo;
  }

  @WrapForJNI
  public GeckoVideoInfo getVideoInfo(final int index) {
    assertTrue(mPlayer != null);
    if (DEBUG) Log.d(LOGTAG, "[getVideoInfo] formatIndex : " + index);
    final GeckoVideoInfo vInfo = mPlayer.getVideoInfo(index);
    return vInfo;
  }

  @WrapForJNI
  public boolean seek(final long seekTime) {
    // seekTime : microseconds.
    assertTrue(mPlayer != null);
    if (DEBUG) Log.d(LOGTAG, "seek  : " + seekTime + " (Us)");
    return mPlayer.seek(seekTime);
  }

  GeckoHLSDemuxerWrapper(final int id, final BaseHlsPlayer.DemuxerCallbacks callback) {
    if (DEBUG) Log.d(LOGTAG, "Constructing GeckoHLSDemuxerWrapper ...");
    assertTrue(callback != null);
    try {
      mPlayer = GeckoPlayerFactory.getPlayer(id);
      if (mPlayer != null) {
        mPlayer.addDemuxerWrapperCallbackListener(callback);
      }
    } catch (final Exception e) {
      Log.e(LOGTAG, "Constructing GeckoHLSDemuxerWrapper ... error", e);
      callback.onError(BaseHlsPlayer.DemuxerError.UNKNOWN.code());
    }
  }

  @WrapForJNI
  private GeckoHLSSample[] getSamples(final int mediaType, final int number) {
    assertTrue(mPlayer != null);
    ConcurrentLinkedQueue<GeckoHLSSample> samples = null;
    // getA/VSamples will always return a non-null instance.
    samples = mPlayer.getSamples(getPlayerTrackType(mediaType), number);
    assertTrue(samples.size() <= number);
    return samples.toArray(new GeckoHLSSample[samples.size()]);
  }

  @WrapForJNI
  private long getNextKeyFrameTime() {
    assertTrue(mPlayer != null);
    return mPlayer.getNextKeyFrameTime();
  }

  @WrapForJNI
  private boolean isLiveStream() {
    assertTrue(mPlayer != null);
    return mPlayer.isLiveStream();
  }

  @WrapForJNI // Called when native object is destroyed.
  private void destroy() {
    if (DEBUG) Log.d(LOGTAG, "destroy!! Native object is destroyed.");
    if (mPlayer != null) {
      release();
    }
  }

  private void release() {
    assertTrue(mPlayer != null);
    if (DEBUG) Log.d(LOGTAG, "release BaseHlsPlayer...");
    GeckoPlayerFactory.removePlayer(mPlayer);
    mPlayer.release();
    mPlayer = null;
  }
}
