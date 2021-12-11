/*
 * Copyright (C) 2016 The Android Open Source Project
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
package org.mozilla.thirdparty.com.google.android.exoplayer2;

import android.os.Looper;
import androidx.annotation.Nullable;
import org.mozilla.thirdparty.com.google.android.exoplayer2.decoder.DecoderInputBuffer;
import org.mozilla.thirdparty.com.google.android.exoplayer2.drm.DrmInitData;
import org.mozilla.thirdparty.com.google.android.exoplayer2.drm.DrmSession;
import org.mozilla.thirdparty.com.google.android.exoplayer2.drm.DrmSessionManager;
import org.mozilla.thirdparty.com.google.android.exoplayer2.drm.ExoMediaCrypto;
import org.mozilla.thirdparty.com.google.android.exoplayer2.source.SampleStream;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Assertions;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.MediaClock;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Util;
import java.io.IOException;

/**
 * An abstract base class suitable for most {@link Renderer} implementations.
 */
public abstract class BaseRenderer implements Renderer, RendererCapabilities {

  private final int trackType;
  private final FormatHolder formatHolder;

  private RendererConfiguration configuration;
  private int index;
  private int state;
  private SampleStream stream;
  private Format[] streamFormats;
  private long streamOffsetUs;
  private long readingPositionUs;
  private boolean streamIsFinal;
  private boolean throwRendererExceptionIsExecuting;

  /**
   * @param trackType The track type that the renderer handles. One of the {@link C}
   * {@code TRACK_TYPE_*} constants.
   */
  public BaseRenderer(int trackType) {
    this.trackType = trackType;
    formatHolder = new FormatHolder();
    readingPositionUs = C.TIME_END_OF_SOURCE;
  }

  @Override
  public final int getTrackType() {
    return trackType;
  }

  @Override
  public final RendererCapabilities getCapabilities() {
    return this;
  }

  @Override
  public final void setIndex(int index) {
    this.index = index;
  }

  @Override
  @Nullable
  public MediaClock getMediaClock() {
    return null;
  }

  @Override
  public final int getState() {
    return state;
  }

  @Override
  public final void enable(RendererConfiguration configuration, Format[] formats,
      SampleStream stream, long positionUs, boolean joining, long offsetUs)
      throws ExoPlaybackException {
    Assertions.checkState(state == STATE_DISABLED);
    this.configuration = configuration;
    state = STATE_ENABLED;
    onEnabled(joining);
    replaceStream(formats, stream, offsetUs);
    onPositionReset(positionUs, joining);
  }

  @Override
  public final void start() throws ExoPlaybackException {
    Assertions.checkState(state == STATE_ENABLED);
    state = STATE_STARTED;
    onStarted();
  }

  @Override
  public final void replaceStream(Format[] formats, SampleStream stream, long offsetUs)
      throws ExoPlaybackException {
    Assertions.checkState(!streamIsFinal);
    this.stream = stream;
    readingPositionUs = offsetUs;
    streamFormats = formats;
    streamOffsetUs = offsetUs;
    onStreamChanged(formats, offsetUs);
  }

  @Override
  @Nullable
  public final SampleStream getStream() {
    return stream;
  }

  @Override
  public final boolean hasReadStreamToEnd() {
    return readingPositionUs == C.TIME_END_OF_SOURCE;
  }

  @Override
  public final long getReadingPositionUs() {
    return readingPositionUs;
  }

  @Override
  public final void setCurrentStreamFinal() {
    streamIsFinal = true;
  }

  @Override
  public final boolean isCurrentStreamFinal() {
    return streamIsFinal;
  }

  @Override
  public final void maybeThrowStreamError() throws IOException {
    stream.maybeThrowError();
  }

  @Override
  public final void resetPosition(long positionUs) throws ExoPlaybackException {
    streamIsFinal = false;
    readingPositionUs = positionUs;
    onPositionReset(positionUs, false);
  }

  @Override
  public final void stop() throws ExoPlaybackException {
    Assertions.checkState(state == STATE_STARTED);
    state = STATE_ENABLED;
    onStopped();
  }

  @Override
  public final void disable() {
    Assertions.checkState(state == STATE_ENABLED);
    formatHolder.clear();
    state = STATE_DISABLED;
    stream = null;
    streamFormats = null;
    streamIsFinal = false;
    onDisabled();
  }

  @Override
  public final void reset() {
    Assertions.checkState(state == STATE_DISABLED);
    formatHolder.clear();
    onReset();
  }

  // RendererCapabilities implementation.

