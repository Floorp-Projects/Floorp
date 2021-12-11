/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.WrapForJNI;

// A subset of the class VideoInfo in dom/media/MediaInfo.h
@WrapForJNI
public final class GeckoVideoInfo {
  public final byte[] codecSpecificData;
  public final byte[] extraData;
  public final int displayWidth;
  public final int displayHeight;
  public final int pictureWidth;
  public final int pictureHeight;
  public final int rotation;
  public final int stereoMode;
  public final long duration;
  public final String mimeType;

  public GeckoVideoInfo(
      final int displayWidth,
      final int displayHeight,
      final int pictureWidth,
      final int pictureHeight,
      final int rotation,
      final int stereoMode,
      final long duration,
      final String mimeType,
      final byte[] extraData,
      final byte[] codecSpecificData) {
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
