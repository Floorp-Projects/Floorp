/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import android.support.annotation.AnyThread;
import android.support.annotation.LongDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;

import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.mozglue.JNIObject;

/**
 * The MediaSession API provides media controls and events for a GeckoSession.
 * This includes support for the DOM Media Session API and regular HTML media
 * content.
 *
 * @see <a href="https://developer.mozilla.org/en-US/docs/Web/API/MediaSession">Media Session API</a>
 */
@UiThread
public class MediaSession {
    private static final String LOGTAG = "MediaSession";
    private static final boolean DEBUG = false;

    private final GeckoSession mSession;
    private Controller mController;

    private static final String ATTACHED_EVENT =
        "GeckoView:MediaSession:Attached";
    private boolean mControllerAttached;

    protected MediaSession(final GeckoSession session) {
        mSession = session;
    }

    /* package */ final class Controller extends JNIObject {
        private final long mId;

        /* package */ Controller(final long id) {
            mId = id;
        }

        public long getId() {
            return mId;
        }

        @Override // JNIObject
        public void disposeNative() {
            // Dispose in native code.
            throw new UnsupportedOperationException();
        }

        @WrapForJNI(calledFrom = "ui")
        /* package */ void onAttached() {
            MediaSession.this.onControllerAttached();
        }

        @WrapForJNI(calledFrom = "ui")
        /* package */ void onDetached() {
            MediaSession.this.onControllerDetached();
        }

        @WrapForJNI(dispatchTo = "gecko")
        public native void pause();

        @WrapForJNI(dispatchTo = "gecko")
        public native void stop();

        @WrapForJNI(dispatchTo = "gecko")
        public native void play();

        @WrapForJNI(dispatchTo = "gecko")
        public native void skipAd();

        @WrapForJNI(dispatchTo = "gecko")
        public native void focus();

        @WrapForJNI(dispatchTo = "gecko")
        public native void seekTo(double time, boolean fast);

        @WrapForJNI(dispatchTo = "gecko")
        public native void seekForward(double offset);

        @WrapForJNI(dispatchTo = "gecko")
        public native void seekBackward(double offset);

        @WrapForJNI(dispatchTo = "gecko")
        public native void nextTrack();

        @WrapForJNI(dispatchTo = "gecko")
        public native void previousTrack();

        @WrapForJNI(dispatchTo = "gecko")
        public native void muteAudio(boolean mute);
    }

    /* package */ Controller getController() {
        return mController;
    }

    /**
     * Get whether the media session is active.
     * Only active media sessions can be controlled.
     * Inactive media session may receive state events since some state events
     * may be dispatched before the media session becomes active.
     *
     * Changes in the active state are notified via {@link Delegate#onActivated}
     * and {@link Delegate#onDeactivated} respectively.
     *
     * @see MediaSession.Delegate#onActivated
     * @see MediaSession.Delegate#onDeactivated
     *
     * @return True if this media session is active, false otherwise.
     */
    public boolean isActive() {
        return mControllerAttached;
    }

    /* package */ void attachController(final long id) {
        mController = new Controller(id);
        mSession.attachMediaSessionController(mController);
    }

    void onControllerAttached() {
        mControllerAttached = true;
        // TODO: Remove temp workaround once we move to webidl (bug 1658937).
        mSession.getEventDispatcher().dispatch(ATTACHED_EVENT, null);
    }

    void onControllerDetached() {
        if (!mControllerAttached) {
            return;
        }
        mControllerAttached = false;
        // TODO: Remove temp workaround once we move to webidl (bug 1658937).
        mSession.getEventDispatcher().dispatch(DEACTIVATED_EVENT, null);
    }

    /* package */ void detachController() {
        if (mControllerAttached) {
            return;
        }
        mSession.detachMediaSessionController(mController);
        mControllerAttached = false;
        mController = null;
    }

