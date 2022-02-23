/*
 * Copyright (C) 2017 The Android Open Source Project
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
package org.mozilla.thirdparty.com.google.android.exoplayer2.source.ads;

import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import org.mozilla.thirdparty.com.google.android.exoplayer2.C;
import org.mozilla.thirdparty.com.google.android.exoplayer2.Timeline;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.CompositeMediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MaskingMediaPeriod;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaPeriod;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSource.MediaPeriodId;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSourceEventListener;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSourceEventListener.LoadEventInfo;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSourceEventListener.MediaLoadData;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.MediaSourceFactory;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.ProgressiveMediaSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.Allocator;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DataSource;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.DataSpec;
import org.mozilla.thirdparty.com.google.android.exoplayer2.upstream.TransferListener;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Assertions;
import java.io.IOException;
import java.lang.annotation.Documented;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.checkerframework.checker.nullness.compatqual.NullableType;

/**
 * A {@link MediaSource} that inserts ads linearly with a provided content media source. This source
 * cannot be used as a child source in a composition. It must be the top-level source used to
 * prepare the player.
 */
public final class AdsMediaSource extends CompositeMediaSource<MediaPeriodId> {

  /**
   * Wrapper for exceptions that occur while loading ads, which are notified via {@link
   * MediaSourceEventListener#onLoadError(int, MediaPeriodId, LoadEventInfo, MediaLoadData,
   * IOException, boolean)}.
   */
  public static final class AdLoadException extends IOException {

    /**
     * Types of ad load exceptions. One of {@link #TYPE_AD}, {@link #TYPE_AD_GROUP}, {@link
     * #TYPE_ALL_ADS} or {@link #TYPE_UNEXPECTED}.
     */
    @Documented
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({TYPE_AD, TYPE_AD_GROUP, TYPE_ALL_ADS, TYPE_UNEXPECTED})
    public @interface Type {}
    /** Type for when an ad failed to load. The ad will be skipped. */
    public static final int TYPE_AD = 0;
    /** Type for when an ad group failed to load. The ad group will be skipped. */
    public static final int TYPE_AD_GROUP = 1;
    /** Type for when all ad groups failed to load. All ads will be skipped. */
    public static final int TYPE_ALL_ADS = 2;
    /** Type for when an unexpected error occurred while loading ads. All ads will be skipped. */
    public static final int TYPE_UNEXPECTED = 3;

    /** Returns a new ad load exception of {@link #TYPE_AD}. */
    public static AdLoadException createForAd(Exception error) {
      return new AdLoadException(TYPE_AD, error);
    }

    /** Returns a new ad load exception of {@link #TYPE_AD_GROUP}. */
    public static AdLoadException createForAdGroup(Exception error, int adGroupIndex) {
      return new AdLoadException(
          TYPE_AD_GROUP, new IOException("Failed to load ad group " + adGroupIndex, error));
    }

    /** Returns a new ad load exception of {@link #TYPE_ALL_ADS}. */
    public static AdLoadException createForAllAds(Exception error) {
      return new AdLoadException(TYPE_ALL_ADS, error);
    }

    /** Returns a new ad load exception of {@link #TYPE_UNEXPECTED}. */
    public static AdLoadException createForUnexpected(RuntimeException error) {
      return new AdLoadException(TYPE_UNEXPECTED, error);
    }

    /** The {@link Type} of the ad load exception. */
    public final @Type int type;

    private AdLoadException(@Type int type, Exception cause) {
      super(cause);
      this.type = type;
    }

    /**
     * Returns the {@link RuntimeException} that caused the exception if its type is {@link
     * #TYPE_UNEXPECTED}.
     */
    public RuntimeException getRuntimeExceptionForUnexpected() {
      Assertions.checkState(type == TYPE_UNEXPECTED);
      return (RuntimeException) Assertions.checkNotNull(getCause());
    }
  }

  // Used to identify the content "child" source for CompositeMediaSource.
  private static final MediaPeriodId DUMMY_CONTENT_MEDIA_PERIOD_ID =
      new MediaPeriodId(/* periodUid= */ new Object());

  private final MediaSource contentMediaSource;
  private final MediaSourceFactory adMediaSourceFactory;
  private final AdsLoader adsLoader;
  private final AdsLoader.AdViewProvider adViewProvider;
  private final Handler mainHandler;
  private final Map<MediaSource, List<MaskingMediaPeriod>> maskingMediaPeriodByAdMediaSource;
  private final Timeline.Period period;

