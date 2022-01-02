/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import android.os.Build;
import android.util.Log;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.concurrent.ConcurrentLinkedQueue;
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

public class GeckoHlsVideoRenderer extends GeckoHlsRendererBase {
  /*
   * By configuring these states, initialization data is provided for
   * ExoPlayer's HlsMediaSource to parse HLS bitstream and then provide samples
   * starting with an Access Unit Delimiter including SPS/PPS for TS,
   * and provide samples starting with an AUD without SPS/PPS for FMP4.
   */
  private enum RECONFIGURATION_STATE {
    NONE,
    WRITE_PENDING,
    QUEUE_PENDING
  }

  private boolean mRendererReconfigured;
  private RECONFIGURATION_STATE mRendererReconfigurationState = RECONFIGURATION_STATE.NONE;

  // A list of the formats which may be included in the bitstream.
  private Format[] mStreamFormats;
  // The max width/height/inputBufferSize for specific codec format.
  private CodecMaxValues mCodecMaxValues;
  // A temporary queue for samples whose duration is not calculated yet.
  private ConcurrentLinkedQueue<GeckoHLSSample> mDemuxedNoDurationSamples =
      new ConcurrentLinkedQueue<>();

  // Contain CSD-0(SPS)/CSD-1(PPS) information (in AnnexB format) for
  // prepending each keyframe. When video format changes, this information
  // changes accordingly.
  private byte[] mCSDInfo = null;

  public GeckoHlsVideoRenderer(final GeckoHlsPlayer.ComponentEventDispatcher eventDispatcher) {
    super(C.TRACK_TYPE_VIDEO, eventDispatcher);
    assertTrue(Build.VERSION.SDK_INT >= 16);
    LOGTAG = getClass().getSimpleName();
    DEBUG = !BuildConfig.MOZILLA_OFFICIAL;
  }

