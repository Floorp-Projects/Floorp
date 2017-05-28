/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodec.CryptoInfo;

import org.mozilla.gecko.annotation.WrapForJNI;

import java.io.IOException;
import java.nio.ByteBuffer;

public final class GeckoHlsSample {
    public static final GeckoHlsSample EOS;
    static {
        BufferInfo eosInfo = new BufferInfo();
        eosInfo.set(0, 0, Long.MIN_VALUE, MediaCodec.BUFFER_FLAG_END_OF_STREAM);
        EOS = new GeckoHlsSample(null, eosInfo, null, 0);
    }

    // Indicate the index of format which is used by this sample.
    @WrapForJNI
    final public int formatIndex;

    @WrapForJNI
    public long duration;

    @WrapForJNI
    final public BufferInfo info;

    @WrapForJNI
    final public CryptoInfo cryptoInfo;

    private ByteBuffer buffer = null;

    @WrapForJNI
    public void writeToByteBuffer(ByteBuffer dest) throws IOException {
        if (buffer != null && dest != null && info.size > 0) {
            dest.put(buffer);
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

    public static GeckoHlsSample create(ByteBuffer src, BufferInfo info, CryptoInfo cryptoInfo,
                                        int formatIndex) {
        return new GeckoHlsSample(src, info, cryptoInfo, formatIndex);
    }

    private GeckoHlsSample(ByteBuffer buffer, BufferInfo info, CryptoInfo cryptoInfo,
                           int formatIndex) {
        this.formatIndex = formatIndex;
        duration = Long.MAX_VALUE;
        this.buffer = buffer;
        this.info = info;
        this.cryptoInfo = cryptoInfo;
    }

    @Override
    public String toString() {
        if (isEOS()) {
            return "EOS GeckoHlsSample";
        }

        StringBuilder str = new StringBuilder();
        str.append("{ info=").
                append("{ offset=").append(info.offset).
                append(", size=").append(info.size).
                append(", pts=").append(info.presentationTimeUs).
                append(", duration=").append(duration).
                append(", flags=").append(Integer.toHexString(info.flags)).append(" }").
                append(" }");
            return str.toString();
    }
}