  // Accessed on the player thread.
  @Nullable private ComponentListener componentListener;
  @Nullable private Timeline contentTimeline;
  @Nullable private AdPlaybackState adPlaybackState;
  private @NullableType MediaSource[][] adGroupMediaSources;
  private @NullableType Timeline[][] adGroupTimelines;

  /**
   * Constructs a new source that inserts ads linearly with the content specified by {@code
   * contentMediaSource}. Ad media is loaded using {@link ProgressiveMediaSource}.
   *
   * @param contentMediaSource The {@link MediaSource} providing the content to play.
   * @param dataSourceFactory Factory for data sources used to load ad media.
   * @param adsLoader The loader for ads.
   * @param adViewProvider Provider of views for the ad UI.
   */
  public AdsMediaSource(
      MediaSource contentMediaSource,
      DataSource.Factory dataSourceFactory,
      AdsLoader adsLoader,
      AdsLoader.AdViewProvider adViewProvider) {
    this(
        contentMediaSource,
        new ProgressiveMediaSource.Factory(dataSourceFactory),
        adsLoader,
        adViewProvider);
  }

  /**
   * Constructs a new source that inserts ads linearly with the content specified by {@code
   * contentMediaSource}.
   *
   * @param contentMediaSource The {@link MediaSource} providing the content to play.
   * @param adMediaSourceFactory Factory for media sources used to load ad media.
   * @param adsLoader The loader for ads.
   * @param adViewProvider Provider of views for the ad UI.
   */
  public AdsMediaSource(
      MediaSource contentMediaSource,
      MediaSourceFactory adMediaSourceFactory,
      AdsLoader adsLoader,
      AdsLoader.AdViewProvider adViewProvider) {
    this.contentMediaSource = contentMediaSource;
    this.adMediaSourceFactory = adMediaSourceFactory;
    this.adsLoader = adsLoader;
    this.adViewProvider = adViewProvider;
    mainHandler = new Handler(Looper.getMainLooper());
    maskingMediaPeriodByAdMediaSource = new HashMap<>();
    period = new Timeline.Period();
    adGroupMediaSources = new MediaSource[0][];
    adGroupTimelines = new Timeline[0][];
    adsLoader.setSupportedContentTypes(adMediaSourceFactory.getSupportedTypes());
  }

  @Override
  @Nullable
  public Object getTag() {
    return contentMediaSource.getTag();
  }

  @Override
  protected void prepareSourceInternal(@Nullable TransferListener mediaTransferListener) {
    super.prepareSourceInternal(mediaTransferListener);
    ComponentListener componentListener = new ComponentListener();
    this.componentListener = componentListener;
    prepareChildSource(DUMMY_CONTENT_MEDIA_PERIOD_ID, contentMediaSource);
    mainHandler.post(() -> adsLoader.start(componentListener, adViewProvider));
  }

  @Override
  public MediaPeriod createPeriod(MediaPeriodId id, Allocator allocator, long startPositionUs) {
    AdPlaybackState adPlaybackState = Assertions.checkNotNull(this.adPlaybackState);
    if (adPlaybackState.adGroupCount > 0 && id.isAd()) {
      int adGroupIndex = id.adGroupIndex;
      int adIndexInAdGroup = id.adIndexInAdGroup;
      Uri adUri =
          Assertions.checkNotNull(adPlaybackState.adGroups[adGroupIndex].uris[adIndexInAdGroup]);
      if (adGroupMediaSources[adGroupIndex].length <= adIndexInAdGroup) {
        int adCount = adIndexInAdGroup + 1;
        adGroupMediaSources[adGroupIndex] =
            Arrays.copyOf(adGroupMediaSources[adGroupIndex], adCount);
        adGroupTimelines[adGroupIndex] = Arrays.copyOf(adGroupTimelines[adGroupIndex], adCount);
      }
      MediaSource mediaSource = adGroupMediaSources[adGroupIndex][adIndexInAdGroup];
      if (mediaSource == null) {
        mediaSource = adMediaSourceFactory.createMediaSource(adUri);
        adGroupMediaSources[adGroupIndex][adIndexInAdGroup] = mediaSource;
        maskingMediaPeriodByAdMediaSource.put(mediaSource, new ArrayList<>());
        prepareChildSource(id, mediaSource);
      }
      MaskingMediaPeriod maskingMediaPeriod =
          new MaskingMediaPeriod(mediaSource, id, allocator, startPositionUs);
      maskingMediaPeriod.setPrepareErrorListener(
          new AdPrepareErrorListener(adUri, adGroupIndex, adIndexInAdGroup));
      List<MaskingMediaPeriod> mediaPeriods = maskingMediaPeriodByAdMediaSource.get(mediaSource);
      if (mediaPeriods == null) {
        Object periodUid =
            Assertions.checkNotNull(adGroupTimelines[adGroupIndex][adIndexInAdGroup])
                .getUidOfPeriod(/* periodIndex= */ 0);
        MediaPeriodId adSourceMediaPeriodId = new MediaPeriodId(periodUid, id.windowSequenceNumber);
        maskingMediaPeriod.createPeriod(adSourceMediaPeriodId);
      } else {
        // Keep track of the masking media period so it can be populated with the real media period
        // when the source's info becomes available.
        mediaPeriods.add(maskingMediaPeriod);
      }
      return maskingMediaPeriod;
    } else {
      MaskingMediaPeriod mediaPeriod =
          new MaskingMediaPeriod(contentMediaSource, id, allocator, startPositionUs);
      mediaPeriod.createPeriod(id);
      return mediaPeriod;
    }
  }