  @Override
  public final int supportsMixedMimeTypeAdaptation() {
    return ADAPTIVE_NOT_SEAMLESS;
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
     * ADAPTIVE_SEAMLESS : The Renderer can seamlessly adapt between formats.
     */
    final String mimeType = format.sampleMimeType;
    if (!MimeTypes.isVideo(mimeType)) {
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

    boolean decoderCapable = false;
    MediaCodecInfo info = null;
    for (final MediaCodecInfo i : decoderInfos) {
      if (i.isCodecSupported(format)) {
        decoderCapable = true;
        info = i;
      }
    }
    if (decoderCapable && format.width > 0 && format.height > 0) {
      if (Build.VERSION.SDK_INT < 21) {
        try {
          decoderCapable =
              format.width * format.height <= MediaCodecUtil.maxH264DecodableFrameSize();
        } catch (final MediaCodecUtil.DecoderQueryException e) {
          Log.e(LOGTAG, e.getMessage());
        }
        if (!decoderCapable) {
          if (DEBUG) {
            Log.d(LOGTAG, "Check [legacyFrameSize, " + format.width + "x" + format.height + "]");
          }
        }
      } else {
        decoderCapable =
            info.isVideoSizeAndRateSupportedV21(format.width, format.height, format.frameRate);
      }
    }

    return RendererCapabilities.create(
        decoderCapable ? FORMAT_HANDLED : FORMAT_EXCEEDS_CAPABILITIES,
        info != null && info.adaptive ? ADAPTIVE_SEAMLESS : ADAPTIVE_NOT_SEAMLESS,
        TUNNELING_NOT_SUPPORTED);
  }

  @Override
  protected final void createInputBuffer() throws ExoPlaybackException {
    assertTrue(mFormats.size() > 0);
    // Calculate maximum size which might be used for target format.
    final Format currentFormat = mFormats.get(mFormats.size() - 1);
    mCodecMaxValues = getCodecMaxValues(currentFormat, mStreamFormats);
    // Create a buffer with maximal size for reading source.
    // Note : Though we are able to dynamically enlarge buffer size by
    // creating DecoderInputBuffer with specific BufferReplacementMode, we
    // still allocate a calculated max size buffer for it at first to reduce
    // runtime overhead.
    try {
      mInputBuffer = ByteBuffer.wrap(new byte[mCodecMaxValues.inputSize]);
    } catch (final OutOfMemoryError e) {
      Log.e(LOGTAG, "cannot allocate input buffer of size " + mCodecMaxValues.inputSize, e);
      throw ExoPlaybackException.createForRenderer(
          new Exception(e),
          getIndex(),
          mFormats.isEmpty() ? null : getFormat(mFormats.size() - 1),
          RendererCapabilities.FORMAT_HANDLED);
    }
  }

  @Override
  protected void resetRenderer() {
    if (DEBUG) {
      Log.d(LOGTAG, "[resetRenderer] mInitialized = " + mInitialized);
    }
    if (mInitialized) {
      mRendererReconfigured = false;
      mRendererReconfigurationState = RECONFIGURATION_STATE.NONE;
      mInputBuffer = null;
      mCSDInfo = null;
      mInitialized = false;
    }
  }

  @Override
  protected void handleReconfiguration(final DecoderInputBuffer bufferForRead) {
    // For adaptive reconfiguration OMX decoders expect all reconfiguration
    // data to be supplied at the start of the buffer that also contains
    // the first frame in the new format.
    assertTrue(mFormats.size() > 0);
    if (mRendererReconfigurationState == RECONFIGURATION_STATE.WRITE_PENDING) {
      if (bufferForRead.data == null) {
        if (DEBUG) {
          Log.d(LOGTAG, "[feedInput][WRITE_PENDING] bufferForRead.data is not initialized.");
        }
        return;
      }
      if (DEBUG) {
        Log.d(LOGTAG, "[feedInput][WRITE_PENDING] put initialization data");
      }
      final Format currentFormat = mFormats.get(mFormats.size() - 1);
      for (int i = 0; i < currentFormat.initializationData.size(); i++) {
        final byte[] data = currentFormat.initializationData.get(i);
        bufferForRead.data.put(data);
      }
      mRendererReconfigurationState = RECONFIGURATION_STATE.QUEUE_PENDING;
    }
  }

  @Override
  protected void handleFormatRead(final DecoderInputBuffer bufferForRead)
      throws ExoPlaybackException {
    if (mRendererReconfigurationState == RECONFIGURATION_STATE.QUEUE_PENDING) {
      if (DEBUG) {
        Log.d(LOGTAG, "[feedInput][QUEUE_PENDING] 2 formats in a row.");
      }
      // We received two formats in a row. Clear the current buffer of any reconfiguration data
      // associated with the first format.
      bufferForRead.clear();
      mRendererReconfigurationState = RECONFIGURATION_STATE.WRITE_PENDING;
    }
    onInputFormatChanged(mFormatHolder.format);
  }

  @Override
  protected void handleEndOfStream(final DecoderInputBuffer bufferForRead) {
    if (mRendererReconfigurationState == RECONFIGURATION_STATE.QUEUE_PENDING) {
      if (DEBUG) {
        Log.d(LOGTAG, "[feedInput][QUEUE_PENDING] isEndOfStream.");
      }
      // We received a new format immediately before the end of the stream. We need to clear
      // the corresponding reconfiguration data from the current buffer, but re-write it into
      // a subsequent buffer if there are any (e.g. if the user seeks backwards).
      bufferForRead.clear();
      mRendererReconfigurationState = RECONFIGURATION_STATE.WRITE_PENDING;
    }
    mInputStreamEnded = true;
    final GeckoHLSSample sample = GeckoHLSSample.EOS;
    calculatDuration(sample);
  }

  @Override
  protected void handleSamplePreparation(final DecoderInputBuffer bufferForRead) {
    final int csdInfoSize = mCSDInfo != null ? mCSDInfo.length : 0;
    final int dataSize = bufferForRead.data.limit();
    final int size = bufferForRead.isKeyFrame() ? csdInfoSize + dataSize : dataSize;
    final byte[] realData = new byte[size];
    if (bufferForRead.isKeyFrame()) {
      // Prepend the CSD information to the sample if it's a key frame.
      System.arraycopy(mCSDInfo, 0, realData, 0, csdInfoSize);
      bufferForRead.data.get(realData, csdInfoSize, dataSize);
    } else {
      bufferForRead.data.get(realData, 0, dataSize);
    }
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

    assertTrue(mFormats.size() > 0);
    // We add a new format in the list once format changes, so the formatIndex
    // should indicate to the last(latest) format.
    final GeckoHLSSample sample =
        GeckoHLSSample.create(buffer, bufferInfo, cryptoInfo, mFormats.size() - 1);

    // There's no duration information from the ExoPlayer's sample, we need
    // to calculate it.
    calculatDuration(sample);
    mRendererReconfigurationState = RECONFIGURATION_STATE.NONE;
  }

  @Override
  protected void onPositionReset(final long positionUs, final boolean joining) {
    super.onPositionReset(positionUs, joining);
    if (mInitialized && mRendererReconfigured && mFormats.size() != 0) {
      if (DEBUG) {
        Log.d(LOGTAG, "[onPositionReset] WRITE_PENDING");
      }
      // Any reconfiguration data that we put shortly before the reset
      // may be invalid. We avoid this issue by sending reconfiguration
      // data following every position reset.
      mRendererReconfigurationState = RECONFIGURATION_STATE.WRITE_PENDING;
    }
  }

  @Override
  protected boolean clearInputSamplesQueue() {
    if (DEBUG) {
      Log.d(LOGTAG, "clearInputSamplesQueue");
    }
    mDemuxedInputSamples.clear();
    mDemuxedNoDurationSamples.clear();
    return true;
  }

  @Override
  protected boolean canReconfigure(final Format oldFormat, final Format newFormat) {
    final boolean canReconfig =
        areAdaptationCompatible(oldFormat, newFormat)
            && newFormat.width <= mCodecMaxValues.width
            && newFormat.height <= mCodecMaxValues.height
            && newFormat.maxInputSize <= mCodecMaxValues.inputSize;
    if (DEBUG) {
      Log.d(LOGTAG, "[canReconfigure] : " + canReconfig);
    }
    return canReconfig;
  }

  @Override
  protected void prepareReconfiguration() {
    if (DEBUG) {
      Log.d(LOGTAG, "[onInputFormatChanged] starting reconfiguration !");
    }
    mRendererReconfigured = true;
    mRendererReconfigurationState = RECONFIGURATION_STATE.WRITE_PENDING;
  }

  @Override
  protected void updateCSDInfo(final Format format) {
    int size = 0;
    for (int i = 0; i < format.initializationData.size(); i++) {
      size += format.initializationData.get(i).length;
    }
    int startPos = 0;
    mCSDInfo = new byte[size];
    for (int i = 0; i < format.initializationData.size(); i++) {
      final byte[] data = format.initializationData.get(i);
      System.arraycopy(data, 0, mCSDInfo, startPos, data.length);
      startPos += data.length;
    }
    if (DEBUG) {
      Log.d(LOGTAG, "mCSDInfo [" + Utils.bytesToHex(mCSDInfo) + "]");
    }
  }

  @Override
  protected void notifyPlayerInputFormatChanged(final Format newFormat) {
    mPlayerEventDispatcher.onVideoInputFormatChanged(newFormat);
  }

  private void calculateSamplesWithin(final GeckoHLSSample[] samples, final int range) {
    // Calculate the first 'range' elements.
    for (int i = 0; i < range; i++) {
      // Comparing among samples in the window.
      for (int j = -2; j < 14; j++) {
        if (i + j >= 0
            && i + j < range
            && samples[i + j].info.presentationTimeUs > samples[i].info.presentationTimeUs) {
          samples[i].duration =
              Math.min(
                  samples[i].duration,
                  samples[i + j].info.presentationTimeUs - samples[i].info.presentationTimeUs);
        }
      }
    }
  }

  private void calculatDuration(final GeckoHLSSample inputSample) {
    /*
     * NOTE :
     * Since we customized renderer as a demuxer. Here we're not able to
     * obtain duration from the DecoderInputBuffer as there's no duration inside.
     * So we calcualte it by referring to nearby samples' timestamp.
     * A temporary queue |mDemuxedNoDurationSamples| is used to queue demuxed
     * samples from HlsMediaSource which have no duration information at first.
     * We're choosing 16 as the comparing window size, because it's commonly
     * used as a GOP size.
     * Considering there're 16 demuxed samples in the _no duration_ queue already,
     * e.g. |-2|-1|0|1|2|3|4|5|6|...|13|
     * Once a new demuxed(No duration) sample X (17th) is put into the
     * temporary queue,
     * e.g. |-2|-1|0|1|2|3|4|5|6|...|13|X|
     * we are able to calculate the correct duration for sample 0 by finding
     * the closest but greater pts than sample 0 among these 16 samples,
     * here, let's say sample -2 to 13.
     */
    if (inputSample != null) {
      mDemuxedNoDurationSamples.offer(inputSample);
    }
    final int sizeOfNoDura = mDemuxedNoDurationSamples.size();
    // A calculation window we've ever found suitable for both HLS TS & FMP4.
    final int range = sizeOfNoDura >= 17 ? 17 : sizeOfNoDura;
    final GeckoHLSSample[] inputArray =
        mDemuxedNoDurationSamples.toArray(new GeckoHLSSample[sizeOfNoDura]);
    if (range >= 17 && !mInputStreamEnded) {
      calculateSamplesWithin(inputArray, range);

      final GeckoHLSSample toQueue = mDemuxedNoDurationSamples.poll();
      mDemuxedInputSamples.offer(toQueue);
      if (BuildConfig.DEBUG_BUILD) {
        Log.d(
            LOGTAG,
            "Demuxed sample PTS : "
                + toQueue.info.presentationTimeUs
                + ", duration :"
                + toQueue.duration
                + ", isKeyFrame("
                + toQueue.isKeyFrame()
                + ", formatIndex("
                + toQueue.formatIndex
                + "), queue size : "
                + mDemuxedInputSamples.size()
                + ", NoDuQueue size : "
                + mDemuxedNoDurationSamples.size());
      }
    } else if (mInputStreamEnded) {
      calculateSamplesWithin(inputArray, sizeOfNoDura);

      // NOTE : We're not able to calculate the duration for the last sample.
      //        A workaround here is to assign a close duration to it.
      long prevDuration = 33333;
      GeckoHLSSample sample = null;
      for (sample = mDemuxedNoDurationSamples.poll();
          sample != null;
          sample = mDemuxedNoDurationSamples.poll()) {
        if (sample.duration == Long.MAX_VALUE) {
          sample.duration = prevDuration;
          if (DEBUG) {
            Log.d(LOGTAG, "Adjust the PTS of the last sample to " + sample.duration + " (us)");
          }
        }
        prevDuration = sample.duration;
        if (DEBUG) {
          Log.d(
              LOGTAG,
              "last loop to offer samples - PTS : "
                  + sample.info.presentationTimeUs
                  + ", Duration : "
                  + sample.duration
                  + ", isEOS : "
                  + sample.isEOS());
        }
        mDemuxedInputSamples.offer(sample);
      }
    }
  }

  // Return the time of first keyframe sample in the queue.
  // If there's no key frame in the queue, return the MAX_VALUE so
  // MFR won't mistake for that which the decode is getting slow.
  public long getNextKeyFrameTime() {
    long nextKeyFrameTime = Long.MAX_VALUE;
    for (final GeckoHLSSample sample : mDemuxedInputSamples) {
      if (sample != null && (sample.info.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
        nextKeyFrameTime = sample.info.presentationTimeUs;
        break;
      }
    }
    return nextKeyFrameTime;
  }

  @Override
  protected void onStreamChanged(final Format[] formats, final long offsetUs) {
    mStreamFormats = formats;
  }

  private static CodecMaxValues getCodecMaxValues(
      final Format format, final Format[] streamFormats) {
    int maxWidth = format.width;
    int maxHeight = format.height;
    int maxInputSize = getMaxInputSize(format);
    for (final Format streamFormat : streamFormats) {
      if (areAdaptationCompatible(format, streamFormat)) {
        maxWidth = Math.max(maxWidth, streamFormat.width);
        maxHeight = Math.max(maxHeight, streamFormat.height);
        maxInputSize = Math.max(maxInputSize, getMaxInputSize(streamFormat));
      }
    }
    return new CodecMaxValues(maxWidth, maxHeight, maxInputSize);
  }

  private static int getMaxInputSize(final Format format) {
    if (format.maxInputSize != Format.NO_VALUE) {
      // The format defines an explicit maximum input size.
      return format.maxInputSize;
    }

    if (format.width == Format.NO_VALUE || format.height == Format.NO_VALUE) {
      // We can't infer a maximum input size without video dimensions.
      return Format.NO_VALUE;
    }

    // Attempt to infer a maximum input size from the format.
    final int maxPixels;
    final int minCompressionRatio;
    switch (format.sampleMimeType) {
      case MimeTypes.VIDEO_H264:
        // Round up width/height to an integer number of macroblocks.
        maxPixels = ((format.width + 15) / 16) * ((format.height + 15) / 16) * 16 * 16;
        minCompressionRatio = 2;
        break;
      default:
        // Leave the default max input size.
        return Format.NO_VALUE;
    }
    // Estimate the maximum input size assuming three channel 4:2:0 subsampled input frames.
    return (maxPixels * 3) / (2 * minCompressionRatio);
  }

  private static boolean areAdaptationCompatible(final Format first, final Format second) {
    return first.sampleMimeType.equals(second.sampleMimeType)
        && getRotationDegrees(first) == getRotationDegrees(second);
  }

  private static int getRotationDegrees(final Format format) {
    return format.rotationDegrees == Format.NO_VALUE ? 0 : format.rotationDegrees;
  }

  private static final class CodecMaxValues {
    public final int width;
    public final int height;
    public final int inputSize;

    public CodecMaxValues(final int width, final int height, final int inputSize) {
      this.width = width;
      this.height = height;
      this.inputSize = inputSize;
    }
  }
}
