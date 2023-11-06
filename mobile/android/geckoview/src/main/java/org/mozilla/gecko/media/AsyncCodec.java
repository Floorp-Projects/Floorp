/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCrypto;
import android.media.MediaFormat;
import android.os.Handler;
import android.view.Surface;
import java.nio.ByteBuffer;

// A wrapper interface that mimics the new {@link android.media.MediaCodec}
// asynchronous mode API in Lollipop.
public interface AsyncCodec {
  interface Callbacks {
    void onInputBufferAvailable(AsyncCodec codec, int index);

    void onOutputBufferAvailable(AsyncCodec codec, int index, BufferInfo info);

    void onError(AsyncCodec codec, int error);

    void onOutputFormatChanged(AsyncCodec codec, MediaFormat format);
  }

  void setCallbacks(Callbacks callbacks, Handler handler);

  void configure(MediaFormat format, Surface surface, MediaCrypto crypto, int flags);

  boolean isAdaptivePlaybackSupported(String mimeType);

  boolean isTunneledPlaybackSupported(final String mimeType);

  void start();

  void stop();

  void flush();

  // Must be called after flush().
  void resumeReceivingInputs();

  void release();

  ByteBuffer getInputBuffer(int index);

  MediaFormat getInputFormat();

  ByteBuffer getOutputBuffer(int index);

  void queueInputBuffer(int index, int offset, int size, long presentationTimeUs, int flags);

  void setBitrate(int bps);

  void queueSecureInputBuffer(
      int index, int offset, CryptoInfo info, long presentationTimeUs, int flags);

  void releaseOutputBuffer(int index, boolean render);
}