  @Override
  public void releasePeriod(MediaPeriod mediaPeriod) {
    MaskingMediaPeriod maskingMediaPeriod = (MaskingMediaPeriod) mediaPeriod;
    List<MaskingMediaPeriod> mediaPeriods =
        maskingMediaPeriodByAdMediaSource.get(maskingMediaPeriod.mediaSource);
    if (mediaPeriods != null) {
      mediaPeriods.remove(maskingMediaPeriod);
    }
    maskingMediaPeriod.releasePeriod();
  }

  @Override
  protected void releaseSourceInternal() {
    super.releaseSourceInternal();
    Assertions.checkNotNull(componentListener).release();
    componentListener = null;
    maskingMediaPeriodByAdMediaSource.clear();
    contentTimeline = null;
    adPlaybackState = null;
    adGroupMediaSources = new MediaSource[0][];
    adGroupTimelines = new Timeline[0][];
    mainHandler.post(adsLoader::stop);
  }

  @Override
  protected void onChildSourceInfoRefreshed(
      MediaPeriodId mediaPeriodId, MediaSource mediaSource, Timeline timeline) {
    if (mediaPeriodId.isAd()) {
      int adGroupIndex = mediaPeriodId.adGroupIndex;
      int adIndexInAdGroup = mediaPeriodId.adIndexInAdGroup;
      onAdSourceInfoRefreshed(mediaSource, adGroupIndex, adIndexInAdGroup, timeline);
    } else {
      onContentSourceInfoRefreshed(timeline);
    }
  }

  @Override
  protected @Nullable MediaPeriodId getMediaPeriodIdForChildMediaPeriodId(
      MediaPeriodId childId, MediaPeriodId mediaPeriodId) {
    // The child id for the content period is just DUMMY_CONTENT_MEDIA_PERIOD_ID. That's why we need
    // to forward the reported mediaPeriodId in this case.
    return childId.isAd() ? childId : mediaPeriodId;
  }

  // Internal methods.

  private void onAdPlaybackState(AdPlaybackState adPlaybackState) {
    if (this.adPlaybackState == null) {
      adGroupMediaSources = new MediaSource[adPlaybackState.adGroupCount][];
      Arrays.fill(adGroupMediaSources, new MediaSource[0]);
      adGroupTimelines = new Timeline[adPlaybackState.adGroupCount][];
      Arrays.fill(adGroupTimelines, new Timeline[0]);
    }
    this.adPlaybackState = adPlaybackState;
    maybeUpdateSourceInfo();
  }

  private void onContentSourceInfoRefreshed(Timeline timeline) {
    Assertions.checkArgument(timeline.getPeriodCount() == 1);
    contentTimeline = timeline;
    maybeUpdateSourceInfo();
  }

