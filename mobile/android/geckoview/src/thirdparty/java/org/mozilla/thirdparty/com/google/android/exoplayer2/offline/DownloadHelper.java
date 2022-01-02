/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.mozilla.thirdparty.com.google.android.exoplayer2.offline;

import android.content.Context;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.util.SparseIntArray;
import androidx.annotation.Nullable;
import org.mozilla.thirdparty.com.google.android.exoplayer2.C;
import org.mozilla.thirdparty.com.google.android.exoplayer2.ExoPlaybackException;
import org.mozilla.thirdparty.com.google.android.exoplayer2.RendererCapabilities;
import org.mozilla.thirdparty.com.google.android.exoplayer2.RenderersFactory;
import org.mozilla.thirdparty.com.google.android.exoplayer2.Timeline;
import org.mozilla.thirdparty.com.google.android.exoplayer2.drm.DrmSessionManager;
import org.mozilla.thirdparty.com.google.android.exoplayer2.drm.FrameworkMediaCrypto;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaPeriod;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSource.MediaPeriodId;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSource.MediaSourceCaller;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSourceFactory;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.ProgressiveMediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.TrackGroup;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.TrackGroupArray;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.chunk.MediaChunk;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.chunk.MediaChunkIterator;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.BaseTrackSelection;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.DefaultTrackSelector;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.DefaultTrackSelector.Parameters;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.DefaultTrackSelector.SelectionOverride;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.MappingTrackSelector.MappedTrackInfo;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.TrackSelection;
import org.mozilla.thirdparty.com.google.android.exoplayer2.trackselection.TrackSelectorResult;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.Allocator;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.BandwidthMeter;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DataSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DataSource.Factory;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DefaultAllocator;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.TransferListener;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Assertions;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Util;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.checkerframework.checker.nullness.compatqual.NullableType;
import org.checkerframework.checker.nullness.qual.EnsuresNonNull;
import org.checkerframework.checker.nullness.qual.MonotonicNonNull;
import org.checkerframework.checker.nullness.qual.RequiresNonNull;

/**
 * A helper for initializing and removing downloads.
 *
 * <p>The helper extracts track information from the media, selects tracks for downloading, and
 * creates {@link DownloadRequest download requests} based on the selected tracks.
 *
 * <p>A typical usage of DownloadHelper follows these steps:
 *
 * <ol>
 *   <li>Build the helper using one of the {@code forXXX} methods.
 *   <li>Prepare the helper using {@link #prepare(Callback)} and wait for the callback.
 *   <li>Optional: Inspect the selected tracks using {@link #getMappedTrackInfo(int)} and {@link
 *       #getTrackSelections(int, int)}, and make adjustments using {@link
 *       #clearTrackSelections(int)}, {@link #replaceTrackSelections(int, Parameters)} and {@link
 *       #addTrackSelection(int, Parameters)}.
 *   <li>Create a download request for the selected track using {@link #getDownloadRequest(byte[])}.
 *   <li>Release the helper using {@link #release()}.
 * </ol>
 */
public final class DownloadHelper {

  /**
   * Default track selection parameters for downloading, but without any {@link Context}
   * constraints.
   *
   * <p>If possible, use {@link #getDefaultTrackSelectorParameters(Context)} instead.
   *
   * @see Parameters#DEFAULT_WITHOUT_CONTEXT
   */
  public static final Parameters DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_CONTEXT =
      Parameters.DEFAULT_WITHOUT_CONTEXT.buildUpon().setForceHighestSupportedBitrate(true).build();

  /**
   * @deprecated This instance does not have {@link Context} constraints. Use {@link
   *     #getDefaultTrackSelectorParameters(Context)} instead.
   */
  @Deprecated
  public static final Parameters DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_VIEWPORT =
      DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_CONTEXT;

  /**
   * @deprecated This instance does not have {@link Context} constraints. Use {@link
   *     #getDefaultTrackSelectorParameters(Context)} instead.
   */
  @Deprecated
  public static final DefaultTrackSelector.Parameters DEFAULT_TRACK_SELECTOR_PARAMETERS =
      DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_CONTEXT;

  /** Returns the default parameters used for track selection for downloading. */
  public static DefaultTrackSelector.Parameters getDefaultTrackSelectorParameters(Context context) {
    return Parameters.getDefaults(context)
        .buildUpon()
        .setForceHighestSupportedBitrate(true)
        .build();
  }

  /** A callback to be notified when the {@link DownloadHelper} is prepared. */
  public interface Callback {

    /**
     * Called when preparation completes.
     *
     * @param helper The reporting {@link DownloadHelper}.
     */
    void onPrepared(DownloadHelper helper);

    /**
     * Called when preparation fails.
     *
     * @param helper The reporting {@link DownloadHelper}.
     * @param e The error.
     */
    void onPrepareError(DownloadHelper helper, IOException e);
  }

  /** Thrown at an attempt to download live content. */
  public static class LiveContentUnsupportedException extends IOException {}

  @Nullable
  private static final Constructor<? extends MediaSourceFactory> DASH_FACTORY_CONSTRUCTOR =
      getConstructor("com.google.android.exoplayer2.source.dash.DashMediaSource$Factory");

  @Nullable
  private static final Constructor<? extends MediaSourceFactory> SS_FACTORY_CONSTRUCTOR =
      getConstructor("com.google.android.exoplayer2.source.smoothstreaming.SsMediaSource$Factory");

  @Nullable
  private static final Constructor<? extends MediaSourceFactory> HLS_FACTORY_CONSTRUCTOR =
      getConstructor("com.google.android.exoplayer2.source.hls.HlsMediaSource$Factory");

