/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


package org.mozilla.gecko.media;

import android.util.Log;

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.util.MimeTypes;

import java.util.concurrent.ConcurrentLinkedQueue;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

public final class GeckoHlsDemuxerWrapper {
    private static final String LOGTAG = "GeckoHlsDemuxerWrapper";
    private static final boolean DEBUG = true;

    // NOTE : These TRACK definitions should be synced with Gecko.
    public enum TrackType {
        UNDEFINED(0),
        AUDIO(1),
        VIDEO(2),
        TEXT(3);
        private int mType;
        private TrackType(int type) {
            mType = type;
        }
        public int value() {
            return mType;
        }
    }

    private GeckoHlsPlayer mPlayer = null;

    public static class HlsDemuxerCallbacks extends JNIObject
        implements GeckoHlsPlayer.DemuxerCallbacks {

        @WrapForJNI(calledFrom = "gecko")
        HlsDemuxerCallbacks() {}

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
    } // HlsDemuxerCallbacks

    private static void assertTrue(boolean condition) {
        if (DEBUG && !condition) {
            throw new AssertionError("Expected condition to be true");
        }
    }

    private GeckoHlsPlayer.TrackType getPlayerTrackType(int trackType) {
        if (trackType == TrackType.AUDIO.value()) {
            return GeckoHlsPlayer.TrackType.AUDIO;
        } else if (trackType == TrackType.VIDEO.value()) {
            return GeckoHlsPlayer.TrackType.VIDEO;
        } else if (trackType == TrackType.TEXT.value()) {
            return GeckoHlsPlayer.TrackType.TEXT;
        }
        return GeckoHlsPlayer.TrackType.UNDEFINED;
    }

    @WrapForJNI
    public long getBuffered() {
        assertTrue(mPlayer != null);
        return mPlayer.getBufferedPosition();
    }

    @WrapForJNI(calledFrom = "gecko")
    public static GeckoHlsDemuxerWrapper create(GeckoHlsPlayer player,
                                                GeckoHlsPlayer.DemuxerCallbacks callback) {
        return new GeckoHlsDemuxerWrapper(player, callback);
    }

    @WrapForJNI
    public int getNumberOfTracks(int trackType) {
        int tracks = mPlayer != null ? mPlayer.getNumberOfTracks(getPlayerTrackType(trackType)) : 0;
        if (DEBUG) Log.d(LOGTAG, "[GetNumberOfTracks] type : " + trackType + ", num = " + tracks);
        return tracks;
    }

    @WrapForJNI
    public GeckoAudioInfo getAudioInfo(int index) {
        assertTrue(mPlayer != null);

        if (DEBUG) Log.d(LOGTAG, "[getAudioInfo] formatIndex : " + index);
        Format fmt = mPlayer.getAudioTrackFormat(index);
        if (fmt == null) {
            return null;
        }
        /* According to https://github.com/google/ExoPlayer/blob
         * /d979469659861f7fe1d39d153b90bdff1ab479cc/library/core/src/main
         * /java/com/google/android/exoplayer2/audio/MediaCodecAudioRenderer.java#L221-L224,
         * if the input audio format is not raw, exoplayer would assure that
         * the sample's pcm encoding bitdepth is 16.
         * For HLS content, it should always be 16.
         */
        assertTrue(!MimeTypes.AUDIO_RAW.equals(fmt.sampleMimeType));
        // For HLS content, csd-0 is enough.
        byte[] csd = fmt.initializationData.isEmpty() ? null : fmt.initializationData.get(0);
        GeckoAudioInfo aInfo = new GeckoAudioInfo(fmt.sampleRate, fmt.channelCount,
                                                  16, 0, mPlayer.getDuration(),
                                                  fmt.sampleMimeType, csd);
        return aInfo;
    }

    @WrapForJNI
    public GeckoVideoInfo getVideoInfo(int index) {
        assertTrue(mPlayer != null);

        if (DEBUG) Log.d(LOGTAG, "[getVideoInfo] formatIndex : " + index);
        Format fmt = mPlayer.getVideoTrackFormat(index);
        if (fmt == null) {
            return null;
        }
        GeckoVideoInfo vInfo = new GeckoVideoInfo(fmt.width, fmt.height,
                                                  fmt.width, fmt.height,
                                                  fmt.rotationDegrees, fmt.stereoMode,
                                                  mPlayer.getDuration(), fmt.sampleMimeType,
                                                  null, null);
        return vInfo;
    }

    @WrapForJNI
    public boolean seek(long seekTime) {
        // seekTime : microseconds.
        assertTrue(mPlayer != null);
        if (DEBUG) Log.d(LOGTAG, "seek  : " + seekTime + " (Us)");
        return mPlayer.seek(seekTime);
    }

    GeckoHlsDemuxerWrapper(GeckoHlsPlayer player,
                           GeckoHlsPlayer.DemuxerCallbacks callback) {
        if (DEBUG) Log.d(LOGTAG, "Constructing GeckoHlsDemuxerWrapper ...");
        assertTrue(callback != null);
        assertTrue(player != null);
        try {
            this.mPlayer = player;
            this.mPlayer.addDemuxerWrapperCallbackListener(callback);
        } catch (Exception e) {
            Log.e(LOGTAG, "Constructing GeckoHlsDemuxerWrapper ... error", e);
            callback.onError(GeckoHlsPlayer.DemuxerError.UNKNOWN.code());
        }
    }

    @WrapForJNI
    private GeckoHlsSample[] getSamples(int mediaType, int number) {
        ConcurrentLinkedQueue<GeckoHlsSample> samples = null;
        // getA/VSamples will always return a non-null instance.
        if (mediaType == TrackType.VIDEO.value()) {
            samples = mPlayer.getVideoSamples(number);
        } else if (mediaType == TrackType.AUDIO.value()) {
            samples = mPlayer.getAudioSamples(number);
        }

        assertTrue(samples.size() <= number);
        return samples.toArray(new GeckoHlsSample[samples.size()]);
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
        if (DEBUG) Log.d(LOGTAG, "release GeckoHlsPlayer...");
        mPlayer.release();
        mPlayer = null;
    }
}