  private void onAdSourceInfoRefreshed(MediaSource mediaSource, int adGroupIndex,
      int adIndexInAdGroup, Timeline timeline) {
    Assertions.checkArgument(timeline.getPeriodCount() == 1);
    adGroupTimelines[adGroupIndex][adIndexInAdGroup] = timeline;
    List<MaskingMediaPeriod> mediaPeriods = maskingMediaPeriodByAdMediaSource.remove(mediaSource);
    if (mediaPeriods != null) {
      Object periodUid = timeline.getUidOfPeriod(/* periodIndex= */ 0);
      for (int i = 0; i < mediaPeriods.size(); i++) {
        MaskingMediaPeriod mediaPeriod = mediaPeriods.get(i);
        MediaPeriodId adSourceMediaPeriodId =
            new MediaPeriodId(periodUid, mediaPeriod.id.windowSequenceNumber);
        mediaPeriod.createPeriod(adSourceMediaPeriodId);
      }
    }
    maybeUpdateSourceInfo();
  }

  private void maybeUpdateSourceInfo() {
    Timeline contentTimeline = this.contentTimeline;
    if (adPlaybackState != null && contentTimeline != null) {
      adPlaybackState = adPlaybackState.withAdDurationsUs(getAdDurations(adGroupTimelines, period));
      Timeline timeline =
          adPlaybackState.adGroupCount == 0
              ? contentTimeline
              : new SinglePeriodAdTimeline(contentTimeline, adPlaybackState);
      refreshSourceInfo(timeline);
    }
  }

  private static long[][] getAdDurations(
      @NullableType Timeline[][] adTimelines, Timeline.Period period) {
    long[][] adDurations = new long[adTimelines.length][];
    for (int i = 0; i < adTimelines.length; i++) {
      adDurations[i] = new long[adTimelines[i].length];
      for (int j = 0; j < adTimelines[i].length; j++) {
        adDurations[i][j] =
            adTimelines[i][j] == null
                ? C.TIME_UNSET
                : adTimelines[i][j].getPeriod(/* periodIndex= */ 0, period).getDurationUs();
      }
    }
    return adDurations;
  }

  /** Listener for component events. All methods are called on the main thread. */
  private final class ComponentListener implements AdsLoader.EventListener {

    private final Handler playerHandler;

    private volatile boolean released;

    /**
     * Creates new listener which forwards ad playback states on the creating thread and all other
     * events on the external event listener thread.
     */
    public ComponentListener() {
      playerHandler = new Handler();
    }

    /** Releases the component listener. */
    public void release() {
      released = true;
      playerHandler.removeCallbacksAndMessages(null);
    }

    @Override
    public void onAdPlaybackState(final AdPlaybackState adPlaybackState) {
      if (released) {
        return;
      }
      playerHandler.post(
          () -> {
            if (released) {
              return;
            }
            AdsMediaSource.this.onAdPlaybackState(adPlaybackState);
          });
    }

    @Override
    public void onAdLoadError(final AdLoadException error, DataSpec dataSpec) {
      if (released) {
        return;
      }
      createEventDispatcher(/* mediaPeriodId= */ null)
          .loadError(
              dataSpec,
              dataSpec.uri,
              /* responseHeaders= */ Collections.emptyMap(),
              C.DATA_TYPE_AD,
              C.TRACK_TYPE_UNKNOWN,
              /* loadDurationMs= */ 0,
              /* bytesLoaded= */ 0,
              error,
              /* wasCanceled= */ true);
    }
  }

  private final class AdPrepareErrorListener implements MaskingMediaPeriod.PrepareErrorListener {

    private final Uri adUri;
    private final int adGroupIndex;
    private final int adIndexInAdGroup;

    public AdPrepareErrorListener(Uri adUri, int adGroupIndex, int adIndexInAdGroup) {
      this.adUri = adUri;
      this.adGroupIndex = adGroupIndex;
      this.adIndexInAdGroup = adIndexInAdGroup;
    }

    @Override
    public void onPrepareError(MediaPeriodId mediaPeriodId, final IOException exception) {
      createEventDispatcher(mediaPeriodId)
          .loadError(
              new DataSpec(adUri),
              adUri,
              /* responseHeaders= */ Collections.emptyMap(),
              C.DATA_TYPE_AD,
              C.TRACK_TYPE_UNKNOWN,
              /* loadDurationMs= */ 0,
              /* bytesLoaded= */ 0,
              AdLoadException.createForAd(exception),
              /* wasCanceled= */ true);
      mainHandler.post(
          () -> adsLoader.handlePrepareError(adGroupIndex, adIndexInAdGroup, exception));
    }
  }
}