  /** @deprecated Use {@link #forProgressive(Context, Uri)} */
  @Deprecated
  @SuppressWarnings("deprecation")
  public static DownloadHelper forProgressive(Uri uri) {
    return forProgressive(uri, /* cacheKey= */ null);
  }

  /**
   * Creates a {@link DownloadHelper} for progressive streams.
   *
   * @param context Any {@link Context}.
   * @param uri A stream {@link Uri}.
   * @return A {@link DownloadHelper} for progressive streams.
   */
  public static DownloadHelper forProgressive(Context context, Uri uri) {
    return forProgressive(context, uri, /* cacheKey= */ null);
  }

  /** @deprecated Use {@link #forProgressive(Context, Uri, String)} */
  @Deprecated
  public static DownloadHelper forProgressive(Uri uri, @Nullable String cacheKey) {
    return new DownloadHelper(
        DownloadRequest.TYPE_PROGRESSIVE,
        uri,
        cacheKey,
        /* mediaSource= */ null,
        DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_VIEWPORT,
        /* rendererCapabilities= */ new RendererCapabilities[0]);
  }

  /**
   * Creates a {@link DownloadHelper} for progressive streams.
   *
   * @param context Any {@link Context}.
   * @param uri A stream {@link Uri}.
   * @param cacheKey An optional cache key.
   * @return A {@link DownloadHelper} for progressive streams.
   */
  public static DownloadHelper forProgressive(Context context, Uri uri, @Nullable String cacheKey) {
    return new DownloadHelper(
        DownloadRequest.TYPE_PROGRESSIVE,
        uri,
        cacheKey,
        /* mediaSource= */ null,
        getDefaultTrackSelectorParameters(context),
        /* rendererCapabilities= */ new RendererCapabilities[0]);
  }

  /** @deprecated Use {@link #forDash(Context, Uri, Factory, RenderersFactory)} */
  @Deprecated
  public static DownloadHelper forDash(
      Uri uri, DataSource.Factory dataSourceFactory, RenderersFactory renderersFactory) {
    return forDash(
        uri,
        dataSourceFactory,
        renderersFactory,
        /* drmSessionManager= */ null,
        DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_VIEWPORT);
  }

  /**
   * Creates a {@link DownloadHelper} for DASH streams.
   *
   * @param context Any {@link Context}.
   * @param uri A manifest {@link Uri}.
   * @param dataSourceFactory A {@link DataSource.Factory} used to load the manifest.
   * @param renderersFactory A {@link RenderersFactory} creating the renderers for which tracks are
   *     selected.
   * @return A {@link DownloadHelper} for DASH streams.
   * @throws IllegalStateException If the DASH module is missing.
   */
  public static DownloadHelper forDash(
      Context context,
      Uri uri,
      DataSource.Factory dataSourceFactory,
      RenderersFactory renderersFactory) {
    return forDash(
        uri,
        dataSourceFactory,
        renderersFactory,
        /* drmSessionManager= */ null,
        getDefaultTrackSelectorParameters(context));
  }

  /**
   * Creates a {@link DownloadHelper} for DASH streams.
   *
   * @param uri A manifest {@link Uri}.
   * @param dataSourceFactory A {@link DataSource.Factory} used to load the manifest.
   * @param renderersFactory A {@link RenderersFactory} creating the renderers for which tracks are
   *     selected.
   * @param drmSessionManager An optional {@link DrmSessionManager}. Used to help determine which
   *     tracks can be selected.
   * @param trackSelectorParameters {@link DefaultTrackSelector.Parameters} for selecting tracks for
   *     downloading.
   * @return A {@link DownloadHelper} for DASH streams.
   * @throws IllegalStateException If the DASH module is missing.
   */
  public static DownloadHelper forDash(
      Uri uri,
      DataSource.Factory dataSourceFactory,
      RenderersFactory renderersFactory,
      @Nullable DrmSessionManager<FrameworkMediaCrypto> drmSessionManager,
      DefaultTrackSelector.Parameters trackSelectorParameters) {
    return new DownloadHelper(
        DownloadRequest.TYPE_DASH,
        uri,
        /* cacheKey= */ null,
        createMediaSourceInternal(
            DASH_FACTORY_CONSTRUCTOR,
            uri,
            dataSourceFactory,
            drmSessionManager,
            /* streamKeys= */ null),
        trackSelectorParameters,
        Util.getRendererCapabilities(renderersFactory));
  }

  /** @deprecated Use {@link #forHls(Context, Uri, Factory, RenderersFactory)} */
  @Deprecated
  public static DownloadHelper forHls(
      Uri uri, DataSource.Factory dataSourceFactory, RenderersFactory renderersFactory) {
    return forHls(
        uri,
        dataSourceFactory,
        renderersFactory,
        /* drmSessionManager= */ null,
        DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_VIEWPORT);
  }

  /**
   * Creates a {@link DownloadHelper} for HLS streams.
   *
   * @param context Any {@link Context}.
   * @param uri A playlist {@link Uri}.
   * @param dataSourceFactory A {@link DataSource.Factory} used to load the playlist.
   * @param renderersFactory A {@link RenderersFactory} creating the renderers for which tracks are
   *     selected.
   * @return A {@link DownloadHelper} for HLS streams.
   * @throws IllegalStateException If the HLS module is missing.
   */
  public static DownloadHelper forHls(
      Context context,
      Uri uri,
      DataSource.Factory dataSourceFactory,
      RenderersFactory renderersFactory) {
    return forHls(
        uri,
        dataSourceFactory,
        renderersFactory,
        /* drmSessionManager= */ null,
        getDefaultTrackSelectorParameters(context));
  }

