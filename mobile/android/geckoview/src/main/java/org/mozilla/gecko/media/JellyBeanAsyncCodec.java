/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;
import org.mozilla.gecko.util.HardwareCodecCapabilityUtils;

// Implement async API using MediaCodec sync mode (API v16).
// This class uses internal worker thread/handler (mBufferPoller) to poll
// input and output buffer and notifies the client through callbacks.
final class JellyBeanAsyncCodec implements AsyncCodec {
  private static final String LOGTAG = "GeckoAsyncCodecAPIv16";
  private static final boolean DEBUG = false;

  private static final int ERROR_CODEC = -10000;

  private abstract class CancelableHandler extends Handler {
    private static final int MSG_CANCELLATION = 0x434E434C; // 'CNCL'

    protected CancelableHandler(final Looper looper) {
      super(looper);
    }

    protected void cancel() {
      removeCallbacksAndMessages(null);
      sendEmptyMessage(MSG_CANCELLATION);
      // Wait until handleMessageLocked() is done.
      synchronized (this) {
      }
    }

    protected boolean isCanceled() {
      return hasMessages(MSG_CANCELLATION);
    }

    // Subclass should implement this and return true if it handles msg.
    // Warning: Never, ever call super.handleMessage() in this method!
    protected abstract boolean handleMessageLocked(Message msg);

    public final void handleMessage(final Message msg) {
      // Block cancel() during handleMessageLocked().
      synchronized (this) {
        if (isCanceled() || handleMessageLocked(msg)) {
          return;
        }
      }

      switch (msg.what) {
        case MSG_CANCELLATION:
          // Just a marker. Nothing to do here.
          if (DEBUG) {
            Log.d(
                LOGTAG,
                "handler " + this + " done cancellation, codec=" + JellyBeanAsyncCodec.this);
          }
          break;
        default:
          super.handleMessage(msg);
          break;
      }
    }
  }

  // A handler to invoke AsyncCodec.Callbacks methods.
  private final class CallbackSender extends CancelableHandler {
    private static final int MSG_INPUT_BUFFER_AVAILABLE = 1;
    private static final int MSG_OUTPUT_BUFFER_AVAILABLE = 2;
    private static final int MSG_OUTPUT_FORMAT_CHANGE = 3;
    private static final int MSG_ERROR = 4;
    private Callbacks mCallbacks;

    private CallbackSender(final Looper looper, final Callbacks callbacks) {
      super(looper);
      mCallbacks = callbacks;
    }

    public void notifyInputBuffer(final int index) {
      if (isCanceled()) {
        return;
      }

      final Message msg = obtainMessage(MSG_INPUT_BUFFER_AVAILABLE);
      msg.arg1 = index;
      processMessage(msg);
    }

    private void processMessage(final Message msg) {
      if (Looper.myLooper() == getLooper()) {
        handleMessage(msg);
      } else {
        sendMessage(msg);
      }
    }

    public void notifyOutputBuffer(final int index, final MediaCodec.BufferInfo info) {
      if (isCanceled()) {
        return;
      }

      final Message msg = obtainMessage(MSG_OUTPUT_BUFFER_AVAILABLE, info);
      msg.arg1 = index;
      processMessage(msg);
    }

    public void notifyOutputFormat(final MediaFormat format) {
      if (isCanceled()) {
        return;
      }
      processMessage(obtainMessage(MSG_OUTPUT_FORMAT_CHANGE, format));
    }

    public void notifyError(final int result) {
      Log.e(LOGTAG, "codec error:" + result);
      processMessage(obtainMessage(MSG_ERROR, result, 0));
    }

    protected boolean handleMessageLocked(final Message msg) {
      switch (msg.what) {
        case MSG_INPUT_BUFFER_AVAILABLE: // arg1: buffer index.
          mCallbacks.onInputBufferAvailable(JellyBeanAsyncCodec.this, msg.arg1);
          break;
        case MSG_OUTPUT_BUFFER_AVAILABLE: // arg1: buffer index, obj: info.
          mCallbacks.onOutputBufferAvailable(
              JellyBeanAsyncCodec.this, msg.arg1, (MediaCodec.BufferInfo) msg.obj);
          break;
        case MSG_OUTPUT_FORMAT_CHANGE: // obj: output format.
          mCallbacks.onOutputFormatChanged(JellyBeanAsyncCodec.this, (MediaFormat) msg.obj);
          break;
        case MSG_ERROR: // arg1: error code.
          mCallbacks.onError(JellyBeanAsyncCodec.this, msg.arg1);
          break;
        default:
          return false;
      }

      return true;
    }
  }

