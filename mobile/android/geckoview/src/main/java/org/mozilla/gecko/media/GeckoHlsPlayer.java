/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.util.Log;

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.ExoPlaybackException;
import com.google.android.exoplayer2.ExoPlayer;
import com.google.android.exoplayer2.ExoPlayerFactory;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.PlaybackParameters;
import com.google.android.exoplayer2.RendererCapabilities;
import com.google.android.exoplayer2.Timeline;
import com.google.android.exoplayer2.source.MediaSource;
import com.google.android.exoplayer2.source.TrackGroup;
import com.google.android.exoplayer2.source.TrackGroupArray;
import com.google.android.exoplayer2.source.hls.HlsMediaSource;
import com.google.android.exoplayer2.trackselection.AdaptiveTrackSelection;
import com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import com.google.android.exoplayer2.trackselection.MappingTrackSelector.MappedTrackInfo;
import com.google.android.exoplayer2.trackselection.TrackSelection;
import com.google.android.exoplayer2.trackselection.TrackSelectionArray;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.android.exoplayer2.upstream.DefaultBandwidthMeter;
import com.google.android.exoplayer2.upstream.DefaultDataSourceFactory;
import com.google.android.exoplayer2.upstream.DefaultHttpDataSource;
import com.google.android.exoplayer2.upstream.DefaultHttpDataSourceFactory;
import com.google.android.exoplayer2.upstream.HttpDataSource;
import com.google.android.exoplayer2.util.MimeTypes;
import com.google.android.exoplayer2.util.Util;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.ReflectionTarget;

import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

@ReflectionTarget
public class GeckoHlsPlayer implements BaseHlsPlayer, ExoPlayer.EventListener {
    private static final String LOGTAG = "GeckoHlsPlayer";
    private static final DefaultBandwidthMeter BANDWIDTH_METER = new DefaultBandwidthMeter();
    private static final int MAX_TIMELINE_ITEM_LINES = 3;
    private static boolean DEBUG = AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG_BUILD;

    private static AtomicInteger sPlayerId = new AtomicInteger(0);
    /*
     *  Because we treat GeckoHlsPlayer as a source data provider.
     *  It will be created and initialized with a URL by HLSResource in
     *  Gecko media pipleine (in cpp). Once HLSDemuxer is created later, we
     *  need to bridge this HLSResource to the created demuxer. And they share
     *  the same GeckoHlsPlayer.
     *  mPlayerId is a token used for Gecko media pipeline to obtain corresponding player.
     */
    private final int mPlayerId;

    private DataSource.Factory mMediaDataSourceFactory;

    private Handler mMainHandler;
    private ExoPlayer mPlayer;
    private GeckoHlsRendererBase[] mRenderers;
    private DefaultTrackSelector mTrackSelector;
    private MediaSource mMediaSource;
    private ComponentListener mComponentListener;
    private ComponentEventDispatcher mComponentEventDispatcher;

    private boolean mIsTimelineStatic = false;
    private long mDurationUs;

    private GeckoHlsVideoRenderer mVRenderer = null;
    private GeckoHlsAudioRenderer mARenderer = null;

    // Able to control if we only want V/A/V+A tracks from bitstream.
    private class RendererController {
        private final boolean mEnableV;
        private final boolean mEnableA;
        RendererController(boolean enableVideoRenderer, boolean enableAudioRenderer) {
            this.mEnableV = enableVideoRenderer;
            this.mEnableA = enableAudioRenderer;
        }
        boolean isVideoRendererEnabled() { return mEnableV; }
        boolean isAudioRendererEnabled() { return mEnableA; }
    }
    private RendererController mRendererController = new RendererController(true, true);