  @Override
  @AdaptiveSupport
  public int supportsMixedMimeTypeAdaptation() throws ExoPlaybackException {
    return ADAPTIVE_NOT_SUPPORTED;
  }

  // PlayerMessage.Target implementation.

  @Override
  public void handleMessage(int what, @Nullable Object object) throws ExoPlaybackException {
    // Do nothing.
  }

  // Methods to be overridden by subclasses.

  /**
   * Called when the renderer is enabled.
   * <p>
   * The default implementation is a no-op.
   *
   * @param joining Whether this renderer is being enabled to join an ongoing playback.
   * @throws ExoPlaybackException If an error occurs.
   */
  protected void onEnabled(boolean joining) throws ExoPlaybackException {
    // Do nothing.
  }

  /**
   * Called when the renderer's stream has changed. This occurs when the renderer is enabled after
   * {@link #onEnabled(boolean)} has been called, and also when the stream has been replaced whilst
   * the renderer is enabled or started.
   * <p>
   * The default implementation is a no-op.
   *
   * @param formats The enabled formats.
   * @param offsetUs The offset that will be added to the timestamps of buffers read via
   *     {@link #readSource(FormatHolder, DecoderInputBuffer, boolean)} so that decoder input
   *     buffers have monotonically increasing timestamps.
   * @throws ExoPlaybackException If an error occurs.
   */
  protected void onStreamChanged(Format[] formats, long offsetUs) throws ExoPlaybackException {
    // Do nothing.
  }

  /**
   * Called when the position is reset. This occurs when the renderer is enabled after
   * {@link #onStreamChanged(Format[], long)} has been called, and also when a position
   * discontinuity is encountered.
   * <p>
   * After a position reset, the renderer's {@link SampleStream} is guaranteed to provide samples
   * starting from a key frame.
   * <p>
   * The default implementation is a no-op.
   *
   * @param positionUs The new playback position in microseconds.
   * @param joining Whether this renderer is being enabled to join an ongoing playback.
   * @throws ExoPlaybackException If an error occurs.
   */
  protected void onPositionReset(long positionUs, boolean joining) throws ExoPlaybackException {
    // Do nothing.
  }

  /**
   * Called when the renderer is started.
   * <p>
   * The default implementation is a no-op.
   *
   * @throws ExoPlaybackException If an error occurs.
   */
  protected void onStarted() throws ExoPlaybackException {
    // Do nothing.
  }

  /**
   * Called when the renderer is stopped.
   * <p>
   * The default implementation is a no-op.
   *
   * @throws ExoPlaybackException If an error occurs.
   */
  protected void onStopped() throws ExoPlaybackException {
    // Do nothing.
  }

  /**
   * Called when the renderer is disabled.
   * <p>
   * The default implementation is a no-op.
   */
  protected void onDisabled() {
    // Do nothing.
  }

  /**
   * Called when the renderer is reset.
   *
   * <p>The default implementation is a no-op.
   */
  protected void onReset() {
    // Do nothing.
  }

  // Methods to be called by subclasses.

  /** Returns a clear {@link FormatHolder}. */
  protected final FormatHolder getFormatHolder() {
    formatHolder.clear();
    return formatHolder;
  }

  /** Returns the formats of the currently enabled stream. */
  protected final Format[] getStreamFormats() {
    return streamFormats;
  }

  /**
   * Returns the configuration set when the renderer was most recently enabled.
   */
  protected final RendererConfiguration getConfiguration() {
    return configuration;
  }

  /** Returns a {@link DrmSession} ready for assignment, handling resource management. */
  @Nullable
  protected final <T extends ExoMediaCrypto> DrmSession<T> getUpdatedSourceDrmSession(
      @Nullable Format oldFormat,
      Format newFormat,
      @Nullable DrmSessionManager<T> drmSessionManager,
      @Nullable DrmSession<T> existingSourceSession)
      throws ExoPlaybackException {
    boolean drmInitDataChanged =
        !Util.areEqual(newFormat.drmInitData, oldFormat == null ? null : oldFormat.drmInitData);
    if (!drmInitDataChanged) {
      return existingSourceSession;
    }
    @Nullable DrmSession<T> newSourceDrmSession = null;
    if (newFormat.drmInitData != null) {
      if (drmSessionManager == null) {
        throw createRendererException(
            new IllegalStateException("Media requires a DrmSessionManager"), newFormat);
      }
      newSourceDrmSession =
          drmSessionManager.acquireSession(
              Assertions.checkNotNull(Looper.myLooper()), newFormat.drmInitData);
    }
    if (existingSourceSession != null) {
      existingSourceSession.release();
    }
    return newSourceDrmSession;
  }