    /**
     * Pause playback for the media session.
     */
    public void pause() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "pause");
        }
        mController.pause();
    }

    /**
     * Stop playback for the media session.
     */
    public void stop() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "stop");
        }
        mController.stop();
    }

    /**
     * Start playback for the media session.
     */
    public void play() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "play");
        }
        mController.play();
    }

    /**
     * Seek to a specific time.
     * Prefer using fast seeking when calling this in a sequence.
     * Don't use fast seeking for the last or only call in a sequence.
     *
     * @param time The time in seconds to move the playback time to.
     * @param fast Whether fast seeking should be used.
     */
    public void seekTo(final double time, final boolean fast) {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "seekTo: time=" + time + ", fast=" + fast);
        }
        mController.seekTo(time, fast);
    }

    /**
     * Seek forward by a sensible number of seconds.
     */
    public void seekForward() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "seekForward");
        }
        mController.seekForward(0.0);
    }

    /**
     * Seek backward by a sensible number of seconds.
     */
    public void seekBackward() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "seekBackward");
        }
        mController.seekBackward(0.0);
    }

    /**
     * Select and play the next track.
     * Move playback to the next item in the playlist when supported.
     */
    public void nextTrack() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "nextTrack");
        }
        mController.nextTrack();
    }

    /**
     * Select and play the previous track.
     * Move playback to the previous item in the playlist when supported.
     */
    public void previousTrack() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "previousTrack");
        }
        mController.previousTrack();
    }

    /**
     * Skip the advertisement that is currently playing.
     */
    public void skipAd() {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "skipAd");
        }
        mController.skipAd();
    }

    /**
     * Set whether audio should be muted.
     * Muting audio is supported by default and does not require the media
     * session to be active.
     *
     * @param mute True if audio for this media session should be muted.
     */
    public void muteAudio(final boolean mute) {
        if (!mControllerAttached) {
            return;
        }
        if (DEBUG) {
            Log.d(LOGTAG, "muteAudio=" + mute);
        }
        mController.muteAudio(mute);
    }

    // TODO: Not sure if we want it.
    // public void focus() {}

    /**
     * Implement this delegate to receive media session events.
     */
    @UiThread
    public interface Delegate {
        /**
         * Notify that the given media session has become active.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         */
        default void onActivated(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession) {}

        /**
         * Notify that the given media session has become inactive.
         * Inactive media sessions can not be controlled.
         *
         * TODO: Add settings links to control behavior.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         */
        default void onDeactivated(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession) {}

        /**
         * Notify on updated metadata.
         * Metadata may be provided by content via the DOM API or by GeckoView
         * when not availble.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         * @param meta The updated metadata.
         */
        default void onMetadata(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession,
                @NonNull Metadata meta) {}

        /**
         * Notify on updated supported features.
         * Unsupported actions will have no effect.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         * @param features A combination of {@link Feature}.
         */
        default void onFeatures(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession,
                @MSFeature long features) {}

        /**
         * Notify that playback has started for the given media session.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         */
        default void onPlay(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession) {}

        /**
         * Notify that playback has paused for the given media session.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         */
        default void onPause(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession) {}

        /**
         * Notify that playback has stopped for the given media session.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         */
        default void onStop(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession) {}

        /**
         * Notify on updated position state.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         * @param state An instance of {@link PositionState}.
         */
        default void onPositionState(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession,
                @NonNull PositionState state) {}

        /**
         * Notify on changed fullscreen state.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         * @param enabled True when this media session in in fullscreen mode.
         * @param meta An instance of {@link ElementMetadata}, if enabled.
         */
        default void onFullscreen(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession,
                boolean enabled,
                @Nullable ElementMetadata meta) {}

        /**
         * Notify on changed picture-in-picture mode state.
         *
         * @param session The associated GeckoSession.
         * @param mediaSession The media session for the given GeckoSession.
         * @param enabled True when this media session in in picture-in-picture
         *                mode.
         */
        default void onPictureInPicture(
                @NonNull GeckoSession session,
                @NonNull MediaSession mediaSession,
                boolean enabled) {}
    }


    /**
     * The representation of a media element's metadata.
     */
    public static class ElementMetadata {
        /**
         * The media source URI.
         */
        public final @Nullable String source;

        /**
         * The duration of the media in seconds.
         */
        public final double duration;

        /**
         * The width of the video in device pixels.
         */
        public final long width;

        /**
         * The height of the video in device pixels.
         */
        public final long height;

        /**
         * The number of audio tracks contained in this element.
         */
        public final int audioTrackCount;

        /**
         * The number of video tracks contained in this element.
         */
        public final int videoTrackCount;

        /**
         * ElementMetadata constructor.
         *
         * @param source The media URI.
         * @param duration The media duration in seconds.
         * @param width The video width in device pixels.
         * @param height The video height in device pixels.
         * @param audioTrackCount The audio track count.
         * @param videoTrackCount The video track count.
         */
        public ElementMetadata(
                @Nullable final String source,
                final double duration,
                final long width,
                final long height,
                final int audioTrackCount,
                final int videoTrackCount) {
            this.source = source;
            this.duration = duration;
            this.width = width;
            this.height = height;
            this.audioTrackCount = audioTrackCount;
            this.videoTrackCount = videoTrackCount;
        }

        /* package */ static @NonNull ElementMetadata fromBundle(
                final GeckoBundle bundle) {
            // Sync with MediaUtils.jsm.
            return new ElementMetadata(
                    bundle.getString("src"),
                    bundle.getDouble("duration", 0.0),
                    bundle.getLong("width", 0),
                    bundle.getLong("height", 0),
                    bundle.getInt("audioTrackCount", 0),
                    bundle.getInt("videoTrackCount", 0));
        }
    }

    /**
     * The representation of a media session's metadata.
     */
    public static class Metadata {
        /**
         * The media title.
         * May be backfilled based on the document's title.
         * May be null or empty.
         */
        public final @Nullable String title;

        /**
         * The media artist name.
         * May be null or empty.
         */
        public final @Nullable String artist;

        /**
         * The media album title.
         * May be null or empty.
         */
        public final @Nullable String album;

        /**
         * Metadata constructor.
         *
         * @param title The media title string.
         * @param artist The media artist string.
         * @param album The media album string.
         */
        protected Metadata(
                final @Nullable String title,
                final @Nullable String artist,
                final @Nullable String album) {
            this.title = title;
            this.artist = artist;
            this.album = album;
        }

        @AnyThread
        /* package */ static final class Builder {
            private final GeckoBundle mBundle;

            public Builder(final GeckoBundle bundle) {
                mBundle = new GeckoBundle(bundle);
            }

            public Builder(final Metadata meta) {
                mBundle = meta.toBundle();
            }

            @NonNull Builder title(final @Nullable String title) {
                mBundle.putString("title", title);
                return this;
            }

            @NonNull Builder artist(final @Nullable String artist) {
                mBundle.putString("artist", artist);
                return this;
            }

            @NonNull Builder album(final @Nullable String album) {
                mBundle.putString("album", album);
                return this;
            }
        }

        /* package */ static @NonNull Metadata fromBundle(
                final GeckoBundle bundle) {
            return new Metadata(
                  bundle.getString("title"),
                  bundle.getString("artist"),
                  bundle.getString("album"));
        }

        /* package */ @NonNull GeckoBundle toBundle() {
            final GeckoBundle bundle = new GeckoBundle(3);
            bundle.putString("title", title);
            bundle.putString("artist", artist);
            bundle.putString("album", album);
            return bundle;
        }

        @Override
        public String toString() {
            final StringBuilder builder = new StringBuilder("Metadata {");
            builder
                .append(", title=").append(title)
                .append(", artist=").append(artist)
                .append(", album=").append(album)
                .append("}");
            return builder.toString();
        }
    }

    /**
     * Holds the details of the media session's playback state.
     */
    public static class PositionState {
        /**
         * The duration of the media in seconds.
         */
        public final double duration;

        /**
         * The last reported media playback position in seconds.
         */
        public final double position;

        /**
         * The media playback rate coefficient.
         * The rate is positive for forward and negative for backward playback.
         */
        public final double playbackRate;

        /**
         * PositionState constructor.
         *
         * @param duration The media duration in seconds.
         * @param position The current media playback position in seconds.
         * @param playbackRate The playback rate coefficient.
         */
        protected PositionState(
                final double duration,
                final double position,
                final double playbackRate) {
            this.duration = duration;
            this.position = position;
            this.playbackRate = playbackRate;
        }

        /* package */ static @NonNull PositionState fromBundle(
                final GeckoBundle bundle) {
            return new PositionState(
                  bundle.getDouble("duration"),
                  bundle.getDouble("position"),
                  bundle.getDouble("playbackRate"));
        }

        @Override
        public String toString() {
            final StringBuilder builder = new StringBuilder("PositionState {");
            builder
                .append("duration=").append(duration)
                .append(", position=").append(position)
                .append(", playbackRate=").append(playbackRate)
                .append("}");
            return builder.toString();
        }
    }

    @Retention(RetentionPolicy.SOURCE)
    @LongDef(flag = true,
             value = {
                 Feature.NONE, Feature.PLAY, Feature.PAUSE, Feature.STOP,
                 Feature.SEEK_TO, Feature.SEEK_FORWARD, Feature.SEEK_BACKWARD,
                 Feature.SKIP_AD, Feature.NEXT_TRACK, Feature.PREVIOUS_TRACK,
                 //Feature.SET_VIDEO_SURFACE,
                 Feature.FOCUS })
    /* package */ @interface MSFeature {}

    /**
     * Flags for supported media session features.
     */
    public static class Feature {
        public static final long NONE = 0;

        /**
         * Playback supported.
         */
        public static final long PLAY = 1 << 0;

        /**
         * Pausing supported.
         */
        public static final long PAUSE = 1 << 1;

        /**
         * Stopping supported.
         */
        public static final long STOP = 1 << 2;

        /**
         * Absolute seeking supported.
         */
        public static final long SEEK_TO = 1 << 3;

        /**
         * Relative seeking supported (forward).
         */
        public static final long SEEK_FORWARD = 1 << 4;

        /**
         * Relative seeking supported (backward).
         */
        public static final long SEEK_BACKWARD = 1 << 5;

        /**
         * Skipping advertisements supported.
         */
        public static final long SKIP_AD = 1 << 6;

        /**
         * Next track selection supported.
         */
        public static final long NEXT_TRACK = 1 << 7;

        /**
         * Previous track selection supported.
         */
        public static final long PREVIOUS_TRACK = 1 << 8;

        /**
         * Focusing supported.
         */
        public static final long FOCUS = 1 << 9;

        // /**
        //  * Custom video surface supported.
        //  */
        // public static final long SET_VIDEO_SURFACE = 1 << 10;

        /* package */ static long fromBundle(final GeckoBundle bundle) {
            // Sync with MediaController.webidl.
            final long features =
                    NONE |
                    (bundle.getBoolean("play") ? PLAY : NONE) |
                    (bundle.getBoolean("pause") ? PAUSE : NONE) |
                    (bundle.getBoolean("stop") ? STOP : NONE) |
                    (bundle.getBoolean("seekto") ? SEEK_TO : NONE) |
                    (bundle.getBoolean("seekforward") ? SEEK_FORWARD : NONE) |
                    (bundle.getBoolean("seekbackward") ? SEEK_BACKWARD : NONE) |
                    (bundle.getBoolean("nexttrack") ? NEXT_TRACK : NONE) |
                    (bundle.getBoolean("previoustrack") ? PREVIOUS_TRACK : NONE) |
                    (bundle.getBoolean("skipad") ? SKIP_AD : NONE) |
                    (bundle.getBoolean("focus") ? FOCUS : NONE);
            return features;
        }
    }

    private static final String ACTIVATED_EVENT =
        "GeckoView:MediaSession:Activated";
    private static final String DEACTIVATED_EVENT =
        "GeckoView:MediaSession:Deactivated";
    private static final String METADATA_EVENT =
        "GeckoView:MediaSession:Metadata";
    private static final String POSITION_STATE_EVENT =
        "GeckoView:MediaSession:PositionState";
    private static final String FEATURES_EVENT =
        "GeckoView:MediaSession:Features";
    private static final String FULLSCREEN_EVENT =
        "GeckoView:MediaSession:Fullscreen";
    private static final String PICTURE_IN_PICTURE_EVENT =
        "GeckoView:MediaSession:PictureInPicture";
    private static final String PLAYBACK_NONE_EVENT =
        "GeckoView:MediaSession:Playback:None";
    private static final String PLAYBACK_PAUSED_EVENT =
        "GeckoView:MediaSession:Playback:Paused";
    private static final String PLAYBACK_PLAYING_EVENT =
        "GeckoView:MediaSession:Playback:Playing";

    /* package */ static class Handler
            extends GeckoSessionHandler<MediaSession.Delegate> {

        private final GeckoSession mSession;
        private final MediaSession mMediaSession;

        public Handler(final GeckoSession session) {
            super(
                "GeckoViewMediaControl",
                session,
                new String[]{
                    ATTACHED_EVENT,
                    ACTIVATED_EVENT,
                    DEACTIVATED_EVENT,
                    METADATA_EVENT,
                    FULLSCREEN_EVENT,
                    PICTURE_IN_PICTURE_EVENT,
                    POSITION_STATE_EVENT,
                    PLAYBACK_NONE_EVENT,
                    PLAYBACK_PAUSED_EVENT,
                    PLAYBACK_PLAYING_EVENT,
                    FEATURES_EVENT,
                });
            mSession = session;
            mMediaSession = new MediaSession(session);
        }

        @Override
        public void handleMessage(
                final Delegate delegate,
                final String event,
                final GeckoBundle message,
                final EventCallback callback) {
            if (DEBUG) {
                Log.d(LOGTAG, "handleMessage " + event);
            }

            if (ATTACHED_EVENT.equals(event)) {
                delegate.onActivated(mSession, mMediaSession);
            } else if (ACTIVATED_EVENT.equals(event)) {
                mMediaSession.attachController(message.getLong("id"));
                // TODO: We can call this direclty, once we move to webidl.
                // delegate.onActivated(mSession, mMediaSession);
            } else if (DEACTIVATED_EVENT.equals(event)) {
                mMediaSession.detachController();
                delegate.onDeactivated(mSession, mMediaSession);
            } else if (METADATA_EVENT.equals(event)) {
                final Metadata meta = Metadata.fromBundle(message);
                delegate.onMetadata(mSession, mMediaSession, meta);
            } else if (POSITION_STATE_EVENT.equals(event)) {
                final PositionState state =
                        PositionState.fromBundle(message.getBundle("state"));
                delegate.onPositionState(mSession, mMediaSession, state);
            } else if (PLAYBACK_NONE_EVENT.equals(event)) {
                delegate.onStop(mSession, mMediaSession);
            } else if (PLAYBACK_PAUSED_EVENT.equals(event)) {
                delegate.onPause(mSession, mMediaSession);
            } else if (PLAYBACK_PLAYING_EVENT.equals(event)) {
                delegate.onPlay(mSession, mMediaSession);
            } else if (FEATURES_EVENT.equals(event)) {
                final long features = Feature.fromBundle(
                        message.getBundle("features"));
                delegate.onFeatures(mSession, mMediaSession, features);
            } else if (FULLSCREEN_EVENT.equals(event)) {
                final boolean enabled = message.getBoolean("enabled");
                final ElementMetadata meta =
                        ElementMetadata.fromBundle(
                                message.getBundle("metadata"));
                delegate.onFullscreen(mSession, mMediaSession, enabled, meta);
            } else if (PICTURE_IN_PICTURE_EVENT.equals(event)) {
                final boolean enabled = message.getBoolean("enabled");
                delegate.onPictureInPicture(mSession, mMediaSession, enabled);
            }
        }
    }
}