  /**
   * Creates a {@link DownloadHelper} for HLS streams.
   *
   * @param uri A playlist {@link Uri}.
   * @param dataSourceFactory A {@link DataSource.Factory} used to load the playlist.
   * @param renderersFactory A {@link RenderersFactory} creating the renderers for which tracks are
   *     selected.
   * @param drmSessionManager An optional {@link DrmSessionManager}. Used to help determine which
   *     tracks can be selected.
   * @param trackSelectorParameters {@link DefaultTrackSelector.Parameters} for selecting tracks for
   *     downloading.
   * @return A {@link DownloadHelper} for HLS streams.
   * @throws IllegalStateException If the HLS module is missing.
   */
  public static DownloadHelper forHls(
      Uri uri,
      DataSource.Factory dataSourceFactory,
      RenderersFactory renderersFactory,
      @Nullable DrmSessionManager<FrameworkMediaCrypto> drmSessionManager,
      DefaultTrackSelector.Parameters trackSelectorParameters) {
    return new DownloadHelper(
        DownloadRequest.TYPE_HLS,
        uri,
        /* cacheKey= */ null,
        createMediaSourceInternal(
            HLS_FACTORY_CONSTRUCTOR,
            uri,
            dataSourceFactory,
            drmSessionManager,
            /* streamKeys= */ null),
        trackSelectorParameters,
        Util.getRendererCapabilities(renderersFactory));
  }

  /** @deprecated Use {@link #forSmoothStreaming(Context, Uri, Factory, RenderersFactory)} */
  @Deprecated
  public static DownloadHelper forSmoothStreaming(
      Uri uri, DataSource.Factory dataSourceFactory, RenderersFactory renderersFactory) {
    return forSmoothStreaming(
        uri,
        dataSourceFactory,
        renderersFactory,
        /* drmSessionManager= */ null,
        DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_VIEWPORT);
  }

  /**
   * Creates a {@link DownloadHelper} for SmoothStreaming streams.
   *
   * @param context Any {@link Context}.
   * @param uri A manifest {@link Uri}.
   * @param dataSourceFactory A {@link DataSource.Factory} used to load the manifest.
   * @param renderersFactory A {@link RenderersFactory} creating the renderers for which tracks are
   *     selected.
   * @return A {@link DownloadHelper} for SmoothStreaming streams.
   * @throws IllegalStateException If the SmoothStreaming module is missing.
   */
  public static DownloadHelper forSmoothStreaming(
      Context context,
      Uri uri,
      DataSource.Factory dataSourceFactory,
      RenderersFactory renderersFactory) {
    return forSmoothStreaming(
        uri,
        dataSourceFactory,
        renderersFactory,
        /* drmSessionManager= */ null,
        getDefaultTrackSelectorParameters(context));
  }

  /**
   * Creates a {@link DownloadHelper} for SmoothStreaming streams.
   *
   * @param uri A manifest {@link Uri}.
   * @param dataSourceFactory A {@link DataSource.Factory} used to load the manifest.
   * @param renderersFactory A {@link RenderersFactory} creating the renderers for which tracks are
   *     selected.
   * @param drmSessionManager An optional {@link DrmSessionManager}. Used to help determine which
   *     tracks can be selected.
   * @param trackSelectorParameters {@link DefaultTrackSelector.Parameters} for selecting tracks for
   *     downloading.
   * @return A {@link DownloadHelper} for SmoothStreaming streams.
   * @throws IllegalStateException If the SmoothStreaming module is missing.
   */
  public static DownloadHelper forSmoothStreaming(
      Uri uri,
      DataSource.Factory dataSourceFactory,
      RenderersFactory renderersFactory,
      @Nullable DrmSessionManager<FrameworkMediaCrypto> drmSessionManager,
      DefaultTrackSelector.Parameters trackSelectorParameters) {
    return new DownloadHelper(
        DownloadRequest.TYPE_SS,
        uri,
        /* cacheKey= */ null,
        createMediaSourceInternal(
            SS_FACTORY_CONSTRUCTOR,
            uri,
            dataSourceFactory,
            drmSessionManager,
            /* streamKeys= */ null),
        trackSelectorParameters,
        Util.getRendererCapabilities(renderersFactory));
  }

  /**
   * Equivalent to {@link #createMediaSource(DownloadRequest, Factory, DrmSessionManager)
   * createMediaSource(downloadRequest, dataSourceFactory, null)}.
   */
  public static MediaSource createMediaSource(
      DownloadRequest downloadRequest, DataSource.Factory dataSourceFactory) {
    return createMediaSource(downloadRequest, dataSourceFactory, /* drmSessionManager= */ null);
  }

  /**
   * Utility method to create a {@link MediaSource} that only exposes the tracks defined in {@code
   * downloadRequest}.
   *
   * @param downloadRequest A {@link DownloadRequest}.
   * @param dataSourceFactory A factory for {@link DataSource}s to read the media.
   * @param drmSessionManager An optional {@link DrmSessionManager} to be passed to the {@link
   *     MediaSource}.
   * @return A {@link MediaSource} that only exposes the tracks defined in {@code downloadRequest}.
   */
  public static MediaSource createMediaSource(
      DownloadRequest downloadRequest,
      DataSource.Factory dataSourceFactory,
      @Nullable DrmSessionManager<?> drmSessionManager) {
    @Nullable Constructor<? extends MediaSourceFactory> constructor;
    switch (downloadRequest.type) {
      case DownloadRequest.TYPE_DASH:
        constructor = DASH_FACTORY_CONSTRUCTOR;
        break;
      case DownloadRequest.TYPE_SS:
        constructor = SS_FACTORY_CONSTRUCTOR;
        break;
      case DownloadRequest.TYPE_HLS:
        constructor = HLS_FACTORY_CONSTRUCTOR;
        break;
      case DownloadRequest.TYPE_PROGRESSIVE:
        return new ProgressiveMediaSource.Factory(dataSourceFactory)
            .setCustomCacheKey(downloadRequest.customCacheKey)
            .createMediaSource(downloadRequest.uri);
      default:
        throw new IllegalStateException("Unsupported type: " + downloadRequest.type);
    }
    return createMediaSourceInternal(
        constructor,
        downloadRequest.uri,
        dataSourceFactory,
        drmSessionManager,
        downloadRequest.streamKeys);
  }

