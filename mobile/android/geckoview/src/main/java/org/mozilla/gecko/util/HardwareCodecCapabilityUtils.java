/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 *  * This Source Code Form is subject to the terms of the Mozilla Public
 *   * License, v. 2.0. If a copy of the MPL was not distributed with this
 *    * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


package org.mozilla.gecko.util;

import org.mozilla.gecko.annotation.WrapForJNI;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import android.util.Log;

public final class HardwareCodecCapabilityUtils {
  private static final String LOGTAG = "GeckoHardwareCodecCapabilityUtils";

  // List of supported HW VP8 encoders.
  private static final String[] supportedVp8HwEncCodecPrefixes =
  {"OMX.qcom.", "OMX.Intel." };
  // List of supported HW VP8 decoders.
  private static final String[] supportedVp8HwDecCodecPrefixes =
  {"OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "OMX.Intel." };
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final int[] supportedColorList = {
    CodecCapabilities.COLOR_FormatYUV420Planar,
    CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
    CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m
  };

  @WrapForJNI
  public static boolean findDecoderCodecInfoForMimeType(String aMimeType) {
    int numCodecs = 0;
    try {
      numCodecs = MediaCodecList.getCodecCount();
    } catch (final RuntimeException e) {
      Log.e(LOGTAG, "Failed to retrieve media codec count", e);
      return false;
    }

    for (int i = 0; i < numCodecs; ++i) {
      MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      if (info.isEncoder()) {
        continue;
      }
      for (String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(aMimeType)) {
          return true;
        }
      }
    }
    return false;
  }

  @WrapForJNI
  public static boolean checkSupportsAdaptivePlayback(MediaCodec aCodec, String aMimeType) {
      // isFeatureSupported supported on API level >= 19.
      if (!(Build.VERSION.SDK_INT >= 19)) {
          return false;
      }

      try {
          MediaCodecInfo info = aCodec.getCodecInfo();
          MediaCodecInfo.CodecCapabilities capabilities = info.getCapabilitiesForType(aMimeType);
          return capabilities != null &&
                 capabilities.isFeatureSupported(
                     MediaCodecInfo.CodecCapabilities.FEATURE_AdaptivePlayback);
      } catch (IllegalArgumentException e) {
            Log.e(LOGTAG, "Retrieve codec information failed", e);
      }
      return false;
  }

  public static boolean getHWEncoderCapability() {
    if (Build.VERSION.SDK_INT >= 20) {
      for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
        MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
        if (!info.isEncoder()) {
          continue;
        }
        String name = null;
        for (String mimeType : info.getSupportedTypes()) {
          if (mimeType.equals(VP8_MIME_TYPE)) {
            name = info.getName();
            break;
          }
        }
        if (name == null) {
          continue;  // No HW support in this codec; try the next one.
        }
        Log.e(LOGTAG, "Found candidate encoder " + name);

        // Check if this is supported encoder.
        boolean supportedCodec = false;
        for (String codecPrefix : supportedVp8HwEncCodecPrefixes) {
          if (name.startsWith(codecPrefix)) {
            supportedCodec = true;
            break;
          }
        }
        if (!supportedCodec) {
          continue;
        }

        // Check if codec supports either yuv420 or nv12.
        CodecCapabilities capabilities =
          info.getCapabilitiesForType(VP8_MIME_TYPE);
        for (int colorFormat : capabilities.colorFormats) {
          Log.v(LOGTAG, "   Color: 0x" + Integer.toHexString(colorFormat));
        }
        for (int supportedColorFormat : supportedColorList) {
          for (int codecColorFormat : capabilities.colorFormats) {
            if (codecColorFormat == supportedColorFormat) {
              // Found supported HW Encoder.
              Log.e(LOGTAG, "Found target encoder " + name +
                  ". Color: 0x" + Integer.toHexString(codecColorFormat));
              return true;
            }
          }
        }
      }
    }
    // No HW encoder.
    return false;
  }

  public static boolean getHWDecoderCapability() {
    return getHWDecoderCapability(VP8_MIME_TYPE);
  }

  @WrapForJNI
  public static boolean HasHWVP9() {
    return getHWDecoderCapability(VP9_MIME_TYPE);
  }

  public static boolean getHWDecoderCapability(String aMimeType) {
    if (Build.VERSION.SDK_INT >= 20) {
      for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
        MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
        if (info.isEncoder()) {
          continue;
        }
        String name = null;
        for (String mimeType : info.getSupportedTypes()) {
          if (mimeType.equals(aMimeType)) {
            name = info.getName();
            break;
          }
        }
        if (name == null) {
          continue;  // No HW support in this codec; try the next one.
        }
        Log.e(LOGTAG, "Found candidate decoder " + name);

        // Check if this is supported decoder.
        boolean supportedCodec = false;
        for (String codecPrefix : supportedVp8HwDecCodecPrefixes) {
          if (name.startsWith(codecPrefix)) {
            supportedCodec = true;
            break;
          }
        }
        if (!supportedCodec) {
          continue;
        }

        // Check if codec supports either yuv420 or nv12.
        CodecCapabilities capabilities =
          info.getCapabilitiesForType(aMimeType);
        for (int colorFormat : capabilities.colorFormats) {
          Log.v(LOGTAG, "   Color: 0x" + Integer.toHexString(colorFormat));
        }
        for (int supportedColorFormat : supportedColorList) {
          for (int codecColorFormat : capabilities.colorFormats) {
            if (codecColorFormat == supportedColorFormat) {
              // Found supported HW decoder.
              Log.e(LOGTAG, "Found target decoder " + name +
                  ". Color: 0x" + Integer.toHexString(codecColorFormat));
              return true;
            }
          }
        }
      }
    }
    return false;  // No HW decoder.
  }
}
