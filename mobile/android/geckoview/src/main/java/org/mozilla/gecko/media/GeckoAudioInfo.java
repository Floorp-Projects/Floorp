/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import java.nio.ByteBuffer;
import org.mozilla.gecko.annotation.WrapForJNI;

//A subset of the class AudioInfo in dom/media/MediaInfo.h
@WrapForJNI
public final class GeckoAudioInfo {
    final public byte[] codecSpecificData;
    final public int rate;
    final public int channels;
    final public int bitDepth;
    final public int profile;
    final public long duration;
    final public String mimeType;
    public GeckoAudioInfo(int rate, int channels, int bitDepth, int profile,
                          long duration, String mimeType, byte[] codecSpecificData) {
        this.rate = rate;
        this.channels = channels;
        this.bitDepth = bitDepth;
        this.profile = profile;
        this.duration = duration;
        this.mimeType = mimeType;
        this.codecSpecificData = codecSpecificData;
    }
}