    // Provide statistical information of tracks.
    private class HlsMediaTracksInfo {
        private int mNumVideoTracks = 0;
        private int mNumAudioTracks = 0;
        private boolean mVideoInfoUpdated = false;
        private boolean mAudioInfoUpdated = false;
        private boolean mVideoDataArrived = false;
        private boolean mAudioDataArrived = false;
        HlsMediaTracksInfo(int numVideoTracks, int numAudioTracks) {
            this.mNumVideoTracks = numVideoTracks;
            this.mNumAudioTracks = numAudioTracks;
        }
        public boolean hasVideo() { return mNumVideoTracks > 0; }
        public boolean hasAudio() { return mNumAudioTracks > 0; }
        public int getNumOfVideoTracks() { return mNumVideoTracks; }
        public int getNumOfAudioTracks() { return mNumAudioTracks; }
        public void onVideoInfoUpdated() { mVideoInfoUpdated = true; }
        public void onAudioInfoUpdated() { mAudioInfoUpdated = true; }
        public void onDataArrived(int trackType) {
            if (trackType == C.TRACK_TYPE_VIDEO) {
                mVideoDataArrived = true;
            } else if (trackType == C.TRACK_TYPE_AUDIO) {
                mAudioDataArrived = true;
            }
        }
        public boolean videoReady() {
            return !hasVideo() || (mVideoInfoUpdated && mVideoDataArrived);
        }
        public boolean audioReady() {
            return !hasAudio() || (mAudioInfoUpdated && mAudioDataArrived);
        }
    }
    private HlsMediaTracksInfo mTracksInfo = null;

    private boolean mIsPlayerInitDone = false;
    private boolean mIsDemuxerInitDone = false;

    private BaseHlsPlayer.DemuxerCallbacks mDemuxerCallbacks;
    private BaseHlsPlayer.ResourceCallbacks mResourceCallbacks;

    private static void assertTrue(boolean condition) {
      if (DEBUG && !condition) {
        throw new AssertionError("Expected condition to be true");
      }
    }

    protected void checkInitDone() {
        if (mIsDemuxerInitDone) {
            return;
        }
        assertTrue(mDemuxerCallbacks != null);
        assertTrue(mTracksInfo != null);

        if (DEBUG) {
            Log.d(LOGTAG, "[checkInitDone] VReady:" + mTracksInfo.videoReady() +
                    ",AReady:" + mTracksInfo.audioReady() +
                    ",hasV:" + mTracksInfo.hasVideo() +
                    ",hasA:" + mTracksInfo.hasAudio());
        }
        if (mTracksInfo.videoReady() && mTracksInfo.audioReady()) {
            mDemuxerCallbacks.onInitialized(mTracksInfo.hasAudio(), mTracksInfo.hasVideo());
            mIsDemuxerInitDone = true;
        }
    }