  // Handler to poll input and output buffers using dequeue(Input|Output)Buffer(),
  // with 10ms time-out. Once triggered and successfully gets a buffer, it
  // will schedule next polling until EOS or failure. To prevent it from
  // automatically polling more buffer, use cancel() it inherits from
  // CancelableHandler.
  private final class BufferPoller extends CancelableHandler {
    private static final int MSG_POLL_INPUT_BUFFERS = 1;
    private static final int MSG_POLL_OUTPUT_BUFFERS = 2;

    private static final long DEQUEUE_TIMEOUT_US = 10000;

    public BufferPoller(final Looper looper) {
      super(looper);
    }

    private void schedulePollingIfNotCanceled(final int what) {
      if (isCanceled()) {
        return;
      }

      schedulePolling(what);
    }

    private void schedulePolling(final int what) {
      if (needsBuffer(what)) {
        sendEmptyMessage(what);
      }
    }

    private boolean needsBuffer(final int what) {
      if (mOutputEnded && (what == MSG_POLL_OUTPUT_BUFFERS)) {
        return false;
      }

      return !(mInputEnded && (what == MSG_POLL_INPUT_BUFFERS));
    }

    protected boolean handleMessageLocked(final Message msg) {
      try {
        switch (msg.what) {
          case MSG_POLL_INPUT_BUFFERS:
            pollInputBuffer();
            break;
          case MSG_POLL_OUTPUT_BUFFERS:
            pollOutputBuffer();
            break;
          default:
            return false;
        }
      } catch (final IllegalStateException e) {
        e.printStackTrace();
        mCallbackSender.notifyError(ERROR_CODEC);
      }

      return true;
    }

    private void pollInputBuffer() {
      final int result = mCodec.dequeueInputBuffer(DEQUEUE_TIMEOUT_US);
      if (result >= 0) {
        mCallbackSender.notifyInputBuffer(result);
      } else if (result == MediaCodec.INFO_TRY_AGAIN_LATER) {
        mBufferPoller.schedulePollingIfNotCanceled(BufferPoller.MSG_POLL_INPUT_BUFFERS);
      } else {
        mCallbackSender.notifyError(result);
      }
    }