  /**
   * Returns the index of the renderer within the player.
   */
  protected final int getIndex() {
    return index;
  }

  /**
   * Creates an {@link ExoPlaybackException} of type {@link ExoPlaybackException#TYPE_RENDERER} for
   * this renderer.
   *
   * @param cause The cause of the exception.
   * @param format The current format used by the renderer. May be null.
   */
  protected final ExoPlaybackException createRendererException(
      Exception cause, @Nullable Format format) {
    @FormatSupport int formatSupport = RendererCapabilities.FORMAT_HANDLED;
    if (format != null && !throwRendererExceptionIsExecuting) {
      // Prevent recursive re-entry from subclass supportsFormat implementations.
      throwRendererExceptionIsExecuting = true;
      try {
        formatSupport = RendererCapabilities.getFormatSupport(supportsFormat(format));
      } catch (ExoPlaybackException e) {
        // Ignore, we are already failing.
      } finally {
        throwRendererExceptionIsExecuting = false;
      }
    }
    return ExoPlaybackException.createForRenderer(cause, getIndex(), format, formatSupport);
  }

  /**
   * Reads from the enabled upstream source. If the upstream source has been read to the end then
   * {@link C#RESULT_BUFFER_READ} is only returned if {@link #setCurrentStreamFinal()} has been
   * called. {@link C#RESULT_NOTHING_READ} is returned otherwise.
   *
   * @param formatHolder A {@link FormatHolder} to populate in the case of reading a format.
   * @param buffer A {@link DecoderInputBuffer} to populate in the case of reading a sample or the
   *     end of the stream. If the end of the stream has been reached, the {@link
   *     C#BUFFER_FLAG_END_OF_STREAM} flag will be set on the buffer.
   * @param formatRequired Whether the caller requires that the format of the stream be read even if
   *     it's not changing. A sample will never be read if set to true, however it is still possible
   *     for the end of stream or nothing to be read.
   * @return The result, which can be {@link C#RESULT_NOTHING_READ}, {@link C#RESULT_FORMAT_READ} or
   *     {@link C#RESULT_BUFFER_READ}.
   */
  protected final int readSource(
      FormatHolder formatHolder, DecoderInputBuffer buffer, boolean formatRequired) {
    int result = stream.readData(formatHolder, buffer, formatRequired);
    if (result == C.RESULT_BUFFER_READ) {
      if (buffer.isEndOfStream()) {
        readingPositionUs = C.TIME_END_OF_SOURCE;
        return streamIsFinal ? C.RESULT_BUFFER_READ : C.RESULT_NOTHING_READ;
      }
      buffer.timeUs += streamOffsetUs;
      readingPositionUs = Math.max(readingPositionUs, buffer.timeUs);
    } else if (result == C.RESULT_FORMAT_READ) {
      Format format = formatHolder.format;
      if (format.subsampleOffsetUs != Format.OFFSET_SAMPLE_RELATIVE) {
        format = format.copyWithSubsampleOffsetUs(format.subsampleOffsetUs + streamOffsetUs);
        formatHolder.format = format;
      }
    }
    return result;
  }

  /**
   * Attempts to skip to the keyframe before the specified position, or to the end of the stream if
   * {@code positionUs} is beyond it.
   *
   * @param positionUs The position in microseconds.
   * @return The number of samples that were skipped.
   */
  protected int skipSource(long positionUs) {
    return stream.skipData(positionUs - streamOffsetUs);
  }

  /**
   * Returns whether the upstream source is ready.
   */
  protected final boolean isSourceReady() {
    return hasReadStreamToEnd() ? streamIsFinal : stream.isReady();
  }

  /**
   * Returns whether {@code drmSessionManager} supports the specified {@code drmInitData}, or true
   * if {@code drmInitData} is null.
   *
   * @param drmSessionManager The drm session manager.
   * @param drmInitData {@link DrmInitData} of the format to check for support.
   * @return Whether {@code drmSessionManager} supports the specified {@code drmInitData}, or
   *     true if {@code drmInitData} is null.
   */
  protected static boolean supportsFormatDrm(@Nullable DrmSessionManager<?> drmSessionManager,
      @Nullable DrmInitData drmInitData) {
    if (drmInitData == null) {
      // Content is unencrypted.
      return true;
    } else if (drmSessionManager == null) {
      // Content is encrypted, but no drm session manager is available.
      return false;
    }
    return drmSessionManager.canAcquireSession(drmInitData);
  }

}