    public final class ComponentEventDispatcher {
        public void onDataArrived(final int trackType) {
            assertTrue(mMainHandler != null);
            assertTrue(mComponentListener != null);
            if (!mIsPlayerInitDone) {
                return;
            }
            if (mMainHandler != null && mComponentListener != null) {
                mMainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mComponentListener.onDataArrived(trackType);
                    }
                });
            }
        }

        public void onVideoInputFormatChanged(final Format format) {
            assertTrue(mMainHandler != null);
            assertTrue(mComponentListener != null);
            if (!mIsPlayerInitDone) {
                return;
            }
            if (mMainHandler != null && mComponentListener != null) {
                mMainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mComponentListener.onVideoInputFormatChanged(format);
                    }
                });
            }
        }

        public void onAudioInputFormatChanged(final Format format) {
            assertTrue(mMainHandler != null);
            assertTrue(mComponentListener != null);
            if (!mIsPlayerInitDone) {
                return;
            }
            if (mMainHandler != null && mComponentListener != null) {
                mMainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mComponentListener.onAudioInputFormatChanged(format);
                    }
                });
            }
        }
    }

    public final class ComponentListener {

        // General purpose implementation
        public synchronized void onDataArrived(int trackType) {
            if (DEBUG) { Log.d(LOGTAG, "[CB][onDataArrived] id " + mPlayerId); }
            if (!mIsPlayerInitDone) {
                return;
            }
            assertTrue(mResourceCallbacks != null);
            assertTrue(mTracksInfo != null);
            mTracksInfo.onDataArrived(trackType);
            mResourceCallbacks.onDataArrived();
            checkInitDone();
        }

        public synchronized void onVideoInputFormatChanged(Format format) {
            if (DEBUG) {
                Log.d(LOGTAG, "[CB] onVideoInputFormatChanged [" + format + "]");
                Log.d(LOGTAG, "[CB] SampleMIMEType [" +
                              format.sampleMimeType + "], ContainerMIMEType [" +
                              format.containerMimeType + "], id : " + mPlayerId);
            }
            if (!mIsPlayerInitDone) {
                return;
            }
            assertTrue(mTracksInfo != null);
            mTracksInfo.onVideoInfoUpdated();
            checkInitDone();
        }

        public synchronized void onAudioInputFormatChanged(Format format) {
            if (DEBUG) { Log.d(LOGTAG, "[CB] onAudioInputFormatChanged [" + format + "], mPlayerId :" + mPlayerId); }
            if (!mIsPlayerInitDone) {
                return;
            }
            assertTrue(mTracksInfo != null);
            mTracksInfo.onAudioInfoUpdated();
            checkInitDone();
        }
    }

    private DataSource.Factory buildDataSourceFactory(Context ctx, DefaultBandwidthMeter bandwidthMeter) {
        return new DefaultDataSourceFactory(ctx, bandwidthMeter,
                buildHttpDataSourceFactory(bandwidthMeter));
    }

    private HttpDataSource.Factory buildHttpDataSourceFactory(DefaultBandwidthMeter bandwidthMeter) {
        return new DefaultHttpDataSourceFactory(
            AppConstants.USER_AGENT_FENNEC_MOBILE,
            bandwidthMeter /* listener */,
            DefaultHttpDataSource.DEFAULT_CONNECT_TIMEOUT_MILLIS,
            DefaultHttpDataSource.DEFAULT_READ_TIMEOUT_MILLIS,
            true /* allowCrossProtocolRedirects */
        );
    }

    // To make sure that each player has a unique id, GeckoHlsPlayer should be
    // created only from synchronized APIs in GeckoPlayerFactory.
    public GeckoHlsPlayer() {
        mPlayerId = sPlayerId.incrementAndGet();
        if (DEBUG) { Log.d(LOGTAG, " construct player with id(" + mPlayerId + ")"); }
    }

    // Should be only called by GeckoPlayerFactory and GeckoHLSResourceWrapper.
    // The mPlayerId is used to make sure that the same GeckoHlsPlayer is used by
    // corresponding HLSResource and HLSDemuxer for each media playback.
    @Override
    public int getId() {
        return mPlayerId;
    }

    @Override
    public void addResourceWrapperCallbackListener(BaseHlsPlayer.ResourceCallbacks callback) {
        if (DEBUG) { Log.d(LOGTAG, " addResourceWrapperCallbackListener ..."); }
        mResourceCallbacks = callback;
    }

    @Override
    public void addDemuxerWrapperCallbackListener(BaseHlsPlayer.DemuxerCallbacks callback) {
        if (DEBUG) { Log.d(LOGTAG, " addDemuxerWrapperCallbackListener ..."); }
        mDemuxerCallbacks = callback;
    }

    @Override
    public void onLoadingChanged(boolean isLoading) {
        if (DEBUG) { Log.d(LOGTAG, "loading [" + isLoading + "]"); }
        if (!isLoading) {
            // To update buffered position.
            mComponentEventDispatcher.onDataArrived(C.TRACK_TYPE_DEFAULT);
        }
    }

    @Override
    public void onPlayerStateChanged(boolean playWhenReady, int state) {
        if (DEBUG) { Log.d(LOGTAG, "state [" + playWhenReady + ", " + getStateString(state) + "]"); }
        if (state == ExoPlayer.STATE_READY) {
            mPlayer.setPlayWhenReady(true);
        }
    }

    @Override
    public void onPositionDiscontinuity() {
        if (DEBUG) { Log.d(LOGTAG, "positionDiscontinuity"); }
    }

    @Override
    public void onPlaybackParametersChanged(PlaybackParameters playbackParameters) {
        if (DEBUG) {
            Log.d(LOGTAG, "playbackParameters " +
                  String.format("[speed=%.2f, pitch=%.2f]", playbackParameters.speed, playbackParameters.pitch));
        }
    }

    @Override
    public void onPlayerError(ExoPlaybackException e) {
        if (DEBUG) { Log.e(LOGTAG, "playerFailed" , e); }
        mIsPlayerInitDone = false;
        if (mResourceCallbacks != null) {
            mResourceCallbacks.onError(ResourceError.PLAYER.code());
        }
        if (mDemuxerCallbacks != null) {
            mDemuxerCallbacks.onError(DemuxerError.PLAYER.code());
        }
    }

    @Override
    public synchronized void onTracksChanged(TrackGroupArray ignored, TrackSelectionArray trackSelections) {
        if (DEBUG) {
            Log.d(LOGTAG, "onTracksChanged : TGA[" + ignored +
                          "], TSA[" + trackSelections + "]");

            MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
            if (mappedTrackInfo == null) {
              Log.d(LOGTAG, "Tracks []");
              return;
            }
            Log.d(LOGTAG, "Tracks [");
            // Log tracks associated to renderers.
            for (int rendererIndex = 0; rendererIndex < mappedTrackInfo.length; rendererIndex++) {
              TrackGroupArray rendererTrackGroups = mappedTrackInfo.getTrackGroups(rendererIndex);
              TrackSelection trackSelection = trackSelections.get(rendererIndex);
              if (rendererTrackGroups.length > 0) {
                Log.d(LOGTAG, "  Renderer:" + rendererIndex + " [");
                for (int groupIndex = 0; groupIndex < rendererTrackGroups.length; groupIndex++) {
                  TrackGroup trackGroup = rendererTrackGroups.get(groupIndex);
                  String adaptiveSupport = getAdaptiveSupportString(trackGroup.length,
                          mappedTrackInfo.getAdaptiveSupport(rendererIndex, groupIndex, false));
                  Log.d(LOGTAG, "    Group:" + groupIndex + ", adaptive_supported=" + adaptiveSupport + " [");
                  for (int trackIndex = 0; trackIndex < trackGroup.length; trackIndex++) {
                    String status = getTrackStatusString(trackSelection, trackGroup, trackIndex);
                    String formatSupport = getFormatSupportString(
                            mappedTrackInfo.getTrackFormatSupport(rendererIndex, groupIndex, trackIndex));
                    Log.d(LOGTAG, "      " + status + " Track:" + trackIndex +
                                  ", " + Format.toLogString(trackGroup.getFormat(trackIndex)) +
                                  ", supported=" + formatSupport);
                  }
                  Log.d(LOGTAG, "    ]");
                }
                Log.d(LOGTAG, "  ]");
              }
            }
            // Log tracks not associated with a renderer.
            TrackGroupArray unassociatedTrackGroups = mappedTrackInfo.getUnassociatedTrackGroups();
            if (unassociatedTrackGroups.length > 0) {
              Log.d(LOGTAG, "  Renderer:None [");
              for (int groupIndex = 0; groupIndex < unassociatedTrackGroups.length; groupIndex++) {
                Log.d(LOGTAG, "    Group:" + groupIndex + " [");
                TrackGroup trackGroup = unassociatedTrackGroups.get(groupIndex);
                for (int trackIndex = 0; trackIndex < trackGroup.length; trackIndex++) {
                  String status = getTrackStatusString(false);
                  String formatSupport = getFormatSupportString(
                          RendererCapabilities.FORMAT_UNSUPPORTED_TYPE);
                  Log.d(LOGTAG, "      " + status + " Track:" + trackIndex +
                                ", " + Format.toLogString(trackGroup.getFormat(trackIndex)) +
                                ", supported=" + formatSupport);
                }
                Log.d(LOGTAG, "    ]");
              }
              Log.d(LOGTAG, "  ]");
            }
            Log.d(LOGTAG, "]");
        }
        mTracksInfo = null;
        int numVideoTracks = 0;
        int numAudioTracks = 0;
        for (int j = 0; j < ignored.length; j++) {
            TrackGroup tg = ignored.get(j);
            for (int i = 0; i < tg.length; i++) {
                Format fmt = tg.getFormat(i);
                if (fmt.sampleMimeType != null) {
                    if (mRendererController.isVideoRendererEnabled() &&
                        fmt.sampleMimeType.startsWith(new String("video"))) {
                        numVideoTracks++;
                    } else if (mRendererController.isAudioRendererEnabled() &&
                               fmt.sampleMimeType.startsWith(new String("audio"))) {
                        numAudioTracks++;
                    }
                }
            }
        }
        mTracksInfo = new HlsMediaTracksInfo(numVideoTracks, numAudioTracks);
    }

    @Override
    public void onTimelineChanged(Timeline timeline, Object manifest) {
        // For now, we use the interface ExoPlayer.getDuration() for gecko,
        // so here we create local variable 'window' & 'peroid' to obtain
        // the dynamic duration.
        // See. http://google.github.io/ExoPlayer/doc/reference/com/google/android/exoplayer2/Timeline.html
        // for further information.
        Timeline.Window window = new Timeline.Window();
        mIsTimelineStatic = !timeline.isEmpty()
                && !timeline.getWindow(timeline.getWindowCount() - 1, window).isDynamic;

        int periodCount = timeline.getPeriodCount();
        int windowCount = timeline.getWindowCount();
        if (DEBUG) { Log.d(LOGTAG, "sourceInfo [periodCount=" + periodCount + ", windowCount=" + windowCount); }
        Timeline.Period period = new Timeline.Period();
        for (int i = 0; i < Math.min(periodCount, MAX_TIMELINE_ITEM_LINES); i++) {
          timeline.getPeriod(i, period);
          if (mDurationUs < period.getDurationUs()) {
              mDurationUs = period.getDurationUs();
          }
        }
        for (int i = 0; i < Math.min(windowCount, MAX_TIMELINE_ITEM_LINES); i++) {
          timeline.getWindow(i, window);
          if (mDurationUs < window.getDurationUs()) {
              mDurationUs = window.getDurationUs();
          }
        }
        // TODO : Need to check if the duration from play.getDuration is different
        // with the one calculated from multi-timelines/windows.
        if (DEBUG) {
            Log.d(LOGTAG, "Media duration (from Timeline) = " + mDurationUs +
                          "(us)" + " player.getDuration() = " + mPlayer.getDuration() +
                          "(ms)");
        }
    }

    private static String getStateString(int state) {
        switch (state) {
            case ExoPlayer.STATE_BUFFERING:
                return "B";
            case ExoPlayer.STATE_ENDED:
                return "E";
            case ExoPlayer.STATE_IDLE:
                return "I";
            case ExoPlayer.STATE_READY:
                return "R";
            default:
                return "?";
        }
    }

    private static String getFormatSupportString(int formatSupport) {
        switch (formatSupport) {
          case RendererCapabilities.FORMAT_HANDLED:
            return "YES";
          case RendererCapabilities.FORMAT_EXCEEDS_CAPABILITIES:
            return "NO_EXCEEDS_CAPABILITIES";
          case RendererCapabilities.FORMAT_UNSUPPORTED_SUBTYPE:
            return "NO_UNSUPPORTED_TYPE";
          case RendererCapabilities.FORMAT_UNSUPPORTED_TYPE:
            return "NO";
          default:
            return "?";
        }
      }

    private static String getAdaptiveSupportString(int trackCount, int adaptiveSupport) {
        if (trackCount < 2) {
          return "N/A";
        }
        switch (adaptiveSupport) {
          case RendererCapabilities.ADAPTIVE_SEAMLESS:
            return "YES";
          case RendererCapabilities.ADAPTIVE_NOT_SEAMLESS:
            return "YES_NOT_SEAMLESS";
          case RendererCapabilities.ADAPTIVE_NOT_SUPPORTED:
            return "NO";
          default:
            return "?";
        }
      }

      private static String getTrackStatusString(TrackSelection selection, TrackGroup group,
                                                 int trackIndex) {
        return getTrackStatusString(selection != null && selection.getTrackGroup() == group
                && selection.indexOf(trackIndex) != C.INDEX_UNSET);
      }

      private static String getTrackStatusString(boolean enabled) {
        return enabled ? "[X]" : "[ ]";
      }

    // =======================================================================
    // API for GeckoHLSResourceWrapper
    // =======================================================================
    @Override
    public synchronized void init(String url) {
        if (DEBUG) { Log.d(LOGTAG, " init"); }
        assertTrue(mResourceCallbacks != null);
        if (mIsPlayerInitDone) {
            return;
        }
        Context ctx = GeckoAppShell.getApplicationContext();
        mComponentListener = new ComponentListener();
        mComponentEventDispatcher = new ComponentEventDispatcher();
        mMainHandler = new Handler();

        mDurationUs = 0;

        // Prepare trackSelector
        TrackSelection.Factory videoTrackSelectionFactory =
                new AdaptiveTrackSelection.Factory(BANDWIDTH_METER);
        mTrackSelector = new DefaultTrackSelector(videoTrackSelectionFactory);

        // Prepare customized renderer
        mRenderers = new GeckoHlsRendererBase[2];
        mVRenderer = new GeckoHlsVideoRenderer(mComponentEventDispatcher);
        mARenderer = new GeckoHlsAudioRenderer(mComponentEventDispatcher);
        mRenderers[0] = mVRenderer;
        mRenderers[1] = mARenderer;

        // Create ExoPlayer instance with specific components.
        mPlayer = ExoPlayerFactory.newInstance(mRenderers, mTrackSelector);
        mPlayer.addListener(this);

        Uri uri = Uri.parse(url);
        mMediaDataSourceFactory = buildDataSourceFactory(ctx, BANDWIDTH_METER);
        mMediaSource = new HlsMediaSource(uri, mMediaDataSourceFactory, mMainHandler, null);
        if (DEBUG) {
            Log.d(LOGTAG, "Uri is " + uri +
                          ", ContentType is " + Util.inferContentType(uri.getLastPathSegment()));
        }

        mPlayer.prepare(mMediaSource);
        mIsPlayerInitDone = true;
    }

    @Override
    public boolean isLiveStream() {
        return !mIsTimelineStatic;
    }

    // =======================================================================
    // API for GeckoHLSDemuxerWrapper
    // =======================================================================
    @Override
    public ConcurrentLinkedQueue<GeckoHLSSample> getSamples(TrackType trackType,
                                                            int number) {
        if (trackType == TrackType.VIDEO) {
            return mVRenderer != null ? mVRenderer.getQueuedSamples(number) :
                                        new ConcurrentLinkedQueue<GeckoHLSSample>();
        } else if (trackType == TrackType.AUDIO) {
            return mARenderer != null ? mARenderer.getQueuedSamples(number) :
                                        new ConcurrentLinkedQueue<GeckoHLSSample>();
        } else {
            return new ConcurrentLinkedQueue<GeckoHLSSample>();
        }
    }

    @Override
    public long getDuration() {
        assertTrue(mPlayer != null);
        if (isLiveStream()) {
            return 0L;
        }
        // Value returned by getDuration() is in milliseconds.
        long duration = mPlayer.getDuration() * 1000;
        if (DEBUG) { Log.d(LOGTAG, "getDuration : " + duration  + "(Us)"); }
        return duration;
    }

    @Override
    public long getBufferedPosition() {
        assertTrue(mPlayer != null);
        // Value returned by getBufferedPosition() is in milliseconds.
        long bufferedPos = Math.max(0L, mPlayer.getBufferedPosition() * 1000L);
        if (DEBUG) { Log.d(LOGTAG, "getBufferedPosition : " + bufferedPos + "(Us)"); }
        return bufferedPos;
    }

    @Override
    public synchronized int getNumberOfTracks(TrackType trackType) {
        if (DEBUG) { Log.d(LOGTAG, "getNumberOfTracks : type " + trackType); }
        assertTrue(mTracksInfo != null);

        if (trackType == TrackType.VIDEO) {
            return mTracksInfo.getNumOfVideoTracks();
        } else if (trackType == TrackType.AUDIO) {
            return mTracksInfo.getNumOfAudioTracks();
        }
        return 0;
    }

    @Override
    public GeckoVideoInfo getVideoInfo(int index) {
        if (DEBUG) { Log.d(LOGTAG, "getVideoInfo"); }
        assertTrue(mVRenderer != null);
        assertTrue(mTracksInfo != null);
        if (!mTracksInfo.hasVideo()) {
            return null;
        }
        Format fmt = mVRenderer.getFormat(index);
        if (fmt == null) {
            return null;
        }
        GeckoVideoInfo vInfo = new GeckoVideoInfo(fmt.width, fmt.height,
                                                  fmt.width, fmt.height,
                                                  fmt.rotationDegrees, fmt.stereoMode,
                                                  getDuration(), fmt.sampleMimeType,
                                                  null, null);
        return vInfo;
    }

    @Override
    public GeckoAudioInfo getAudioInfo(int index) {
        if (DEBUG) { Log.d(LOGTAG, "getAudioInfo"); }
        assertTrue(mARenderer != null);
        assertTrue(mTracksInfo != null);
        if (!mTracksInfo.hasAudio()) {
            return null;
        }
        Format fmt = mARenderer.getFormat(index);
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
                                                  16, 0, getDuration(),
                                                  fmt.sampleMimeType, csd);
        return aInfo;
    }

    @Override
    public boolean seek(long positionUs) {
        // positionUs : microseconds.
        // NOTE : 1) It's not possible to seek media by tracktype via ExoPlayer Interface.
        //        2) positionUs is samples PTS from MFR, we need to re-adjust it
        //           for ExoPlayer by subtracting sample start time.
        //        3) Time unit for ExoPlayer.seek() is milliseconds.
        try {
            // TODO : Gather Timeline Period / Window information to develop
            //        complete timeline, and seekTime should be inside the duration.
            Long startTime = Long.MAX_VALUE;
            for (GeckoHlsRendererBase r : mRenderers) {
                if (r == mVRenderer && mRendererController.isVideoRendererEnabled() ||
                    r == mARenderer && mRendererController.isAudioRendererEnabled()) {
                // Find the min value of the start time
                    startTime = Math.min(startTime, r.getFirstSamplePTS());
                }
            }
            if (DEBUG) {
                Log.d(LOGTAG, "seeking  : " + positionUs / 1000 +
                              " (ms); startTime : " + startTime / 1000 + " (ms)");
            }
            assertTrue(startTime != Long.MAX_VALUE);
            mPlayer.seekTo(positionUs / 1000 - startTime / 1000);
        } catch (Exception e) {
            mDemuxerCallbacks.onError(DemuxerError.UNKNOWN.code());
            return false;
        }
        return true;
    }

    @Override
    public long getNextKeyFrameTime() {
        long nextKeyFrameTime = mVRenderer != null
            ? mVRenderer.getNextKeyFrameTime()
            : Long.MAX_VALUE;
        return nextKeyFrameTime;
    }

    @Override
    public synchronized void release() {
        if (DEBUG) { Log.d(LOGTAG, "releasing  ... id : " + mPlayerId); }
        if (mPlayer != null) {
            mPlayer.removeListener(this);
            mPlayer.stop();
            mPlayer.release();
            mVRenderer = null;
            mARenderer = null;
            mPlayer = null;
        }
        mDemuxerCallbacks = null;
        mResourceCallbacks = null;
        mIsPlayerInitDone = false;
        mIsDemuxerInitDone = false;
    }
}