  private final String downloadType;
  private final Uri uri;
  @Nullable private final String cacheKey;
  @Nullable private final MediaSource mediaSource;
  private final DefaultTrackSelector trackSelector;
  private final RendererCapabilities[] rendererCapabilities;
  private final SparseIntArray scratchSet;
  private final Handler callbackHandler;
  private final Timeline.Window window;

  private boolean isPreparedWithMedia;
  private @MonotonicNonNull Callback callback;
  private @MonotonicNonNull MediaPreparer mediaPreparer;
  private TrackGroupArray @MonotonicNonNull [] trackGroupArrays;
  private MappedTrackInfo @MonotonicNonNull [] mappedTrackInfos;
  private List<TrackSelection> @MonotonicNonNull [][] trackSelectionsByPeriodAndRenderer;
  private List<TrackSelection> @MonotonicNonNull [][] immutableTrackSelectionsByPeriodAndRenderer;

  /**
   * Creates download helper.
   *
   * @param downloadType A download type. This value will be used as {@link DownloadRequest#type}.
   * @param uri A {@link Uri}.
   * @param cacheKey An optional cache key.
   * @param mediaSource A {@link MediaSource} for which tracks are selected, or null if no track
   *     selection needs to be made.
   * @param trackSelectorParameters {@link DefaultTrackSelector.Parameters} for selecting tracks for
   *     downloading.
   * @param rendererCapabilities The {@link RendererCapabilities} of the renderers for which tracks
   *     are selected.
   */
  public DownloadHelper(
      String downloadType,
      Uri uri,
      @Nullable String cacheKey,
      @Nullable MediaSource mediaSource,
      DefaultTrackSelector.Parameters trackSelectorParameters,
      RendererCapabilities[] rendererCapabilities) {
    this.downloadType = downloadType;
    this.uri = uri;
    this.cacheKey = cacheKey;
    this.mediaSource = mediaSource;
    this.trackSelector =
        new DefaultTrackSelector(trackSelectorParameters, new DownloadTrackSelection.Factory());
    this.rendererCapabilities = rendererCapabilities;
    this.scratchSet = new SparseIntArray();
    trackSelector.init(/* listener= */ () -> {}, new DummyBandwidthMeter());
    callbackHandler = new Handler(Util.getLooper());
    window = new Timeline.Window();
  }

  /**
   * Initializes the helper for starting a download.
   *
   * @param callback A callback to be notified when preparation completes or fails.
   * @throws IllegalStateException If the download helper has already been prepared.
   */
  public void prepare(Callback callback) {
    Assertions.checkState(this.callback == null);
    this.callback = callback;
    if (mediaSource != null) {
      mediaPreparer = new MediaPreparer(mediaSource, /* downloadHelper= */ this);
    } else {
      callbackHandler.post(() -> callback.onPrepared(this));
    }
  }

  /** Releases the helper and all resources it is holding. */
  public void release() {
    if (mediaPreparer != null) {
      mediaPreparer.release();
    }
  }

  /**
   * Returns the manifest, or null if no manifest is loaded. Must not be called until after
   * preparation completes.
   */
  @Nullable
  public Object getManifest() {
    if (mediaSource == null) {
      return null;
    }
    assertPreparedWithMedia();
    return mediaPreparer.timeline.getWindowCount() > 0
        ? mediaPreparer.timeline.getWindow(/* windowIndex= */ 0, window).manifest
        : null;
  }

  /**
   * Returns the number of periods for which media is available. Must not be called until after
   * preparation completes.
   */
  public int getPeriodCount() {
    if (mediaSource == null) {
      return 0;
    }
    assertPreparedWithMedia();
    return trackGroupArrays.length;
  }

  /**
   * Returns the track groups for the given period. Must not be called until after preparation
   * completes.
   *
   * <p>Use {@link #getMappedTrackInfo(int)} to get the track groups mapped to renderers.
   *
   * @param periodIndex The period index.
   * @return The track groups for the period. May be {@link TrackGroupArray#EMPTY} for single stream
   *     content.
   */
  public TrackGroupArray getTrackGroups(int periodIndex) {
    assertPreparedWithMedia();
    return trackGroupArrays[periodIndex];
  }

  /**
   * Returns the mapped track info for the given period. Must not be called until after preparation
   * completes.
   *
   * @param periodIndex The period index.
   * @return The {@link MappedTrackInfo} for the period.
   */
  public MappedTrackInfo getMappedTrackInfo(int periodIndex) {
    assertPreparedWithMedia();
    return mappedTrackInfos[periodIndex];
  }

  /**
   * Returns all {@link TrackSelection track selections} for a period and renderer. Must not be
   * called until after preparation completes.
   *
   * @param periodIndex The period index.
   * @param rendererIndex The renderer index.
   * @return A list of selected {@link TrackSelection track selections}.
   */
  public List<TrackSelection> getTrackSelections(int periodIndex, int rendererIndex) {
    assertPreparedWithMedia();
    return immutableTrackSelectionsByPeriodAndRenderer[periodIndex][rendererIndex];
  }

  /**
   * Clears the selection of tracks for a period. Must not be called until after preparation
   * completes.
   *
   * @param periodIndex The period index for which track selections are cleared.
   */
  public void clearTrackSelections(int periodIndex) {
    assertPreparedWithMedia();
    for (int i = 0; i < rendererCapabilities.length; i++) {
      trackSelectionsByPeriodAndRenderer[periodIndex][i].clear();
    }
  }

