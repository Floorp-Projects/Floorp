/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

import org.mozilla.gecko.util.GeckoBundle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * GeckoSession applications can use this class to handle media events
 * and control the HTMLMediaElement externally.
 **/
@AnyThread
public class MediaElement {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MEDIA_STATE_PLAY, MEDIA_STATE_PLAYING, MEDIA_STATE_PAUSE,
             MEDIA_STATE_ENDED, MEDIA_STATE_SEEKING, MEDIA_STATE_SEEKED,
             MEDIA_STATE_STALLED, MEDIA_STATE_SUSPEND, MEDIA_STATE_WAITING,
             MEDIA_STATE_ABORT, MEDIA_STATE_EMPTIED})
    /* package */ @interface MediaStateFlags {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MEDIA_READY_STATE_HAVE_NOTHING, MEDIA_READY_STATE_HAVE_METADATA,
             MEDIA_READY_STATE_HAVE_CURRENT_DATA, MEDIA_READY_STATE_HAVE_FUTURE_DATA,
             MEDIA_READY_STATE_HAVE_ENOUGH_DATA})

    /* package */ @interface ReadyStateFlags {}

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MEDIA_ERROR_NETWORK_NO_SOURCE, MEDIA_ERROR_ABORTED, MEDIA_ERROR_NETWORK,
             MEDIA_ERROR_DECODE, MEDIA_ERROR_SRC_NOT_SUPPORTED})
    /* package */ @interface MediaErrorFlags {}

    /**
     * The media is no longer paused, as a result of the play method, or the autoplay attribute.
     */
    public static final int MEDIA_STATE_PLAY = 0;
    /**
     * Sent when the media has enough data to start playing, after the play event,
     * but also when recovering from being stalled, when looping media restarts,
     * and after seeked, if it was playing before seeking.
     */
    public static final int MEDIA_STATE_PLAYING = 1;
    /**
     * Sent when the playback state is changed to paused.
     */
    public static final int MEDIA_STATE_PAUSE = 2;
    /**
     * Sent when playback completes.
     */
    public static final int MEDIA_STATE_ENDED = 3;
    /**
     * Sent when a seek operation begins.
     */
    public static final int MEDIA_STATE_SEEKING = 4;
    /**
     * Sent when a seek operation completes.
     */
    public static final int MEDIA_STATE_SEEKED = 5;
    /**
     * Sent when the user agent is trying to fetch media data,
     * but data is unexpectedly not forthcoming.
     */
    public static final int MEDIA_STATE_STALLED = 6;
    /**
     * Sent when loading of the media is suspended. This may happen either because
     * the download has completed or because it has been paused for any other reason.
     */
    public static final int MEDIA_STATE_SUSPEND = 7;
    /**
     * Sent when the requested operation (such as playback) is delayed
     * pending the completion of another operation (such as a seek).
     */
    public static final int MEDIA_STATE_WAITING = 8;
    /**
     * Sent when playback is aborted; for example, if the media is playing
     * and is restarted from the beginning, this event is sent.
     */
    public static final int MEDIA_STATE_ABORT = 9;
    /**
     * The media has become empty. For example, this event is sent if the media
     * has already been loaded, and the load() method is called to reload it.
     */
    public static final int MEDIA_STATE_EMPTIED = 10;


    /**
     * No information is available about the media resource.
     */
    public static final int MEDIA_READY_STATE_HAVE_NOTHING = 0;
    /**
     * Enough of the media resource has been retrieved that the metadata
     * attributes are available.
     */
    public static final int MEDIA_READY_STATE_HAVE_METADATA = 1;
    /**
     * Data is available for the current playback position,
     * but not enough to actually play more than one frame.
     */
    public static final int MEDIA_READY_STATE_HAVE_CURRENT_DATA = 2;
    /**
     * Data for the current playback position as well as for at least a little
     * bit of time into the future is available.
     */
    public static final int MEDIA_READY_STATE_HAVE_FUTURE_DATA = 3;
    /**
     * Enough data is available—and the download rate is high enough that the media
     * can be played through to the end without interruption.
     */
    public static final int MEDIA_READY_STATE_HAVE_ENOUGH_DATA = 4;


    /**
     * Media source not found or unable to select any of the child elements
     * for playback during resource selection.
     */
    public static final int MEDIA_ERROR_NETWORK_NO_SOURCE = 0;
    /**
     * The fetching of the associated resource was aborted by the user's request.
     */
    public static final int MEDIA_ERROR_ABORTED = 1;
    /**
     * Some kind of network error occurred which prevented the media from being
     * successfully fetched, despite having previously been available.
     */
    public static final int MEDIA_ERROR_NETWORK = 2;
    /**
     * Despite having previously been determined to be usable,
     * an error occurred while trying to decode the media resource, resulting in an error.
     */
    public static final int MEDIA_ERROR_DECODE = 3;
    /**
     * The associated resource or media provider object has been found to be unsuitable.
     */
    public static final int MEDIA_ERROR_SRC_NOT_SUPPORTED = 4;

    /**
     * Data class with the Metadata associated to a Media Element.
     **/
    public static class Metadata {
        /**
         * Contains the current media source URI.
         */
        public final @Nullable String currentSource;

        /**
         * Indicates the duration of the media in seconds.
         */
        public final double duration;

        /**
         * Indicates the width of the video in device pixels.
         */
        public final long width;

        /**
         * Indicates the height of the video in device pixels.
         */
        public final long height;

        /**
         * Indicates if seek operations are compatible with the media.
         */
        public final boolean isSeekable;

        /**
         * Indicates the number of audio tracks included in the media.
         */
        public final int audioTrackCount;

        /**
         * Indicates the number of video tracks included in the media.
         */
        public final int videoTrackCount;

        /* package */ Metadata(final GeckoBundle bundle) {
            currentSource = bundle.getString("src", "");
            duration = bundle.getDouble("duration", 0);
            width = bundle.getLong("width", 0);
            height = bundle.getLong("height", 0);
            isSeekable = bundle.getBoolean("seekable", false);
            audioTrackCount = bundle.getInt("audioTrackCount", 0);
            videoTrackCount = bundle.getInt("videoTrackCount", 0);
        }

        /**
         * Empty constructor for tests.
         */
        protected Metadata() {
            currentSource = "";
            duration = 0;
            width = 0;
            height = 0;
            isSeekable = false;
            audioTrackCount = 0;
            videoTrackCount = 0;
        }
    }

    /**
     * Data class that indicates infomation about a media load progress event.
     **/
    public static class LoadProgressInfo {
        /**
         * Class used to represent a set of time ranges.
         */
        public class TimeRange {
            protected TimeRange(final double start, final double end) {
                this.start = start;
                this.end = end;
            }

            /**
             * The start time of the range in seconds.
             */
            public final double start;
            /**
             * The end time of the range in seconds.
             */
            public final double end;
        }

        /**
         * The number of bytes transferred since the beginning of the operation
         * or -1 if the data is not computable.
         */
        public final long loadedBytes;

        /**
         * The total number of bytes of content that will be transferred during the operation
         * or -1 if the data is not computable.
         */
        public final long totalBytes;

        /**
         * The ranges of the media source that the browser has currently buffered.
         * Null if the browser has not buffered any time range or the data is not computable.
         */
        public final @Nullable TimeRange[] buffered;

        /* package */ LoadProgressInfo(final GeckoBundle bundle) {
            loadedBytes = bundle.getLong("loadedBytes", -1);
            totalBytes = bundle.getLong("loadedBytes", -1);
            double[] starts = bundle.getDoubleArray("timeRangeStarts");
            double[] ends = bundle.getDoubleArray("timeRangeEnds");
            if (starts == null || ends == null) {
                buffered = null;
                return;
            }

            if (starts.length != ends.length) {
                throw new AssertionError("timeRangeStarts and timeRangeEnds length do not match");
            }

            buffered = new TimeRange[starts.length];
            for (int i = 0; i < starts.length; ++i) {
                buffered[i] = new TimeRange(starts[i], ends[i]);
            }
        }

        /**
         * Empty constructor for tests.
         */
        protected LoadProgressInfo() {
            loadedBytes = 0;
            totalBytes = 0;
            buffered = null;
        }
    }

    /**
     * This interface allows apps to handle media events.
     **/
    public interface Delegate {
        /**
         * The media playback state has changed.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param mediaState The playback state of the media.
         *                   One of the {@link #MEDIA_STATE_PLAY MEDIA_STATE_*} flags.
         */
        @UiThread
        default void onPlaybackStateChange(@NonNull MediaElement mediaElement,
                                           @MediaStateFlags int mediaState) {}

        /**
         * The readiness state of the media has changed.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param readyState The readiness state of the media.
         *                   One of the {@link #MEDIA_READY_STATE_HAVE_NOTHING MEDIA_READY_STATE_*} flags.
         */
        @UiThread
        default void onReadyStateChange(@NonNull MediaElement mediaElement,
                                        @ReadyStateFlags int readyState) {}

        /**
         * The media metadata has loaded or changed.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param metaData The MetaData values of the media.
         */
        @UiThread
        default void onMetadataChange(@NonNull MediaElement mediaElement,
                                      @NonNull Metadata metaData) {}

        /**
         * Indicates that a loading operation is in progress for the media.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param progressInfo Information about the load progress and buffered ranges.
         */
        @UiThread
        default void onLoadProgress(@NonNull MediaElement mediaElement,
                                    @NonNull LoadProgressInfo progressInfo) {}

        /**
         * The media audio volume has changed.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param volume The volume of the media.
         * @param muted True if the media is muted.
         */
        @UiThread
        default void onVolumeChange(@NonNull MediaElement mediaElement, double volume,
                                    boolean muted) {}

        /**
         * The current playback time has changed. This event is usually dispatched every 250ms.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param time The current playback time in seconds.
         */
        @UiThread
        default void onTimeChange(@NonNull MediaElement mediaElement, double time) {}

        /**
         * The media playback speed has changed.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param rate The current playback rate. A value of 1.0 indicates normal speed.
         */
        @UiThread
        default void onPlaybackRateChange(@NonNull MediaElement mediaElement, double rate) {}

        /**
         * A media element has entered or exited fullscreen mode.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param fullscreen True if the media has entered full screen mode.
         */
        @UiThread
        default void onFullscreenChange(@NonNull MediaElement mediaElement, boolean fullscreen) {}

        /**
         * An error has occurred.
         *
         * @param mediaElement A reference to the MediaElement that dispatched the event.
         * @param errorCode The error code.
         *                  One of the {@link #MEDIA_ERROR_NETWORK_NO_SOURCE MEDIA_ERROR_*} flags.
         */
        @UiThread
        default void onError(@NonNull MediaElement mediaElement, @MediaErrorFlags int errorCode) {}
    }

    /* package */ long getVideoId() {
        return mVideoId;
    }

    /**
     * Gets the current the media callback handler.
     *
     * @return the current media callback handler.
     */
    public @Nullable MediaElement.Delegate getDelegate() {
        return mDelegate;
    }

    /**
     * Sets the media callback handler.
     * This will replace the current handler.
     *
     * @param delegate An implementation of MediaDelegate.
     */
    public void setDelegate(final @Nullable MediaElement.Delegate delegate) {
        if (mDelegate == delegate) {
            return;
        }
        MediaElement.Delegate oldDelegate = mDelegate;
        mDelegate = delegate;
        if (oldDelegate != null && mDelegate == null) {
            mSession.getEventDispatcher().dispatch("GeckoView:MediaUnobserve", createMessage());
            mSession.getMediaElements().remove(mVideoId);
        } else if (oldDelegate == null) {
            mSession.getMediaElements().put(mVideoId, this);
            mSession.getEventDispatcher().dispatch("GeckoView:MediaObserve", createMessage());
        }
    }

    /**
     * Pauses the media.
     */
    public void pause() {
        mSession.getEventDispatcher().dispatch("GeckoView:MediaPause", createMessage());
    }

    /**
     * Plays the media.
     */
    public void play() {
        mSession.getEventDispatcher().dispatch("GeckoView:MediaPlay", createMessage());
    }

    /**
     * Seek the media to a given time.
     *
     * @param time Seek time in seconds.
     */
    public void seek(final double time) {
        final GeckoBundle message = createMessage();
        message.putDouble("time", time);
        mSession.getEventDispatcher().dispatch("GeckoView:MediaSeek", message);
    }

    /**
     * Set the volume at which the media will be played.
     *
     * @param volume A Volume value. It must fall between 0 and 1, where 0 is effectively muted
     *               and 1 is the loudest possible value.
     */
    public void setVolume(final double volume) {
        final GeckoBundle message = createMessage();
        message.putDouble("volume", volume);
        mSession.getEventDispatcher().dispatch("GeckoView:MediaSetVolume", message);
    }

    /**
     * Mutes the media.
     *
     * @param muted True in order to mute the audio.
     */
    public void setMuted(final boolean muted) {
        final GeckoBundle message = createMessage();
        message.putBoolean("muted", muted);
        mSession.getEventDispatcher().dispatch("GeckoView:MediaSetMuted", message);
    }

    /**
     * Sets the playback rate at which the media will be played.
     *
     * @param playbackRate The rate at which the media will be played.
     *                     A value of 1.0 indicates normal speed.
     */
    public void setPlaybackRate(final double playbackRate) {
        final GeckoBundle message = createMessage();
        message.putDouble("playbackRate", playbackRate);
        mSession.getEventDispatcher().dispatch("GeckoView:MediaSetPlaybackRate", message);
    }

    // Helper methods used for event observers to update the current video state

    /* package */ void notifyPlaybackStateChange(final String event) {
        @MediaStateFlags int state;
        switch (event.toLowerCase()) {
            case "play":
                state = MEDIA_STATE_PLAY;
                break;
            case "playing":
                state = MEDIA_STATE_PLAYING;
                break;
            case "pause":
                state = MEDIA_STATE_PAUSE;
                break;
            case "ended":
                state = MEDIA_STATE_ENDED;
                break;
            case "seeking":
                state = MEDIA_STATE_SEEKING;
                break;
            case "seeked":
                state = MEDIA_STATE_SEEKED;
                break;
            case "stalled":
                state = MEDIA_STATE_STALLED;
                break;
            case "suspend":
                state = MEDIA_STATE_SUSPEND;
                break;
            case "waiting":
                state = MEDIA_STATE_WAITING;
                break;
            case "abort":
                state = MEDIA_STATE_ABORT;
                break;
            case "emptied":
                state = MEDIA_STATE_EMPTIED;
                break;
            default:
                throw new UnsupportedOperationException(event + " HTMLMediaElement event not implemented");
        }

        if (mDelegate != null) {
            mDelegate.onPlaybackStateChange(this, state);
        }
    }

    /* package */ void notifyReadyStateChange(final int readyState) {
        if (mDelegate != null) {
            mDelegate.onReadyStateChange(this, readyState);
        }
    }

    /* package */ void notifyLoadProgress(final GeckoBundle message) {
        if (mDelegate != null) {
            mDelegate.onLoadProgress(this, new LoadProgressInfo(message));
        }
    }

    /* package */ void notifyTimeChange(final double currentTime) {
        if (mDelegate != null) {
            mDelegate.onTimeChange(this, currentTime);
        }
    }

    /* package */ void notifyVolumeChange(final double volume, final boolean muted) {
        if (mDelegate != null) {
            mDelegate.onVolumeChange(this, volume, muted);
        }
    }

    /* package */ void notifyPlaybackRateChange(final double rate) {
        if (mDelegate != null) {
            mDelegate.onPlaybackRateChange(this, rate);
        }
    }

    /* package */ void notifyMetadataChange(final GeckoBundle message) {
        if (mDelegate != null) {
            mDelegate.onMetadataChange(this, new Metadata(message));
        }
    }

    /* package */ void notifyFullscreenChange(final boolean fullscreen) {
        if (mDelegate != null) {
            mDelegate.onFullscreenChange(this, fullscreen);
        }
    }

    /* package */ void notifyError(final int aCode) {
        if (mDelegate != null) {
            mDelegate.onError(this, aCode);
        }
    }

    private GeckoBundle createMessage() {
        final GeckoBundle bundle = new GeckoBundle();
        bundle.putLong("id", mVideoId);
        return bundle;
    }

    /* package */ MediaElement(final long videoId, final GeckoSession session) {
        mVideoId = videoId;
        mSession = session;
    }

    final protected @NonNull GeckoSession mSession;
    final protected long mVideoId;
    protected @Nullable MediaElement.Delegate mDelegate;
}
