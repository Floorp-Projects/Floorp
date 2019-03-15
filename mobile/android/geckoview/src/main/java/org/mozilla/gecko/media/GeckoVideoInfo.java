/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import java.nio.ByteBuffer;
import org.mozilla.gecko.annotation.WrapForJNI;

//A subset of the class VideoInfo in dom/media/MediaInfo.h
@WrapForJNI
public final class GeckoVideoInfo {
    final public byte[] codecSpecificData;
    final public byte[] extraData;
    final public int displayWidth;
    final public int displayHeight;
    final public int pictureWidth;
    final public int pictureHeight;
    final public int rotation;
    final public int stereoMode;
    final public long duration;
    final public String mimeType;
    public GeckoVideoInfo(int displayWidth, int displayHeight,
                          int pictureWidth, int pictureHeight,
                          int rotation, int stereoMode, long duration, String mimeType,
                          byte[] extraData, byte[] codecSpecificData) {
        this.displayWidth = displayWidth;
        this.displayHeight = displayHeight;
        this.pictureWidth = pictureWidth;
        this.pictureHeight = pictureHeight;
        this.rotation = rotation;
        this.stereoMode = stereoMode;
        this.duration = duration;
        this.mimeType = mimeType;
        this.extraData = extraData;
        this.codecSpecificData = codecSpecificData;
    }
}