  /**
   * Replaces a selection of tracks to be downloaded. Must not be called until after preparation
   * completes.
   *
   * @param periodIndex The period index for which the track selection is replaced.
   * @param trackSelectorParameters The {@link DefaultTrackSelector.Parameters} to obtain the new
   *     selection of tracks.
   */
  public void replaceTrackSelections(
      int periodIndex, DefaultTrackSelector.Parameters trackSelectorParameters) {
    clearTrackSelections(periodIndex);
    addTrackSelection(periodIndex, trackSelectorParameters);
  }

  /**
   * Adds a selection of tracks to be downloaded. Must not be called until after preparation
   * completes.
   *
   * @param periodIndex The period index this track selection is added for.
   * @param trackSelectorParameters The {@link DefaultTrackSelector.Parameters} to obtain the new
   *     selection of tracks.
   */
  public void addTrackSelection(
      int periodIndex, DefaultTrackSelector.Parameters trackSelectorParameters) {
    assertPreparedWithMedia();
    trackSelector.setParameters(trackSelectorParameters);
    runTrackSelection(periodIndex);
  }

  /**
   * Convenience method to add selections of tracks for all specified audio languages. If an audio
   * track in one of the specified languages is not available, the default fallback audio track is
   * used instead. Must not be called until after preparation completes.
   *
   * @param languages A list of audio languages for which tracks should be added to the download
   *     selection, as IETF BCP 47 conformant tags.
   */
  public void addAudioLanguagesToSelection(String... languages) {
    assertPreparedWithMedia();
    for (int periodIndex = 0; periodIndex < mappedTrackInfos.length; periodIndex++) {
      DefaultTrackSelector.ParametersBuilder parametersBuilder =
          DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_CONTEXT.buildUpon();
      MappedTrackInfo mappedTrackInfo = mappedTrackInfos[periodIndex];
      int rendererCount = mappedTrackInfo.getRendererCount();
      for (int rendererIndex = 0; rendererIndex < rendererCount; rendererIndex++) {
        if (mappedTrackInfo.getRendererType(rendererIndex) != C.TRACK_TYPE_AUDIO) {
          parametersBuilder.setRendererDisabled(rendererIndex, /* disabled= */ true);
        }
      }
      for (String language : languages) {
        parametersBuilder.setPreferredAudioLanguage(language);
        addTrackSelection(periodIndex, parametersBuilder.build());
      }
    }
  }

  /**
   * Convenience method to add selections of tracks for all specified text languages. Must not be
   * called until after preparation completes.
   *
   * @param selectUndeterminedTextLanguage Whether a text track with undetermined language should be
   *     selected for downloading if no track with one of the specified {@code languages} is
   *     available.
   * @param languages A list of text languages for which tracks should be added to the download
   *     selection, as IETF BCP 47 conformant tags.
   */
  public void addTextLanguagesToSelection(
      boolean selectUndeterminedTextLanguage, String... languages) {
    assertPreparedWithMedia();
    for (int periodIndex = 0; periodIndex < mappedTrackInfos.length; periodIndex++) {
      DefaultTrackSelector.ParametersBuilder parametersBuilder =
          DEFAULT_TRACK_SELECTOR_PARAMETERS_WITHOUT_CONTEXT.buildUpon();
      MappedTrackInfo mappedTrackInfo = mappedTrackInfos[periodIndex];
      int rendererCount = mappedTrackInfo.getRendererCount();
      for (int rendererIndex = 0; rendererIndex < rendererCount; rendererIndex++) {
        if (mappedTrackInfo.getRendererType(rendererIndex) != C.TRACK_TYPE_TEXT) {
          parametersBuilder.setRendererDisabled(rendererIndex, /* disabled= */ true);
        }
      }
      parametersBuilder.setSelectUndeterminedTextLanguage(selectUndeterminedTextLanguage);
      for (String language : languages) {
        parametersBuilder.setPreferredTextLanguage(language);
        addTrackSelection(periodIndex, parametersBuilder.build());
      }
    }
  }

  /**
   * Convenience method to add a selection of tracks to be downloaded for a single renderer. Must
   * not be called until after preparation completes.
   *
   * @param periodIndex The period index the track selection is added for.
   * @param rendererIndex The renderer index the track selection is added for.
   * @param trackSelectorParameters The {@link DefaultTrackSelector.Parameters} to obtain the new
   *     selection of tracks.
   * @param overrides A list of {@link SelectionOverride SelectionOverrides} to apply to the {@code
   *     trackSelectorParameters}. If empty, {@code trackSelectorParameters} are used as they are.
   */
  public void addTrackSelectionForSingleRenderer(
      int periodIndex,
      int rendererIndex,
      DefaultTrackSelector.Parameters trackSelectorParameters,
      List<SelectionOverride> overrides) {
    assertPreparedWithMedia();
    DefaultTrackSelector.ParametersBuilder builder = trackSelectorParameters.buildUpon();
    for (int i = 0; i < mappedTrackInfos[periodIndex].getRendererCount(); i++) {
      builder.setRendererDisabled(/* rendererIndex= */ i, /* disabled= */ i != rendererIndex);
    }
    if (overrides.isEmpty()) {
      addTrackSelection(periodIndex, builder.build());
    } else {
      TrackGroupArray trackGroupArray = mappedTrackInfos[periodIndex].getTrackGroups(rendererIndex);
      for (int i = 0; i < overrides.size(); i++) {
        builder.setSelectionOverride(rendererIndex, trackGroupArray, overrides.get(i));
        addTrackSelection(periodIndex, builder.build());
      }
    }
  }