    private void pollOutputBuffer() {
      boolean dequeueMoreBuffer = true;
      final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      final int result = mCodec.dequeueOutputBuffer(info, DEQUEUE_TIMEOUT_US);
      if (result >= 0) {
        if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
          mOutputEnded = true;
        }
        mCallbackSender.notifyOutputBuffer(result, info);
      } else if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
        mOutputBuffers = mCodec.getOutputBuffers();
      } else if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        mOutputBuffers = mCodec.getOutputBuffers();
        mCallbackSender.notifyOutputFormat(mCodec.getOutputFormat());
      } else if (result == MediaCodec.INFO_TRY_AGAIN_LATER) {
        // When input ended, keep polling remaining output buffer until EOS.
        dequeueMoreBuffer = mInputEnded;
      } else {
        mCallbackSender.notifyError(result);
        dequeueMoreBuffer = false;
      }

      if (dequeueMoreBuffer) {
        schedulePollingIfNotCanceled(MSG_POLL_OUTPUT_BUFFERS);
      }
    }
  }

  private MediaCodec mCodec;
  private ByteBuffer[] mInputBuffers;
  private ByteBuffer[] mOutputBuffers;
  private AsyncCodec.Callbacks mCallbacks;
  private CallbackSender mCallbackSender;

  private BufferPoller mBufferPoller;
  private volatile boolean mInputEnded;
  private volatile boolean mOutputEnded;

  // Must be called on a thread with looper.
  /* package */ JellyBeanAsyncCodec(final String name) throws IOException {
    mCodec = MediaCodec.createByCodecName(name);
    initBufferPoller(name + " buffer poller");
  }

  private void initBufferPoller(final String name) {
    if (mBufferPoller != null) {
      Log.e(LOGTAG, "poller already initialized");
      return;
    }
    final HandlerThread thread = new HandlerThread(name);
    thread.start();
    mBufferPoller = new BufferPoller(thread.getLooper());
    if (DEBUG) {
      Log.d(LOGTAG, "start poller for codec:" + this + ", thread=" + thread.getThreadId());
    }
  }

  @Override
  public void setCallbacks(final AsyncCodec.Callbacks callbacks, final Handler handler) {
    if (callbacks == null) {
      return;
    }

    Looper looper = (handler == null) ? null : handler.getLooper();
    if (looper == null) {
      // Use this thread if no handler supplied.
      looper = Looper.myLooper();
    }
    if (looper == null) {
      // This thread has no looper. Use poller thread.
      looper = mBufferPoller.getLooper();
    }
    mCallbackSender = new CallbackSender(looper, callbacks);
    if (DEBUG) {
      Log.d(LOGTAG, "setCallbacks(): sender=" + mCallbackSender);
    }
  }

  @Override
  public void configure(
      final MediaFormat format, final Surface surface, final MediaCrypto crypto, final int flags) {
    assertCallbacks();

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

  private void assertCallbacks() {
    if (mCallbackSender == null) {
      throw new IllegalStateException(LOGTAG + ": callback must be supplied with setCallbacks().");
    }
  }

  @Override
  public void start() {
    assertCallbacks();

    mCodec.start();
    mInputEnded = false;
    mOutputEnded = false;
    mInputBuffers = mCodec.getInputBuffers();
    resumeReceivingInputs();
    mOutputBuffers = mCodec.getOutputBuffers();
  }

  @Override
  public void resumeReceivingInputs() {
    for (int i = 0; i < mInputBuffers.length; i++) {
      mBufferPoller.schedulePolling(BufferPoller.MSG_POLL_INPUT_BUFFERS);
    }
  }

  @Override
  public final void setBitrate(final int bps) {
    final Bundle params = new Bundle();
    params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, bps);
    mCodec.setParameters(params);
  }

  @Override
  public final void queueInputBuffer(
      final int index,
      final int offset,
      final int size,
      final long presentationTimeUs,
      final int flags) {
    assertCallbacks();

    mInputEnded = (flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;

    if (((flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0)) {
      final Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
      mCodec.setParameters(params);
    }

    try {
      mCodec.queueInputBuffer(index, offset, size, presentationTimeUs, flags);
    } catch (final IllegalStateException e) {
      e.printStackTrace();
      mCallbackSender.notifyError(ERROR_CODEC);
      return;
    }

    mBufferPoller.schedulePolling(BufferPoller.MSG_POLL_OUTPUT_BUFFERS);
    mBufferPoller.schedulePolling(BufferPoller.MSG_POLL_INPUT_BUFFERS);
  }

  @Override
  public final void queueSecureInputBuffer(
      final int index,
      final int offset,
      final MediaCodec.CryptoInfo cryptoInfo,
      final long presentationTimeUs,
      final int flags) {
    assertCallbacks();

    mInputEnded = (flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;

    try {
      mCodec.queueSecureInputBuffer(index, offset, cryptoInfo, presentationTimeUs, flags);
    } catch (final IllegalStateException e) {
      e.printStackTrace();
      mCallbackSender.notifyError(ERROR_CODEC);
      return;
    }

    mBufferPoller.schedulePolling(BufferPoller.MSG_POLL_INPUT_BUFFERS);
    mBufferPoller.schedulePolling(BufferPoller.MSG_POLL_OUTPUT_BUFFERS);
  }

  @Override
  public final void releaseOutputBuffer(final int index, final boolean render) {
    assertCallbacks();

    mCodec.releaseOutputBuffer(index, render);
  }

  @Override
  public final ByteBuffer getInputBuffer(final int index) {
    assertCallbacks();

    return mInputBuffers[index];
  }

  @Override
  public final ByteBuffer getOutputBuffer(final int index) {
    assertCallbacks();

    return mOutputBuffers[index];
  }

  @Override
  public MediaFormat getInputFormat() {
    return null;
  }

  @Override
  public void flush() {
    assertCallbacks();

    mInputEnded = false;
    mOutputEnded = false;
    cancelPendingTasks();
    mCodec.flush();
  }

  private void cancelPendingTasks() {
    mBufferPoller.cancel();
    mCallbackSender.cancel();
  }

  @Override
  public void stop() {
    assertCallbacks();

    cancelPendingTasks();
    mCodec.stop();
  }

  @Override
  public void release() {
    assertCallbacks();

    cancelPendingTasks();
    mCallbackSender = null;
    mCodec.release();
    stopBufferPoller();
  }

  private void stopBufferPoller() {
    if (mBufferPoller == null) {
      Log.e(LOGTAG, "no initialized poller.");
      return;
    }

    mBufferPoller.getLooper().quit();
    mBufferPoller = null;

    if (DEBUG) {
      Log.d(LOGTAG, "stop poller " + this);
    }
  }
}
