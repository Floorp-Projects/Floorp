/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.media;

import org.mozilla.gecko.annotation.WrapForJNI;

// A subset of the class AudioInfo in dom/media/MediaInfo.h
@WrapForJNI
public final class GeckoAudioInfo {
  public final byte[] codecSpecificData;
  public final int rate;
  public final int channels;
  public final int bitDepth;
  public final int profile;
  public final long duration;
  public final String mimeType;

  public GeckoAudioInfo(
      final int rate,
      final int channels,
      final int bitDepth,
      final int profile,
      final long duration,
      final String mimeType,
      final byte[] codecSpecificData) {
    this.rate = rate;
    this.channels = channels;
    this.bitDepth = bitDepth;
    this.profile = profile;
    this.duration = duration;
    this.mimeType = mimeType;
    this.codecSpecificData = codecSpecificData;
  }
}