  /**
   * Builds a {@link DownloadRequest} for downloading the selected tracks. Must not be called until
   * after preparation completes. The uri of the {@link DownloadRequest} will be used as content id.
   *
   * @param data Application provided data to store in {@link DownloadRequest#data}.
   * @return The built {@link DownloadRequest}.
   */
  public DownloadRequest getDownloadRequest(@Nullable byte[] data) {
    return getDownloadRequest(uri.toString(), data);
  }

  /**
   * Builds a {@link DownloadRequest} for downloading the selected tracks. Must not be called until
   * after preparation completes.
   *
   * @param id The unique content id.
   * @param data Application provided data to store in {@link DownloadRequest#data}.
   * @return The built {@link DownloadRequest}.
   */
  public DownloadRequest getDownloadRequest(String id, @Nullable byte[] data) {
    if (mediaSource == null) {
      return new DownloadRequest(
          id, downloadType, uri, /* streamKeys= */ Collections.emptyList(), cacheKey, data);
    }
    assertPreparedWithMedia();
    List<StreamKey> streamKeys = new ArrayList<>();
    List<TrackSelection> allSelections = new ArrayList<>();
    int periodCount = trackSelectionsByPeriodAndRenderer.length;
    for (int periodIndex = 0; periodIndex < periodCount; periodIndex++) {
      allSelections.clear();
      int rendererCount = trackSelectionsByPeriodAndRenderer[periodIndex].length;
      for (int rendererIndex = 0; rendererIndex < rendererCount; rendererIndex++) {
        allSelections.addAll(trackSelectionsByPeriodAndRenderer[periodIndex][rendererIndex]);
      }
      streamKeys.addAll(mediaPreparer.mediaPeriods[periodIndex].getStreamKeys(allSelections));
    }
    return new DownloadRequest(id, downloadType, uri, streamKeys, cacheKey, data);
  }

  // Initialization of array of Lists.
  @SuppressWarnings("unchecked")
  private void onMediaPrepared() {
    Assertions.checkNotNull(mediaPreparer);
    Assertions.checkNotNull(mediaPreparer.mediaPeriods);
    Assertions.checkNotNull(mediaPreparer.timeline);
    int periodCount = mediaPreparer.mediaPeriods.length;
    int rendererCount = rendererCapabilities.length;
    trackSelectionsByPeriodAndRenderer =
        (List<TrackSelection>[][]) new List<?>[periodCount][rendererCount];
    immutableTrackSelectionsByPeriodAndRenderer =
        (List<TrackSelection>[][]) new List<?>[periodCount][rendererCount];
    for (int i = 0; i < periodCount; i++) {
      for (int j = 0; j < rendererCount; j++) {
        trackSelectionsByPeriodAndRenderer[i][j] = new ArrayList<>();
        immutableTrackSelectionsByPeriodAndRenderer[i][j] =
            Collections.unmodifiableList(trackSelectionsByPeriodAndRenderer[i][j]);
      }
    }
    trackGroupArrays = new TrackGroupArray[periodCount];
    mappedTrackInfos = new MappedTrackInfo[periodCount];
    for (int i = 0; i < periodCount; i++) {
      trackGroupArrays[i] = mediaPreparer.mediaPeriods[i].getTrackGroups();
      TrackSelectorResult trackSelectorResult = runTrackSelection(/* periodIndex= */ i);
      trackSelector.onSelectionActivated(trackSelectorResult.info);
      mappedTrackInfos[i] = Assertions.checkNotNull(trackSelector.getCurrentMappedTrackInfo());
    }
    setPreparedWithMedia();
    Assertions.checkNotNull(callbackHandler)
        .post(() -> Assertions.checkNotNull(callback).onPrepared(this));
  }

  private void onMediaPreparationFailed(IOException error) {
    Assertions.checkNotNull(callbackHandler)
        .post(() -> Assertions.checkNotNull(callback).onPrepareError(this, error));
  }

  @RequiresNonNull({
    "trackGroupArrays",
    "mappedTrackInfos",
    "trackSelectionsByPeriodAndRenderer",
    "immutableTrackSelectionsByPeriodAndRenderer",
    "mediaPreparer",
    "mediaPreparer.timeline",
    "mediaPreparer.mediaPeriods"
  })
  private void setPreparedWithMedia() {
    isPreparedWithMedia = true;
  }

  @EnsuresNonNull({
    "trackGroupArrays",
    "mappedTrackInfos",
    "trackSelectionsByPeriodAndRenderer",
    "immutableTrackSelectionsByPeriodAndRenderer",
    "mediaPreparer",
    "mediaPreparer.timeline",
    "mediaPreparer.mediaPeriods"
  })
  @SuppressWarnings("nullness:contracts.postcondition.not.satisfied")
  private void assertPreparedWithMedia() {
    Assertions.checkState(isPreparedWithMedia);
  }

