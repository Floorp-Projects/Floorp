/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicInteger;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.geckoview.BuildConfig;
import org.mozilla.thirdparty.com.google.android.exoplayer2.C;
import org.mozilla.thirdparty.com.google.android.exoplayer2.DefaultLoadControl;
import org.mozilla.thirdparty.com.google.android.exoplayer2.ExoPlaybackException;
import org.mozilla.thirdparty.com.google.android.exoplayer2.ExoPlayer;
import org.mozilla.thirdparty.com.google.android.exoplayer2.Format;
import org.mozilla.thirdparty.com.google.android.exoplayer2.PlaybackParameters;
import org.mozilla.thirdparty.com.google.android.exoplayer2.RendererCapabilities;
import org.mozilla.thirdparty.com.google.android.exoplayer2.Timeline;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSourceEventListener;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.TrackGroup;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.TrackGroupArray;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.hls.HlsMediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.AdaptiveTrackSelection;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.MappingTrackSelector.MappedTrackInfo;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.TrackSelection;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.TrackSelectionArray;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DefaultAllocator;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DefaultBandwidthMeter;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DefaultDataSourceFactory;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DefaultHttpDataSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DefaultHttpDataSourceFactory;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.HttpDataSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.MimeTypes;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Util;

@ReflectionTarget
public class GeckoHlsPlayer implements BaseHlsPlayer, ExoPlayer.EventListener {
  private static final String LOGTAG = "GeckoHlsPlayer";
  private static final DefaultBandwidthMeter BANDWIDTH_METER =
      new DefaultBandwidthMeter.Builder(null).build();
  private static final int MAX_TIMELINE_ITEM_LINES = 3;
  private static final boolean DEBUG = !BuildConfig.MOZILLA_OFFICIAL;

  private static final AtomicInteger sPlayerId = new AtomicInteger(0);
  /*
   *  Because we treat GeckoHlsPlayer as a source data provider.
   *  It will be created and initialized with a URL by HLSResource in
   *  Gecko media pipleine (in cpp). Once HLSDemuxer is created later, we
   *  need to bridge this HLSResource to the created demuxer. And they share
   *  the same GeckoHlsPlayer.
   *  mPlayerId is a token used for Gecko media pipeline to obtain corresponding player.
   */
  private final int mPlayerId;
  // Accessed only in GeckoHlsPlayerThread.
  private boolean mExoplayerSuspended = false;

  private static final int DEFAULT_MIN_BUFFER_MS = 5 * 1000;
  private static final int DEFAULT_MAX_BUFFER_MS = 10 * 1000;

  private enum MediaDecoderPlayState {
    PLAY_STATE_PREPARING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_PLAYING
  }
  // Default value is PLAY_STATE_PREPARING and it will be set to PLAY_STATE_PLAYING
  // once HTMLMediaElement calls PlayInternal().
  // Accessed only in GeckoHlsPlayerThread.
  private MediaDecoderPlayState mMediaDecoderPlayState = MediaDecoderPlayState.PLAY_STATE_PREPARING;

  private Handler mMainHandler;
  private HandlerThread mThread;
  private ExoPlayer mPlayer;
  private GeckoHlsRendererBase[] mRenderers;
  private DefaultTrackSelector mTrackSelector;
  private MediaSource mMediaSource;
  private SourceEventListener mSourceEventListener;
  private ComponentListener mComponentListener;
  private ComponentEventDispatcher mComponentEventDispatcher;

  private volatile boolean mIsTimelineStatic = false;
  private long mDurationUs;

  private GeckoHlsVideoRenderer mVRenderer = null;
  private GeckoHlsAudioRenderer mARenderer = null;

  // Able to control if we only want V/A/V+A tracks from bitstream.
  private class RendererController {
    private final boolean mEnableV;
    private final boolean mEnableA;

    RendererController(final boolean enableVideoRenderer, final boolean enableAudioRenderer) {
      this.mEnableV = enableVideoRenderer;
      this.mEnableA = enableAudioRenderer;
    }

    boolean isVideoRendererEnabled() {
      return mEnableV;
    }

    boolean isAudioRendererEnabled() {
      return mEnableA;
    }
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

    HlsMediaTracksInfo() {}

    public void reset() {
      mNumVideoTracks = 0;
      mNumAudioTracks = 0;
      mVideoInfoUpdated = false;
      mAudioInfoUpdated = false;
      mVideoDataArrived = false;
      mAudioDataArrived = false;
    }

