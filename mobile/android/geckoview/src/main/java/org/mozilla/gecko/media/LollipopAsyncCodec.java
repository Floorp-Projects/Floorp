/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.Surface;
import androidx.annotation.NonNull;
import java.io.IOException;
import java.nio.ByteBuffer;
import org.mozilla.gecko.util.HardwareCodecCapabilityUtils;

/* package */ final class LollipopAsyncCodec implements AsyncCodec {
  private final MediaCodec mCodec;

  private class CodecCallback extends MediaCodec.Callback {
    private final Forwarder mForwarder;

    private class Forwarder extends Handler {
      private static final int MSG_INPUT_BUFFER_AVAILABLE = 1;
      private static final int MSG_OUTPUT_BUFFER_AVAILABLE = 2;
      private static final int MSG_OUTPUT_FORMAT_CHANGE = 3;
      private static final int MSG_ERROR = 4;

      private final Callbacks mTarget;

      private Forwarder(final Looper looper, final Callbacks target) {
        super(looper);
        mTarget = target;
      }

      @Override
      public void handleMessage(final Message msg) {
        switch (msg.what) {
          case MSG_INPUT_BUFFER_AVAILABLE:
            mTarget.onInputBufferAvailable(LollipopAsyncCodec.this, msg.arg1); // index
            break;
          case MSG_OUTPUT_BUFFER_AVAILABLE:
            mTarget.onOutputBufferAvailable(
                LollipopAsyncCodec.this,
                msg.arg1, // index
                (MediaCodec.BufferInfo) msg.obj); //  buffer info
            break;
          case MSG_OUTPUT_FORMAT_CHANGE:
            mTarget.onOutputFormatChanged(
                LollipopAsyncCodec.this, (MediaFormat) msg.obj); // output format
            break;
          case MSG_ERROR:
            mTarget.onError(LollipopAsyncCodec.this, msg.arg1); // error code
            break;
          default:
            super.handleMessage(msg);
        }
      }

      private void onInput(final int index) {
        notify(obtainMessage(MSG_INPUT_BUFFER_AVAILABLE, index, 0));
      }

      private void notify(final Message msg) {
        if (Looper.myLooper() == getLooper()) {
          handleMessage(msg);
        } else {
          sendMessage(msg);
        }
      }

      private void onOutput(final int index, final MediaCodec.BufferInfo info) {
        final Message msg = obtainMessage(MSG_OUTPUT_BUFFER_AVAILABLE, index, 0, info);
        notify(msg);
      }

      private void onOutputFormatChanged(final MediaFormat format) {
        notify(obtainMessage(MSG_OUTPUT_FORMAT_CHANGE, format));
      }

      private void onError(final MediaCodec.CodecException e) {
        e.printStackTrace();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
          notify(obtainMessage(MSG_ERROR, e.getErrorCode()));
        } else {
          notify(obtainMessage(MSG_ERROR, e.getLocalizedMessage()));
        }
      }
    }

    private CodecCallback(final Callbacks callbacks, final Handler handler) {
      Looper looper = (handler == null) ? null : handler.getLooper();
      if (looper == null) {
        // Use this thread if no handler supplied.
        looper = Looper.myLooper();
      }
      if (looper == null) {
        // This thread has no looper. Use main thread.
        looper = Looper.getMainLooper();
      }

      mForwarder = new Forwarder(looper, callbacks);
    }

    @Override
    public void onInputBufferAvailable(@NonNull final MediaCodec codec, final int index) {
      mForwarder.onInput(index);
    }

    @Override
    public void onOutputBufferAvailable(
        @NonNull final MediaCodec codec,
        final int index,
        @NonNull final MediaCodec.BufferInfo info) {
      mForwarder.onOutput(index, info);
    }

    @Override
    public void onOutputFormatChanged(
        @NonNull final MediaCodec codec, @NonNull final MediaFormat format) {
      mForwarder.onOutputFormatChanged(format);
    }

    @Override
    public void onError(
        @NonNull final MediaCodec codec, @NonNull final MediaCodec.CodecException e) {
      mForwarder.onError(e);
    }
  }

  /* package */ LollipopAsyncCodec(final String name) throws IOException {
    mCodec = MediaCodec.createByCodecName(name);
  }

  @Override
  public void setCallbacks(final Callbacks callbacks, final Handler handler) {
    if (callbacks == null) {
      return;
    }

    mCodec.setCallback(new CodecCallback(callbacks, handler));
  }

  @Override
  public void configure(
      final MediaFormat format, final Surface surface, final MediaCrypto crypto, final int flags) {
    mCodec.configure(format, surface, crypto, flags);
  }

  @Override
  public boolean isAdaptivePlaybackSupported(final String mimeType) {
    return HardwareCodecCapabilityUtils.checkSupportsAdaptivePlayback(mCodec, mimeType);
  }

  @Override
  public boolean isTunneledPlaybackSupported(final String mimeType) {
    try {
      return mCodec
          .getCodecInfo()
          .getCapabilitiesForType(mimeType)
          .isFeatureSupported(CodecCapabilities.FEATURE_TunneledPlayback);
    } catch (final Exception e) {
      return false;
    }
  }

  @Override
  public void start() {
    mCodec.start();
  }

  @Override
  public void stop() {
    mCodec.stop();
  }

  @Override
  public void flush() {
    mCodec.flush();
  }

  @Override
  public void resumeReceivingInputs() {
    mCodec.start();
  }

  @Override
  public void setBitrate(final int bps) {
    final Bundle params = new Bundle();
    params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, bps);
    mCodec.setParameters(params);
  }

  @Override
  public void release() {
    mCodec.release();
  }

  @Override
  public ByteBuffer getInputBuffer(final int index) {
    return mCodec.getInputBuffer(index);
  }

  @Override
  public ByteBuffer getOutputBuffer(final int index) {
    return mCodec.getOutputBuffer(index);
  }

  @Override
  public MediaFormat getInputFormat() {
    return mCodec.getInputFormat();
  }

  @Override
  public void queueInputBuffer(
      final int index,
      final int offset,
      final int size,
      final long presentationTimeUs,
      final int flags) {
    if ((flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
      final Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
      mCodec.setParameters(params);
    }
    mCodec.queueInputBuffer(index, offset, size, presentationTimeUs, flags);
  }

  @Override
  public void queueSecureInputBuffer(
      final int index,
      final int offset,
      final MediaCodec.CryptoInfo info,
      final long presentationTimeUs,
      final int flags) {
    mCodec.queueSecureInputBuffer(index, offset, info, presentationTimeUs, flags);
  }

  @Override
  public void releaseOutputBuffer(final int index, final boolean render) {
    mCodec.releaseOutputBuffer(index, render);
  }
}