  /**
   * Runs the track selection for a given period index with the current parameters. The selected
   * tracks will be added to {@link #trackSelectionsByPeriodAndRenderer}.
   */
  // Intentional reference comparison of track group instances.
  @SuppressWarnings("ReferenceEquality")
  @RequiresNonNull({
    "trackGroupArrays",
    "trackSelectionsByPeriodAndRenderer",
    "mediaPreparer",
    "mediaPreparer.timeline"
  })
  private TrackSelectorResult runTrackSelection(int periodIndex) {
    try {
      TrackSelectorResult trackSelectorResult =
          trackSelector.selectTracks(
              rendererCapabilities,
              trackGroupArrays[periodIndex],
              new MediaPeriodId(mediaPreparer.timeline.getUidOfPeriod(periodIndex)),
              mediaPreparer.timeline);
      for (int i = 0; i < trackSelectorResult.length; i++) {
        @Nullable TrackSelection newSelection = trackSelectorResult.selections.get(i);
        if (newSelection == null) {
          continue;
        }
        List<TrackSelection> existingSelectionList =
            trackSelectionsByPeriodAndRenderer[periodIndex][i];
        boolean mergedWithExistingSelection = false;
        for (int j = 0; j < existingSelectionList.size(); j++) {
          TrackSelection existingSelection = existingSelectionList.get(j);
          if (existingSelection.getTrackGroup() == newSelection.getTrackGroup()) {
            // Merge with existing selection.
            scratchSet.clear();
            for (int k = 0; k < existingSelection.length(); k++) {
              scratchSet.put(existingSelection.getIndexInTrackGroup(k), 0);
            }
            for (int k = 0; k < newSelection.length(); k++) {
              scratchSet.put(newSelection.getIndexInTrackGroup(k), 0);
            }
            int[] mergedTracks = new int[scratchSet.size()];
            for (int k = 0; k < scratchSet.size(); k++) {
              mergedTracks[k] = scratchSet.keyAt(k);
            }
            existingSelectionList.set(
                j, new DownloadTrackSelection(existingSelection.getTrackGroup(), mergedTracks));
            mergedWithExistingSelection = true;
            break;
          }
        }
        if (!mergedWithExistingSelection) {
          existingSelectionList.add(newSelection);
        }
      }
      return trackSelectorResult;
    } catch (ExoPlaybackException e) {
      // DefaultTrackSelector does not throw exceptions during track selection.
      throw new UnsupportedOperationException(e);
    }
  }

  @Nullable
  private static Constructor<? extends MediaSourceFactory> getConstructor(String className) {
    try {
      // LINT.IfChange
      Class<? extends MediaSourceFactory> factoryClazz =
          Class.forName(className).asSubclass(MediaSourceFactory.class);
      return factoryClazz.getConstructor(Factory.class);
      // LINT.ThenChange(../../../../../../../../proguard-rules.txt)
    } catch (ClassNotFoundException e) {
      // Expected if the app was built without the respective module.
      return null;
    } catch (NoSuchMethodException e) {
      // Something is wrong with the library or the proguard configuration.
      throw new IllegalStateException(e);
    }
  }

  private static MediaSource createMediaSourceInternal(
      @Nullable Constructor<? extends MediaSourceFactory> constructor,
      Uri uri,
      Factory dataSourceFactory,
      @Nullable DrmSessionManager<?> drmSessionManager,
      @Nullable List<StreamKey> streamKeys) {
    if (constructor == null) {
      throw new IllegalStateException("Module missing to create media source.");
    }
    try {
      MediaSourceFactory factory = constructor.newInstance(dataSourceFactory);
      if (drmSessionManager != null) {
        factory.setDrmSessionManager(drmSessionManager);
      }
      if (streamKeys != null) {
        factory.setStreamKeys(streamKeys);
      }
      return Assertions.checkNotNull(factory.createMediaSource(uri));
    } catch (Exception e) {
      throw new IllegalStateException("Failed to instantiate media source.", e);
    }
  }

