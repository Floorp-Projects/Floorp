/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;
import java.io.IOException;
import java.nio.ByteBuffer;
import org.mozilla.gecko.annotation.WrapForJNI;

public final class GeckoHLSSample {
  public static final GeckoHLSSample EOS;

  static {
    final BufferInfo eosInfo = new BufferInfo();
    eosInfo.set(0, 0, Long.MIN_VALUE, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
    EOS = new GeckoHLSSample(null, eosInfo, null, 0);
  }

  // Indicate the index of format which is used by this sample.
  @WrapForJNI public final int formatIndex;

  @WrapForJNI public long duration;

  @WrapForJNI public final BufferInfo info;

  @WrapForJNI public final CryptoInfo cryptoInfo;

  private ByteBuffer mBuffer = null;

  @WrapForJNI
  public void writeToByteBuffer(final ByteBuffer dest) throws IOException {
    if (mBuffer != null && dest != null && info.size > 0) {
      dest.put(mBuffer);
    }
  }

  @WrapForJNI
  public boolean isEOS() {
    return (info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0;
  }

  @WrapForJNI
  public boolean isKeyFrame() {
    return (info.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0;
  }

  public static GeckoHLSSample create(
      final ByteBuffer src,
      final BufferInfo info,
      final CryptoInfo cryptoInfo,
      final int formatIndex) {
    return new GeckoHLSSample(src, info, cryptoInfo, formatIndex);
  }

  private GeckoHLSSample(
      final ByteBuffer buffer,
      final BufferInfo info,
      final CryptoInfo cryptoInfo,
      final int formatIndex) {
    this.formatIndex = formatIndex;
    duration = Long.MAX_VALUE;
    this.mBuffer = buffer;
    this.info = info;
    this.cryptoInfo = cryptoInfo;
  }

  @Override
  public String toString() {
    if (isEOS()) {
      return "EOS GeckoHLSSample";
    }

    final StringBuilder str = new StringBuilder();
    str.append("{ info=")
        .append("{ offset=")
        .append(info.offset)
        .append(", size=")
        .append(info.size)
        .append(", pts=")
        .append(info.presentationTimeUs)
        .append(", duration=")
        .append(duration)
        .append(", flags=")
        .append(Integer.toHexString(info.flags))
        .append(" }")
        .append(" }");
    return str.toString();
  }
}
