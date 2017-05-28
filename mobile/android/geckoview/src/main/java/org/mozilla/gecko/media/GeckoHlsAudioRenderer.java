/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import android.os.Handler;
import android.util.Log;

import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.RendererCapabilities;
import com.google.android.exoplayer2.decoder.DecoderInputBuffer;
import com.google.android.exoplayer2.mediacodec.MediaCodecInfo;
import com.google.android.exoplayer2.mediacodec.MediaCodecSelector;
import com.google.android.exoplayer2.mediacodec.MediaCodecUtil;
import com.google.android.exoplayer2.util.MimeTypes;

import java.nio.ByteBuffer;

import org.mozilla.gecko.AppConstants.Versions;

public class GeckoHlsAudioRenderer extends GeckoHlsRendererBase {
    public GeckoHlsAudioRenderer(GeckoHlsPlayer.ComponentEventDispatcher eventDispatcher) {
        super(C.TRACK_TYPE_AUDIO, eventDispatcher);
        assertTrue(Versions.feature16Plus);
        LOGTAG = getClass().getSimpleName();
        DEBUG = false;
    }

    @Override
    public final int supportsFormat(Format format) {
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
        String mimeType = format.sampleMimeType;
        if (!MimeTypes.isAudio(mimeType)) {
            return RendererCapabilities.FORMAT_UNSUPPORTED_TYPE;
        }
        MediaCodecInfo decoderInfo = null;
        try {
            MediaCodecSelector mediaCodecSelector = MediaCodecSelector.DEFAULT;
            decoderInfo = mediaCodecSelector.getDecoderInfo(mimeType, false);
        } catch (MediaCodecUtil.DecoderQueryException e) {
            Log.e(LOGTAG, e.getMessage());
        }
        if (decoderInfo == null) {
            return RendererCapabilities.FORMAT_UNSUPPORTED_SUBTYPE;
        }
        /*
         *  Note : If the code can make it to this place, ExoPlayer assumes
         *         support for unknown sampleRate and channelCount when
         *         SDK version is less than 21, otherwise, further check is needed
         *         if there's no sampleRate/channelCount in format.
         */
        boolean decoderCapable = Versions.preLollipop ||
                                 ((format.sampleRate == Format.NO_VALUE ||
                                  decoderInfo.isAudioSampleRateSupportedV21(format.sampleRate)) &&
                                 (format.channelCount == Format.NO_VALUE ||
                                  decoderInfo.isAudioChannelCountSupportedV21(format.channelCount)));
        int formatSupport = decoderCapable ?
            RendererCapabilities.FORMAT_HANDLED :
            RendererCapabilities.FORMAT_EXCEEDS_CAPABILITIES;
        return RendererCapabilities.ADAPTIVE_NOT_SEAMLESS | formatSupport;
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
    protected void handleReconfiguration(DecoderInputBuffer bufferForRead) {
        // Do nothing
    }

    @Override
    protected void handleFormatRead(DecoderInputBuffer bufferForRead) {
        onInputFormatChanged(mFormatHolder.format);
    }

    @Override
    protected void handleEndOfStream(DecoderInputBuffer bufferForRead) {
        mInputStreamEnded = true;
        mDemuxedInputSamples.offer(GeckoHlsSample.EOS);
    }

    @Override
    protected void handleSamplePreparation(DecoderInputBuffer bufferForRead) {
        int size = bufferForRead.data.limit();
        byte[] realData = new byte[size];
        bufferForRead.data.get(realData, 0, size);
        ByteBuffer buffer = ByteBuffer.wrap(realData);
        mInputBuffer = bufferForRead.data;
        mInputBuffer.clear();

        CryptoInfo cryptoInfo = bufferForRead.isEncrypted() ? bufferForRead.cryptoInfo.getFrameworkCryptoInfoV16() : null;
        BufferInfo bufferInfo = new BufferInfo();
        // Flags in DecoderInputBuffer are synced with MediaCodec Buffer flags.
        int flags = 0;
        flags |= bufferForRead.isKeyFrame() ? MediaCodec.BUFFER_FLAG_KEY_FRAME : 0;
        flags |= bufferForRead.isEndOfStream() ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0;
        bufferInfo.set(0, size, bufferForRead.timeUs, flags);

        assertTrue(mFormats.size() >= 0);
        // We add a new format in the list once format changes, so the formatIndex
        // should indicate to the last(latest) format.
        GeckoHlsSample sample = GeckoHlsSample.create(buffer,
                                                      bufferInfo,
                                                      cryptoInfo,
                                                      mFormats.size() - 1);

        mDemuxedInputSamples.offer(sample);

        if (DEBUG) {
            Log.d(LOGTAG, "Demuxed sample PTS : " +
                          sample.info.presentationTimeUs + ", duration :" +
                          sample.duration + ", formatIndex(" +
                          sample.formatIndex + "), queue size : " +
                          mDemuxedInputSamples.size());
        }
    }

    @Override
    protected boolean clearInputSamplesQueue() {
        if (DEBUG) { Log.d(LOGTAG, "clearInputSamplesQueue"); }
        mDemuxedInputSamples.clear();
        return true;
    }

    @Override
    protected void notifyPlayerInputFormatChanged(Format newFormat) {
        mPlayerEventDispatcher.onAudioInputFormatChanged(newFormat);
    }
}