  private static final class MediaPreparer
      implements MediaSourceCaller, MediaPeriod.Callback, Handler.Callback {

    private static final int MESSAGE_PREPARE_SOURCE = 0;
    private static final int MESSAGE_CHECK_FOR_FAILURE = 1;
    private static final int MESSAGE_CONTINUE_LOADING = 2;
    private static final int MESSAGE_RELEASE = 3;

    private static final int DOWNLOAD_HELPER_CALLBACK_MESSAGE_PREPARED = 0;
    private static final int DOWNLOAD_HELPER_CALLBACK_MESSAGE_FAILED = 1;

    private final MediaSource mediaSource;
    private final DownloadHelper downloadHelper;
    private final Allocator allocator;
    private final ArrayList<MediaPeriod> pendingMediaPeriods;
    private final Handler downloadHelperHandler;
    private final HandlerThread mediaSourceThread;
    private final Handler mediaSourceHandler;

    public @MonotonicNonNull Timeline timeline;
    public MediaPeriod @MonotonicNonNull [] mediaPeriods;

    private boolean released;

    public MediaPreparer(MediaSource mediaSource, DownloadHelper downloadHelper) {
      this.mediaSource = mediaSource;
      this.downloadHelper = downloadHelper;
      allocator = new DefaultAllocator(true, C.DEFAULT_BUFFER_SEGMENT_SIZE);
      pendingMediaPeriods = new ArrayList<>();
      @SuppressWarnings("methodref.receiver.bound.invalid")
      Handler downloadThreadHandler = Util.createHandler(this::handleDownloadHelperCallbackMessage);
      this.downloadHelperHandler = downloadThreadHandler;
      mediaSourceThread = new HandlerThread("DownloadHelper");
      mediaSourceThread.start();
      mediaSourceHandler = Util.createHandler(mediaSourceThread.getLooper(), /* callback= */ this);
      mediaSourceHandler.sendEmptyMessage(MESSAGE_PREPARE_SOURCE);
    }

    public void release() {
      if (released) {
        return;
      }
      released = true;
      mediaSourceHandler.sendEmptyMessage(MESSAGE_RELEASE);
    }

    // Handler.Callback

    @Override
    public boolean handleMessage(Message msg) {
      switch (msg.what) {
        case MESSAGE_PREPARE_SOURCE:
          mediaSource.prepareSource(/* caller= */ this, /* mediaTransferListener= */ null);
          mediaSourceHandler.sendEmptyMessage(MESSAGE_CHECK_FOR_FAILURE);
          return true;
        case MESSAGE_CHECK_FOR_FAILURE:
          try {
            if (mediaPeriods == null) {
              mediaSource.maybeThrowSourceInfoRefreshError();
            } else {
              for (int i = 0; i < pendingMediaPeriods.size(); i++) {
                pendingMediaPeriods.get(i).maybeThrowPrepareError();
              }
            }
            mediaSourceHandler.sendEmptyMessageDelayed(
                MESSAGE_CHECK_FOR_FAILURE, /* delayMillis= */ 100);
          } catch (IOException e) {
            downloadHelperHandler
                .obtainMessage(DOWNLOAD_HELPER_CALLBACK_MESSAGE_FAILED, /* obj= */ e)
                .sendToTarget();
          }
          return true;
        case MESSAGE_CONTINUE_LOADING:
          MediaPeriod mediaPeriod = (MediaPeriod) msg.obj;
          if (pendingMediaPeriods.contains(mediaPeriod)) {
            mediaPeriod.continueLoading(/* positionUs= */ 0);
          }
          return true;
        case MESSAGE_RELEASE:
          if (mediaPeriods != null) {
            for (MediaPeriod period : mediaPeriods) {
              mediaSource.releasePeriod(period);
            }
          }
          mediaSource.releaseSource(this);
          mediaSourceHandler.removeCallbacksAndMessages(null);
          mediaSourceThread.quit();
          return true;
        default:
          return false;
      }
    }

    // MediaSource.MediaSourceCaller implementation.

    @Override
    public void onSourceInfoRefreshed(MediaSource source, Timeline timeline) {
      if (this.timeline != null) {
        // Ignore dynamic updates.
        return;
      }
      if (timeline.getWindow(/* windowIndex= */ 0, new Timeline.Window()).isLive) {
        downloadHelperHandler
            .obtainMessage(
                DOWNLOAD_HELPER_CALLBACK_MESSAGE_FAILED,
                /* obj= */ new LiveContentUnsupportedException())
            .sendToTarget();
        return;
      }
      this.timeline = timeline;
      mediaPeriods = new MediaPeriod[timeline.getPeriodCount()];
      for (int i = 0; i < mediaPeriods.length; i++) {
        MediaPeriod mediaPeriod =
            mediaSource.createPeriod(
                new MediaPeriodId(timeline.getUidOfPeriod(/* periodIndex= */ i)),
                allocator,
                /* startPositionUs= */ 0);
        mediaPeriods[i] = mediaPeriod;
        pendingMediaPeriods.add(mediaPeriod);
      }
      for (MediaPeriod mediaPeriod : mediaPeriods) {
        mediaPeriod.prepare(/* callback= */ this, /* positionUs= */ 0);
      }
    }

    // MediaPeriod.Callback implementation.

    @Override
    public void onPrepared(MediaPeriod mediaPeriod) {
      pendingMediaPeriods.remove(mediaPeriod);
      if (pendingMediaPeriods.isEmpty()) {
        mediaSourceHandler.removeMessages(MESSAGE_CHECK_FOR_FAILURE);
        downloadHelperHandler.sendEmptyMessage(DOWNLOAD_HELPER_CALLBACK_MESSAGE_PREPARED);
      }
    }

    @Override
    public void onContinueLoadingRequested(MediaPeriod mediaPeriod) {
      if (pendingMediaPeriods.contains(mediaPeriod)) {
        mediaSourceHandler.obtainMessage(MESSAGE_CONTINUE_LOADING, mediaPeriod).sendToTarget();
      }
    }

    private boolean handleDownloadHelperCallbackMessage(Message msg) {
      if (released) {
        // Stale message.
        return false;
      }
      switch (msg.what) {
        case DOWNLOAD_HELPER_CALLBACK_MESSAGE_PREPARED:
          downloadHelper.onMediaPrepared();
          return true;
        case DOWNLOAD_HELPER_CALLBACK_MESSAGE_FAILED:
          release();
          downloadHelper.onMediaPreparationFailed((IOException) Util.castNonNull(msg.obj));
          return true;
        default:
          return false;
      }
    }
  }

  private static final class DownloadTrackSelection extends BaseTrackSelection {

    private static final class Factory implements TrackSelection.Factory {

      @Override
      public @NullableType TrackSelection[] createTrackSelections(
          @NullableType Definition[] definitions, BandwidthMeter bandwidthMeter) {
        @NullableType TrackSelection[] selections = new TrackSelection[definitions.length];
        for (int i = 0; i < definitions.length; i++) {
          selections[i] =
              definitions[i] == null
                  ? null
                  : new DownloadTrackSelection(definitions[i].group, definitions[i].tracks);
        }
        return selections;
      }
    }

    public DownloadTrackSelection(TrackGroup trackGroup, int[] tracks) {
      super(trackGroup, tracks);
    }

    @Override
    public int getSelectedIndex() {
      return 0;
    }

    @Override
    public int getSelectionReason() {
      return C.SELECTION_REASON_UNKNOWN;
    }

    @Nullable
    @Override
    public Object getSelectionData() {
      return null;
    }

    @Override
    public void updateSelectedTrack(
        long playbackPositionUs,
        long bufferedDurationUs,
        long availableDurationUs,
        List<? extends MediaChunk> queue,
        MediaChunkIterator[] mediaChunkIterators) {
      // Do nothing.
    }
  }

  private static final class DummyBandwidthMeter implements BandwidthMeter {

    @Override
    public long getBitrateEstimate() {
      return 0;
    }

    @Nullable
    @Override
    public TransferListener getTransferListener() {
      return null;
    }

    @Override
    public void addEventListener(Handler eventHandler, EventListener eventListener) {
      // Do nothing.
    }

    @Override
    public void removeEventListener(EventListener eventListener) {
      // Do nothing.
    }
  }
}