    public void updateNumOfVideoTracks(final int numOfTracks) {
      mNumVideoTracks = numOfTracks;
    }

    public void updateNumOfAudioTracks(final int numOfTracks) {
      mNumAudioTracks = numOfTracks;
    }

    public boolean hasVideo() {
      return mNumVideoTracks > 0;
    }

    public boolean hasAudio() {
      return mNumAudioTracks > 0;
    }

    public int getNumOfVideoTracks() {
      return mNumVideoTracks;
    }

    public int getNumOfAudioTracks() {
      return mNumAudioTracks;
    }

    public void onVideoInfoUpdated() {
      mVideoInfoUpdated = true;
    }

    public void onAudioInfoUpdated() {
      mAudioInfoUpdated = true;
    }

    public void onDataArrived(final int trackType) {
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

  private HlsMediaTracksInfo mTracksInfo = new HlsMediaTracksInfo();

  // Used only in GeckoHlsPlayerThread.
  private boolean mIsPlayerInitDone = false;
  private boolean mIsDemuxerInitDone = false;
  private BaseHlsPlayer.DemuxerCallbacks mDemuxerCallbacks;
  private BaseHlsPlayer.ResourceCallbacks mResourceCallbacks;

  private boolean mReleasing = false; // Used only in Gecko Main thread.

  private static void assertTrue(final boolean condition) {
    if (DEBUG && !condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }

  protected void checkInitDone() {
    if (mIsDemuxerInitDone) {
      return;
    }
    assertTrue(mDemuxerCallbacks != null);

    if (DEBUG) {
      Log.d(
          LOGTAG,
          "[checkInitDone] VReady:"
              + mTracksInfo.videoReady()
              + ",AReady:"
              + mTracksInfo.audioReady()
              + ",hasV:"
              + mTracksInfo.hasVideo()
              + ",hasA:"
              + mTracksInfo.hasAudio());
    }
    if (mTracksInfo.videoReady() && mTracksInfo.audioReady()) {
      if (mDemuxerCallbacks != null) {
        mDemuxerCallbacks.onInitialized(mTracksInfo.hasAudio(), mTracksInfo.hasVideo());
      }
      mIsDemuxerInitDone = true;
    }
  }

  private final class SourceEventListener implements MediaSourceEventListener {
    public void onLoadStarted(
        final int windowIndex,
        final MediaSource.MediaPeriodId mediaPeriodId,
        final LoadEventInfo loadEventInfo,
        final MediaLoadData mediaLoadData) {
      assertTrue(isPlayerThread());

      synchronized (GeckoHlsPlayer.this) {
        if (mediaLoadData.dataType != C.DATA_TYPE_MEDIA) {
          // Don't report non-media URLs.
          return;
        }
        if (mResourceCallbacks == null || loadEventInfo.uri == null || mReleasing) {
          return;
        }

        if (DEBUG) {
          Log.d(LOGTAG, "on-load: url=" + loadEventInfo.uri);
        }
        mResourceCallbacks.onLoad(loadEventInfo.uri.toString());
      }
    }
  }

  public final class ComponentEventDispatcher {
    // Called from GeckoHls{Audio,Video}Renderer/ExoPlayer internal playback thread
    // or GeckoHlsPlayerThread.
    public void onDataArrived(final int trackType) {
      assertTrue(mComponentListener != null);

      if (mComponentListener != null) {
        runOnPlayerThread(() -> mComponentListener.onDataArrived(trackType));
      }
    }

    // Called from GeckoHls{Audio,Video}Renderer internal playback thread.
    public void onVideoInputFormatChanged(final Format format) {
      assertTrue(mComponentListener != null);

      if (mComponentListener != null) {
        runOnPlayerThread(() -> mComponentListener.onVideoInputFormatChanged(format));
      }
    }

    // Called from GeckoHls{Audio,Video}Renderer internal playback thread.
    public void onAudioInputFormatChanged(final Format format) {
      assertTrue(mComponentListener != null);

      if (mComponentListener != null) {
        runOnPlayerThread(() -> mComponentListener.onAudioInputFormatChanged(format));
      }
    }
  }

  public final class ComponentListener {

    // General purpose implementation
    // Called on GeckoHlsPlayerThread
    public void onDataArrived(final int trackType) {
      assertTrue(isPlayerThread());

      synchronized (GeckoHlsPlayer.this) {
        if (DEBUG) {
          Log.d(LOGTAG, "[CB][onDataArrived] id " + mPlayerId);
        }
        if (!mIsPlayerInitDone) {
          return;
        }

        mTracksInfo.onDataArrived(trackType);
        if (!mReleasing) {
          mResourceCallbacks.onDataArrived();
        }
        checkInitDone();
      }
    }

    // Called on GeckoHlsPlayerThread
    public void onVideoInputFormatChanged(final Format format) {
      assertTrue(isPlayerThread());

      synchronized (GeckoHlsPlayer.this) {
        if (DEBUG) {
          Log.d(LOGTAG, "[CB] onVideoInputFormatChanged [" + format + "]");
          Log.d(
              LOGTAG,
              "[CB] SampleMIMEType ["
                  + format.sampleMimeType
                  + "], ContainerMIMEType ["
                  + format.containerMimeType
                  + "], id : "
                  + mPlayerId);
        }
        if (!mIsPlayerInitDone) {
          return;
        }
        mTracksInfo.onVideoInfoUpdated();
        checkInitDone();
      }
    }

    // Called on GeckoHlsPlayerThread
    public void onAudioInputFormatChanged(final Format format) {
      assertTrue(isPlayerThread());

      synchronized (GeckoHlsPlayer.this) {
        if (DEBUG) {
          Log.d(LOGTAG, "[CB] onAudioInputFormatChanged [" + format + "], mPlayerId :" + mPlayerId);
        }
        if (!mIsPlayerInitDone) {
          return;
        }
        mTracksInfo.onAudioInfoUpdated();
        checkInitDone();
      }
    }
  }

  private HlsMediaSource.Factory buildDataSourceFactory(
      final Context ctx, final DefaultBandwidthMeter bandwidthMeter) {
    return new HlsMediaSource.Factory(
        new DefaultDataSourceFactory(
            ctx, bandwidthMeter, buildHttpDataSourceFactory(bandwidthMeter)));
  }

  private HttpDataSource.Factory buildHttpDataSourceFactory(
      final DefaultBandwidthMeter bandwidthMeter) {
    return new DefaultHttpDataSourceFactory(
        BuildConfig.USER_AGENT_GECKOVIEW_MOBILE,
        bandwidthMeter /* listener */,
        DefaultHttpDataSource.DEFAULT_CONNECT_TIMEOUT_MILLIS,
        DefaultHttpDataSource.DEFAULT_READ_TIMEOUT_MILLIS,
        true /* allowCrossProtocolRedirects */);
  }

  private long getDuration() {
    return awaitPlayerThread(
        () -> {
          long duration = 0L;
          // Value returned by getDuration() is in milliseconds.
          if (mPlayer != null && !isLiveStream()) {
            duration = Math.max(0L, mPlayer.getDuration() * 1000L);
          }
          if (DEBUG) {
            Log.d(LOGTAG, "getDuration : " + duration + "(Us)");
          }
          return duration;
        });
  }

  // To make sure that each player has a unique id, GeckoHlsPlayer should be
  // created only from synchronized APIs in GeckoPlayerFactory.
  public GeckoHlsPlayer() {
    mPlayerId = sPlayerId.incrementAndGet();
    if (DEBUG) {
      Log.d(LOGTAG, " construct player with id(" + mPlayerId + ")");
    }
  }

  // Should be only called by GeckoPlayerFactory and GeckoHLSResourceWrapper.
  // The mPlayerId is used to make sure that the same GeckoHlsPlayer is used by
  // corresponding HLSResource and HLSDemuxer for each media playback.
  // Called on Gecko's main thread
  @Override
  public int getId() {
    return mPlayerId;
  }

  // Called on Gecko's main thread
  @Override
  public synchronized void addDemuxerWrapperCallbackListener(
      final BaseHlsPlayer.DemuxerCallbacks callback) {
    if (DEBUG) {
      Log.d(LOGTAG, " addDemuxerWrapperCallbackListener ...");
    }
    mDemuxerCallbacks = callback;
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public synchronized void onLoadingChanged(final boolean isLoading) {
    assertTrue(isPlayerThread());

    if (DEBUG) {
      Log.d(LOGTAG, "loading [" + isLoading + "]");
    }
    if (!isLoading) {
      if (mMediaDecoderPlayState != MediaDecoderPlayState.PLAY_STATE_PLAYING) {
        suspendExoplayer();
      }
      // To update buffered position.
      mComponentEventDispatcher.onDataArrived(C.TRACK_TYPE_DEFAULT);
    }
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public synchronized void onPlayerStateChanged(final boolean playWhenReady, final int state) {
    assertTrue(isPlayerThread());

    if (DEBUG) {
      Log.d(LOGTAG, "state [" + playWhenReady + ", " + getStateString(state) + "]");
    }
    if (state == ExoPlayer.STATE_READY
        && !mExoplayerSuspended
        && mMediaDecoderPlayState == MediaDecoderPlayState.PLAY_STATE_PLAYING) {
      resumeExoplayer();
    }
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public void onPositionDiscontinuity(final int reason) {
    assertTrue(isPlayerThread());

    if (DEBUG) {
      Log.d(LOGTAG, "positionDiscontinuity: reason=" + reason);
    }
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public void onPlaybackParametersChanged(final PlaybackParameters playbackParameters) {
    assertTrue(isPlayerThread());

    if (DEBUG) {
      Log.d(
          LOGTAG,
          "playbackParameters "
              + String.format(
                  "[speed=%.2f, pitch=%.2f]", playbackParameters.speed, playbackParameters.pitch));
    }
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public synchronized void onPlayerError(final ExoPlaybackException e) {
    assertTrue(isPlayerThread());

    if (DEBUG) {
      Log.e(LOGTAG, "playerFailed", e);
    }
    mIsPlayerInitDone = false;
    if (mReleasing) {
      return;
    }
    if (mResourceCallbacks != null) {
      mResourceCallbacks.onError(ResourceError.PLAYER.code());
    }
    if (mDemuxerCallbacks != null) {
      mDemuxerCallbacks.onError(DemuxerError.PLAYER.code());
    }
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public synchronized void onTracksChanged(
      final TrackGroupArray ignored, final TrackSelectionArray trackSelections) {
    assertTrue(isPlayerThread());

    if (DEBUG) {
      Log.d(LOGTAG, "onTracksChanged : TGA[" + ignored + "], TSA[" + trackSelections + "]");

      final MappedTrackInfo mappedTrackInfo = mTrackSelector.getCurrentMappedTrackInfo();
      if (mappedTrackInfo == null) {
        Log.d(LOGTAG, "Tracks []");
        return;
      }
      Log.d(LOGTAG, "Tracks [");
      // Log tracks associated to renderers.
      for (int rendererIndex = 0; rendererIndex < mappedTrackInfo.length; rendererIndex++) {
        final TrackGroupArray rendererTrackGroups = mappedTrackInfo.getTrackGroups(rendererIndex);
        final TrackSelection trackSelection = trackSelections.get(rendererIndex);
        if (rendererTrackGroups.length > 0) {
          Log.d(LOGTAG, "  Renderer:" + rendererIndex + " [");
          for (int groupIndex = 0; groupIndex < rendererTrackGroups.length; groupIndex++) {
            final TrackGroup trackGroup = rendererTrackGroups.get(groupIndex);
            final String adaptiveSupport =
                getAdaptiveSupportString(
                    trackGroup.length,
                    mappedTrackInfo.getAdaptiveSupport(rendererIndex, groupIndex, false));
            Log.d(
                LOGTAG,
                "    Group:" + groupIndex + ", adaptive_supported=" + adaptiveSupport + " [");
            for (int trackIndex = 0; trackIndex < trackGroup.length; trackIndex++) {
              final String status = getTrackStatusString(trackSelection, trackGroup, trackIndex);
              final String formatSupport =
                  getFormatSupportString(
                      mappedTrackInfo.getTrackFormatSupport(rendererIndex, groupIndex, trackIndex));
              Log.d(
                  LOGTAG,
                  "      "
                      + status
                      + " Track:"
                      + trackIndex
                      + ", "
                      + Format.toLogString(trackGroup.getFormat(trackIndex))
                      + ", supported="
                      + formatSupport);
            }
            Log.d(LOGTAG, "    ]");
          }
          Log.d(LOGTAG, "  ]");
        }
      }
      // Log tracks not associated with a renderer.
      final TrackGroupArray unassociatedTrackGroups = mappedTrackInfo.getUnassociatedTrackGroups();
      if (unassociatedTrackGroups.length > 0) {
        Log.d(LOGTAG, "  Renderer:None [");
        for (int groupIndex = 0; groupIndex < unassociatedTrackGroups.length; groupIndex++) {
          Log.d(LOGTAG, "    Group:" + groupIndex + " [");
          final TrackGroup trackGroup = unassociatedTrackGroups.get(groupIndex);
          for (int trackIndex = 0; trackIndex < trackGroup.length; trackIndex++) {
            final String status = getTrackStatusString(false);
            final String formatSupport =
                getFormatSupportString(RendererCapabilities.FORMAT_UNSUPPORTED_TYPE);
            Log.d(
                LOGTAG,
                "      "
                    + status
                    + " Track:"
                    + trackIndex
                    + ", "
                    + Format.toLogString(trackGroup.getFormat(trackIndex))
                    + ", supported="
                    + formatSupport);
          }
          Log.d(LOGTAG, "    ]");
        }
        Log.d(LOGTAG, "  ]");
      }
      Log.d(LOGTAG, "]");
    }
    mTracksInfo.reset();
    int numVideoTracks = 0;
    int numAudioTracks = 0;
    for (int j = 0; j < ignored.length; j++) {
      final TrackGroup tg = ignored.get(j);
      for (int i = 0; i < tg.length; i++) {
        final Format fmt = tg.getFormat(i);
        if (fmt.sampleMimeType != null) {
          if (mRendererController.isVideoRendererEnabled()
              && fmt.sampleMimeType.startsWith(new String("video"))) {
            numVideoTracks++;
          } else if (mRendererController.isAudioRendererEnabled()
              && fmt.sampleMimeType.startsWith(new String("audio"))) {
            numAudioTracks++;
          }
        }
      }
    }
    mTracksInfo.updateNumOfVideoTracks(numVideoTracks);
    mTracksInfo.updateNumOfAudioTracks(numAudioTracks);
  }

  // Called on GeckoHlsPlayerThread from ExoPlayer
  @Override
  public synchronized void onTimelineChanged(final Timeline timeline, final int reason) {
    assertTrue(isPlayerThread());

    // For now, we use the interface ExoPlayer.getDuration() for gecko,
    // so here we create local variable 'window' & 'peroid' to obtain
    // the dynamic duration.
    // See.
    // http://google.github.io/ExoPlayer/doc/reference/com/google/android/exoplayer2/Timeline.html
    // for further information.
    final Timeline.Window window = new Timeline.Window();
    mIsTimelineStatic =
        !timeline.isEmpty() && !timeline.getWindow(timeline.getWindowCount() - 1, window).isDynamic;

    final int periodCount = timeline.getPeriodCount();
    final int windowCount = timeline.getWindowCount();
    if (DEBUG) {
      Log.d(LOGTAG, "sourceInfo [periodCount=" + periodCount + ", windowCount=" + windowCount);
    }
    final Timeline.Period period = new Timeline.Period();
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
      Log.d(
          LOGTAG,
          "Media duration (from Timeline) = "
              + mDurationUs
              + "(us)"
              + " player.getDuration() = "
              + mPlayer.getDuration()
              + "(ms)");
    }
  }

  private static String getStateString(final int state) {
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

  private static String getFormatSupportString(final int formatSupport) {
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

  private static String getAdaptiveSupportString(final int trackCount, final int adaptiveSupport) {
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

  private static String getTrackStatusString(
      final TrackSelection selection, final TrackGroup group, final int trackIndex) {
    return getTrackStatusString(
        selection != null
            && selection.getTrackGroup() == group
            && selection.indexOf(trackIndex) != C.INDEX_UNSET);
  }

  private static String getTrackStatusString(final boolean enabled) {
    return enabled ? "[X]" : "[ ]";
  }

  // Called on GeckoHlsPlayerThread
  private void createExoPlayer(final String url) {
    assertTrue(isPlayerThread());

    final Context ctx = GeckoAppShell.getApplicationContext();
    mComponentListener = new ComponentListener();
    mComponentEventDispatcher = new ComponentEventDispatcher();
    mDurationUs = 0;

    // Prepare trackSelector
    final TrackSelection.Factory videoTrackSelectionFactory =
        new AdaptiveTrackSelection.Factory(BANDWIDTH_METER);
    mTrackSelector = new DefaultTrackSelector(videoTrackSelectionFactory);

    // Prepare customized renderer
    mRenderers = new GeckoHlsRendererBase[2];
    mVRenderer = new GeckoHlsVideoRenderer(mComponentEventDispatcher);
    mARenderer = new GeckoHlsAudioRenderer(mComponentEventDispatcher);
    mRenderers[0] = mVRenderer;
    mRenderers[1] = mARenderer;

    final DefaultLoadControl dlc =
        new DefaultLoadControl.Builder()
            .setAllocator(new DefaultAllocator(true, C.DEFAULT_BUFFER_SEGMENT_SIZE))
            .setBufferDurationsMs(
                DEFAULT_MIN_BUFFER_MS,
                DEFAULT_MAX_BUFFER_MS,
                DefaultLoadControl.DEFAULT_BUFFER_FOR_PLAYBACK_MS,
                DefaultLoadControl.DEFAULT_BUFFER_FOR_PLAYBACK_AFTER_REBUFFER_MS)
            .createDefaultLoadControl();
    // Create ExoPlayer instance with specific components.
    mPlayer =
        new ExoPlayer.Builder(ctx, mRenderers)
            .setTrackSelector(mTrackSelector)
            .setLoadControl(dlc)
            .build();
    mPlayer.addListener(this);

    final Uri uri = Uri.parse(url);
    mMediaSource = buildDataSourceFactory(ctx, BANDWIDTH_METER).createMediaSource(uri);
    mSourceEventListener = new SourceEventListener();
    mMediaSource.addEventListener(mMainHandler, mSourceEventListener);
    if (DEBUG) {
      Log.d(
          LOGTAG,
          "Uri is " + uri + ", ContentType is " + Util.inferContentType(uri.getLastPathSegment()));
    }
    mPlayer.setPlayWhenReady(false);
    mPlayer.prepare(mMediaSource);
    mIsPlayerInitDone = true;
  }
  // =======================================================================
  // API for GeckoHLSResourceWrapper
  // =======================================================================
  // Called on Gecko Main Thread
  @Override
  public synchronized void init(final String url, final BaseHlsPlayer.ResourceCallbacks callback) {
    if (DEBUG) {
      Log.d(LOGTAG, " init");
    }
    assertTrue(callback != null);
    assertTrue(!mIsPlayerInitDone);

    mThread = new HandlerThread("GeckoHlsPlayerThread");
    mThread.start();
    mMainHandler = new Handler(mThread.getLooper());

    mMainHandler.post(
        () -> {
          mResourceCallbacks = callback;
          createExoPlayer(url);
        });
  }

  // Called on MDSM's TaskQueue
  @Override
  public boolean isLiveStream() {
    return !mIsTimelineStatic;
  }
  // =======================================================================
  // API for GeckoHLSDemuxerWrapper
  // =======================================================================
  // Called on HLSDemuxer's TaskQueue
  @Override
  public synchronized ConcurrentLinkedQueue<GeckoHLSSample> getSamples(
      final TrackType trackType, final int number) {
    if (trackType == TrackType.VIDEO) {
      return mVRenderer != null
          ? mVRenderer.getQueuedSamples(number)
          : new ConcurrentLinkedQueue<GeckoHLSSample>();
    } else if (trackType == TrackType.AUDIO) {
      return mARenderer != null
          ? mARenderer.getQueuedSamples(number)
          : new ConcurrentLinkedQueue<GeckoHLSSample>();
    } else {
      return new ConcurrentLinkedQueue<GeckoHLSSample>();
    }
  }

  // Called on MFR's TaskQueue
  @Override
  public long getBufferedPosition() {
    return awaitPlayerThread(
        () -> {
          // Value returned by getBufferedPosition() is in milliseconds.
          final long bufferedPos =
              mPlayer == null ? 0L : Math.max(0L, mPlayer.getBufferedPosition() * 1000L);
          if (DEBUG) {
            Log.d(LOGTAG, "getBufferedPosition : " + bufferedPos + "(Us)");
          }
          return bufferedPos;
        });
  }

  // Called on MFR's TaskQueue
  @Override
  public synchronized int getNumberOfTracks(final TrackType trackType) {
    if (DEBUG) {
      Log.d(LOGTAG, "getNumberOfTracks : type " + trackType);
    }
    if (trackType == TrackType.VIDEO) {
      return mTracksInfo.getNumOfVideoTracks();
    } else if (trackType == TrackType.AUDIO) {
      return mTracksInfo.getNumOfAudioTracks();
    }
    return 0;
  }

  // Called on MFR's TaskQueue
  @Override
  public GeckoVideoInfo getVideoInfo(final int index) {
    final Format fmt;
    synchronized (this) {
      if (DEBUG) {
        Log.d(LOGTAG, "getVideoInfo");
      }
      if (mVRenderer == null) {
        Log.e(LOGTAG, "no render to get video info from. Index : " + index);
        return null;
      }
      if (!mTracksInfo.hasVideo()) {
        return null;
      }
      fmt = mVRenderer.getFormat(index);
      if (fmt == null) {
        return null;
      }
    }
    final GeckoVideoInfo vInfo =
        new GeckoVideoInfo(
            fmt.width,
            fmt.height,
            fmt.width,
            fmt.height,
            fmt.rotationDegrees,
            fmt.stereoMode,
            getDuration(),
            fmt.sampleMimeType,
            null,
            null);
    return vInfo;
  }

  // Called on MFR's TaskQueue
  @Override
  public GeckoAudioInfo getAudioInfo(final int index) {
    final Format fmt;
    synchronized (this) {
      if (DEBUG) {
        Log.d(LOGTAG, "getAudioInfo");
      }
      if (mARenderer == null) {
        Log.e(LOGTAG, "no render to get audio info from. Index : " + index);
        return null;
      }
      if (!mTracksInfo.hasAudio()) {
        return null;
      }
      fmt = mARenderer.getFormat(index);
      if (fmt == null) {
        return null;
      }
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
    final byte[] csd = fmt.initializationData.isEmpty() ? null : fmt.initializationData.get(0);
    final GeckoAudioInfo aInfo =
        new GeckoAudioInfo(
            fmt.sampleRate, fmt.channelCount, 16, 0, getDuration(), fmt.sampleMimeType, csd);
    return aInfo;
  }

  // Called on HLSDemuxer's TaskQueue
  @Override
  public boolean seek(final long positionUs) {
    synchronized (this) {
      if (mPlayer == null) {
        Log.d(LOGTAG, "Seek operation won't be performed as no player exists!");
        return false;
      }
    }
    return awaitPlayerThread(
        () -> {
          // Need to temporarily resume Exoplayer to download the chunks for getting the demuxed
          // keyframe sample when HTMLMediaElement is paused. Suspend Exoplayer when collecting
          // enough
          // samples in onLoadingChanged.
          if (mExoplayerSuspended) {
            resumeExoplayer();
          }
          // positionUs : microseconds.
          // NOTE : 1) It's not possible to seek media by tracktype via ExoPlayer Interface.
          //        2) positionUs is samples PTS from MFR, we need to re-adjust it
          //           for ExoPlayer by subtracting sample start time.
          //        3) Time unit for ExoPlayer.seek() is milliseconds.
          try {
            // TODO : Gather Timeline Period / Window information to develop
            //        complete timeline, and seekTime should be inside the duration.
            Long startTime = Long.MAX_VALUE;
            for (final GeckoHlsRendererBase r : mRenderers) {
              if (r == mVRenderer
                      && mRendererController.isVideoRendererEnabled()
                      && mTracksInfo.hasVideo()
                  || r == mARenderer
                      && mRendererController.isAudioRendererEnabled()
                      && mTracksInfo.hasAudio()) {
                // Find the min value of the start time
                startTime = Math.min(startTime, r.getFirstSamplePTS());
              }
            }
            if (DEBUG) {
              Log.d(
                  LOGTAG,
                  "seeking  : "
                      + positionUs / 1000
                      + " (ms); startTime : "
                      + startTime / 1000
                      + " (ms)");
            }
            assertTrue(startTime != Long.MAX_VALUE && startTime != Long.MIN_VALUE);
            mPlayer.seekTo(positionUs / 1000 - startTime / 1000);
          } catch (final Exception e) {
            if (mReleasing) {
              return false;
            }
            if (mDemuxerCallbacks != null) {
              mDemuxerCallbacks.onError(DemuxerError.UNKNOWN.code());
            }
            return false;
          }
          return true;
        });
  }

  // Called on HLSDemuxer's TaskQueue
  @Override
  public synchronized long getNextKeyFrameTime() {
    final long nextKeyFrameTime =
        mVRenderer != null ? mVRenderer.getNextKeyFrameTime() : Long.MAX_VALUE;
    return nextKeyFrameTime;
  }

  // Called on Gecko's main thread.
  @Override
  public synchronized void suspend() {
    runOnPlayerThread(
        () -> {
          if (mExoplayerSuspended) {
            return;
          }
          if (mMediaDecoderPlayState != MediaDecoderPlayState.PLAY_STATE_PLAYING) {
            if (DEBUG) {
              Log.d(LOGTAG, "suspend player id : " + mPlayerId);
            }
            suspendExoplayer();
          }
        });
  }

  // Called on Gecko's main thread.
  @Override
  public synchronized void resume() {
    runOnPlayerThread(
        () -> {
          if (!mExoplayerSuspended) {
            return;
          }
          if (mMediaDecoderPlayState == MediaDecoderPlayState.PLAY_STATE_PLAYING) {
            if (DEBUG) {
              Log.d(LOGTAG, "resume player id : " + mPlayerId);
            }
            resumeExoplayer();
          }
        });
  }

  // Called on Gecko's main thread.
  @Override
  public synchronized void play() {
    runOnPlayerThread(
        () -> {
          if (mMediaDecoderPlayState == MediaDecoderPlayState.PLAY_STATE_PLAYING) {
            return;
          }
          if (DEBUG) {
            Log.d(LOGTAG, "MediaDecoder played.");
          }
          mMediaDecoderPlayState = MediaDecoderPlayState.PLAY_STATE_PLAYING;
          resumeExoplayer();
        });
  }

  // Called on Gecko's main thread.
  @Override
  public synchronized void pause() {
    runOnPlayerThread(
        () -> {
          if (mMediaDecoderPlayState != MediaDecoderPlayState.PLAY_STATE_PLAYING) {
            return;
          }
          if (DEBUG) {
            Log.d(LOGTAG, "MediaDecoder paused.");
          }
          mMediaDecoderPlayState = MediaDecoderPlayState.PLAY_STATE_PAUSED;
          suspendExoplayer();
        });
  }

  private void suspendExoplayer() {
    assertTrue(isPlayerThread());

    if (mPlayer == null) {
      return;
    }
    mExoplayerSuspended = true;
    if (DEBUG) {
      Log.d(LOGTAG, "suspend Exoplayer");
    }
    mPlayer.setPlayWhenReady(false);
  }

  private void resumeExoplayer() {
    assertTrue(isPlayerThread());

    if (mPlayer == null) {
      return;
    }
    mExoplayerSuspended = false;
    if (DEBUG) {
      Log.d(LOGTAG, "resume Exoplayer");
    }
    mPlayer.setPlayWhenReady(true);
  }

  // Called on Gecko's main thread, when HLSDemuxer or HLSResource destructs.
  @Override
  public void release() {
    if (DEBUG) {
      Log.d(LOGTAG, "releasing  ... id : " + mPlayerId);
    }

    synchronized (this) {
      if (mReleasing) {
        return;
      } else {
        mReleasing = true;
      }
    }

    runOnPlayerThread(
        () -> {
          if (mPlayer != null) {
            mPlayer.removeListener(this);
            mPlayer.stop();
            mPlayer.release();
            mVRenderer = null;
            mARenderer = null;
            mPlayer = null;
          }
          if (mThread != null) {
            mThread.quit();
            mThread = null;
          }
          mDemuxerCallbacks = null;
          mResourceCallbacks = null;
          mIsPlayerInitDone = false;
          mIsDemuxerInitDone = false;
        });
  }

  private void runOnPlayerThread(final Runnable task) {
    assertTrue(mMainHandler != null);
    if (isPlayerThread()) {
      task.run();
    } else {
      mMainHandler.post(task);
    }
  }

  private boolean isPlayerThread() {
    return Thread.currentThread() == mMainHandler.getLooper().getThread();
  }

  private <T> T awaitPlayerThread(final Callable<T> task) {
    assertTrue(!isPlayerThread());

    try {
      final FutureTask<T> wait = new FutureTask<T>(task);
      mMainHandler.post(wait);
      return wait.get();
    } catch (final Exception e) {
      throw new RuntimeException(e);
    }
  }
}
