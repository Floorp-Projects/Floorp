/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import android.util.Log;
import java.nio.ByteBuffer;
import java.util.List;
import org.mozilla.geckoview.BuildConfig;
import org.mozilla.thirdparty.com.google.android.exoplayer2.C;
import org.mozilla.thirdparty.com.google.android.exoplayer2.ExoPlaybackException;
import org.mozilla.thirdparty.com.google.android.exoplayer2.Format;
import org.mozilla.thirdparty.com.google.android.exoplayer2.RendererCapabilities;
import org.mozilla.thirdparty.com.google.android.exoplayer2.decoder.DecoderInputBuffer;
import org.mozilla.thirdparty.com.google.android.exoplayer2.mediacodec.MediaCodecInfo;
import org.mozilla.thirdparty.com.google.android.exoplayer2.mediacodec.MediaCodecSelector;
import org.mozilla.thirdparty.com.google.android.exoplayer2.mediacodec.MediaCodecUtil;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.MimeTypes;

public class GeckoHlsAudioRenderer extends GeckoHlsRendererBase {
  public GeckoHlsAudioRenderer(final GeckoHlsPlayer.ComponentEventDispatcher eventDispatcher) {
    super(C.TRACK_TYPE_AUDIO, eventDispatcher);
    LOGTAG = getClass().getSimpleName();
    DEBUG = !BuildConfig.MOZILLA_OFFICIAL;
  }

  @Override
  public final int supportsFormat(final Format format) {
    /*
     * FORMAT_EXCEEDS_CAPABILITIES : The Renderer is capable of rendering
     *                               formats with the same mime type, but
     *                               the properties of the format exceed
     *                               the renderer's capability.
     * FORMAT_UNSUPPORTED_SUBTYPE : The Renderer is a general purpose
     *                              renderer for formats of the same
     *                              top-level type, but is not capable of
     *                              rendering the format or any other format
     *                              with the same mime type because the
     *                              sub-type is not supported.
     * FORMAT_UNSUPPORTED_TYPE : The Renderer is not capable of rendering
     *                           the format, either because it does not support
     *                           the format's top-level type, or because it's
     *                           a specialized renderer for a different mime type.
     * ADAPTIVE_NOT_SEAMLESS : The Renderer can adapt between formats,
     *                         but may suffer a brief discontinuity (~50-100ms)
     *                         when adaptation occurs.
     */
    final String mimeType = format.sampleMimeType;
    if (!MimeTypes.isAudio(mimeType)) {
      return RendererCapabilities.create(FORMAT_UNSUPPORTED_TYPE);
    }
    List<MediaCodecInfo> decoderInfos = null;
    try {
      final MediaCodecSelector mediaCodecSelector = MediaCodecSelector.DEFAULT;
      decoderInfos = mediaCodecSelector.getDecoderInfos(mimeType, false, false);
    } catch (final MediaCodecUtil.DecoderQueryException e) {
      Log.e(LOGTAG, e.getMessage());
    }
    if (decoderInfos == null || decoderInfos.isEmpty()) {
      return RendererCapabilities.create(FORMAT_UNSUPPORTED_SUBTYPE);
    }
    final MediaCodecInfo info = decoderInfos.get(0);
    /*
     *  Note : If the code can make it to this place, ExoPlayer assumes
     *         support for unknown sampleRate and channelCount when
     *         SDK version is less than 21, otherwise, further check is needed
     *         if there's no sampleRate/channelCount in format.
     */
    final boolean decoderCapable =
        ((format.sampleRate == Format.NO_VALUE
                || info.isAudioSampleRateSupportedV21(format.sampleRate))
            && (format.channelCount == Format.NO_VALUE
                || info.isAudioChannelCountSupportedV21(format.channelCount)));
    return RendererCapabilities.create(
        decoderCapable ? FORMAT_HANDLED : FORMAT_EXCEEDS_CAPABILITIES,
        ADAPTIVE_NOT_SEAMLESS,
        TUNNELING_NOT_SUPPORTED);
  }

  @Override
  protected final void createInputBuffer() {
    // We're not able to estimate the size for audio from format. So we rely
    // on the dynamic allocation mechanism provided in DecoderInputBuffer.
    mInputBuffer = null;
  }

  @Override
  protected void resetRenderer() {
    mInputBuffer = null;
    mInitialized = false;
  }

  @Override
  protected void handleReconfiguration(final DecoderInputBuffer bufferForRead) {
    // Do nothing
  }

  @Override
  protected void handleFormatRead(final DecoderInputBuffer bufferForRead)
      throws ExoPlaybackException {
    onInputFormatChanged(mFormatHolder.format);
  }

  @Override
  protected void handleEndOfStream(final DecoderInputBuffer bufferForRead) {
    mInputStreamEnded = true;
    mDemuxedInputSamples.offer(GeckoHLSSample.EOS);
  }

  @Override
  protected void handleSamplePreparation(final DecoderInputBuffer bufferForRead) {
    final int size = bufferForRead.data.limit();
    final byte[] realData = new byte[size];
    bufferForRead.data.get(realData, 0, size);
    final ByteBuffer buffer = ByteBuffer.wrap(realData);
    mInputBuffer = bufferForRead.data;
    mInputBuffer.clear();

    final CryptoInfo cryptoInfo =
        bufferForRead.isEncrypted() ? bufferForRead.cryptoInfo.getFrameworkCryptoInfoV16() : null;
    final BufferInfo bufferInfo = new BufferInfo();
    // Flags in DecoderInputBuffer are synced with MediaCodec Buffer flags.
    int flags = 0;
    flags |= bufferForRead.isKeyFrame() ? MediaCodec.BUFFER_FLAG_KEY_FRAME : 0;
    flags |= bufferForRead.isEndOfStream() ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0;
    bufferInfo.set(0, size, bufferForRead.timeUs, flags);

    assertTrue(mFormats.size() >= 0);
    // We add a new format in the list once format changes, so the formatIndex
    // should indicate to the last(latest) format.
    final GeckoHLSSample sample =
        GeckoHLSSample.create(buffer, bufferInfo, cryptoInfo, mFormats.size() - 1);

    mDemuxedInputSamples.offer(sample);

    if (BuildConfig.DEBUG_BUILD) {
      Log.d(
          LOGTAG,
          "Demuxed sample PTS : "
              + sample.info.presentationTimeUs
              + ", duration :"
              + sample.duration
              + ", formatIndex("
              + sample.formatIndex
              + "), queue size : "
              + mDemuxedInputSamples.size());
    }
  }

  @Override
  protected boolean clearInputSamplesQueue() {
    if (DEBUG) {
      Log.d(LOGTAG, "clearInputSamplesQueue");
    }
    mDemuxedInputSamples.clear();
    return true;
  }

  @Override
  protected void notifyPlayerInputFormatChanged(final Format newFormat) {
    mPlayerEventDispatcher.onAudioInputFormatChanged(newFormat);
  }
}
