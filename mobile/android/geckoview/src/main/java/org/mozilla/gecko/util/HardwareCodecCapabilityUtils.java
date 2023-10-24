/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.annotation.SuppressLint;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import android.util.Log;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import org.mozilla.gecko.annotation.WrapForJNI;

public final class HardwareCodecCapabilityUtils {
  private static final String LOGTAG = "HardwareCodecCapability";

  // List of supported HW VP8 encoders.
  private static final String[] supportedVp8HwEncCodecPrefixes = {"OMX.qcom.", "OMX.Intel."};
  // List of supported HW VP8 decoders.
  private static final String[] supportedVp8HwDecCodecPrefixes = {
    "OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "c2.exynos", "OMX.Intel."
  };
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  // List of supported HW VP9 codecs.
  private static final String[] supportedVp9HwCodecPrefixes = {
    "OMX.qcom.", "OMX.Exynos.", "c2.exynos"
  };
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  // List of supported HW H.264 codecs.
  private static final String[] supportedH264HwCodecPrefixes = {
    "OMX.qcom.",
    "OMX.Intel.",
    "OMX.Exynos.",
    "c2.exynos",
    "OMX.Nvidia",
    "OMX.SEC.",
    "OMX.IMG.",
    "OMX.k3.",
    "OMX.hisi.",
    "OMX.TI.",
    "OMX.MTK."
  };
  private static final String H264_MIME_TYPE = "video/avc";
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final int[] supportedColorList = {
    CodecCapabilities.COLOR_FormatYUV420Planar,
    CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
    CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
    COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m
  };
  private static final int COLOR_FORMAT_NOT_SUPPORTED = -1;
  private static final String[] adaptivePlaybackBlacklist = {
    "GT-I9300", // S3 (I9300 / I9300I)
    "SCH-I535", // S3
    "SGH-T999", // S3 (T-Mobile)
    "SAMSUNG-SGH-T999", // S3 (T-Mobile)
    "SGH-M919", // S4
    "GT-I9505", // S4
    "GT-I9515", // S4
    "SCH-R970", // S4
    "SGH-I337", // S4
    "SPH-L720", // S4 (Sprint)
    "SAMSUNG-SGH-I337", // S4
    "GT-I9195", // S4 Mini
    "300E5EV/300E4EV/270E5EV/270E4EV/2470EV/2470EE",
    "LG-D605" // LG Optimus L9 II
  };

  private static MediaCodecInfo[] getCodecListWithOldAPI() {
    int numCodecs = 0;
    try {
      numCodecs = MediaCodecList.getCodecCount();
    } catch (final RuntimeException e) {
      Log.e(LOGTAG, "Failed to retrieve media codec count", e);
      return new MediaCodecInfo[numCodecs];
    }

    final MediaCodecInfo[] codecList = new MediaCodecInfo[numCodecs];

    for (int i = 0; i < numCodecs; ++i) {
      final MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
      codecList[i] = info;
    }

    return codecList;
  }

  // Return list of all codecs (decode + encode).
  private static MediaCodecInfo[] getCodecList() {
    final MediaCodecInfo[] codecList;
    try {
      final MediaCodecList list = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
      codecList = list.getCodecInfos();
    } catch (final RuntimeException e) {
      Log.e(LOGTAG, "Failed to retrieve media codec support list", e);
      return new MediaCodecInfo[0];
    }
    return codecList;
  }

  // Return list of all decoders.
  private static MediaCodecInfo[] getDecoderInfos() {
    final ArrayList<MediaCodecInfo> decoderList = new ArrayList<MediaCodecInfo>();
    for (final MediaCodecInfo info : getCodecList()) {
      if (!info.isEncoder()) {
        decoderList.add(info);
      }
    }
    return decoderList.toArray(new MediaCodecInfo[0]);
  }

  // Return list of all encoders.
  private static MediaCodecInfo[] getEncoderInfos() {
    final ArrayList<MediaCodecInfo> encoderList = new ArrayList<MediaCodecInfo>();
    for (final MediaCodecInfo info : getCodecList()) {
      if (info.isEncoder()) {
        encoderList.add(info);
      }
    }
    return encoderList.toArray(new MediaCodecInfo[0]);
  }

  // Return list of all decoder-supported MIME types without distinguishing
  // between SW/HW support.
  @WrapForJNI
  public static String[] getDecoderSupportedMimeTypes() {
    final Set<String> mimeTypes = new HashSet<>();
    for (final MediaCodecInfo info : getDecoderInfos()) {
      mimeTypes.addAll(Arrays.asList(info.getSupportedTypes()));
    }
    return mimeTypes.toArray(new String[0]);
  }

  // Return list of all decoder-supported MIME types, each prefixed with
  // either SW or HW indicating software or hardware support.
  @WrapForJNI
  public static String[] getDecoderSupportedMimeTypesWithAccelInfo() {
    final Set<String> mimeTypes = new HashSet<>();
    final String[] hwPrefixes = getAllSupportedHWCodecPrefixes(false);

    for (final MediaCodecInfo info : getDecoderInfos()) {
      final String[] supportedTypes = info.getSupportedTypes();
      for (final String mimeType : info.getSupportedTypes()) {
        boolean isHwPrefix = false;
        for (final String prefix : hwPrefixes) {
          if (info.getName().startsWith(prefix)) {
            isHwPrefix = true;
            break;
          }
        }
        if (!isHwPrefix) {
          mimeTypes.add("SW " + mimeType);
          continue;
        }
        final CodecCapabilities caps = info.getCapabilitiesForType(mimeType);
        if (getSupportsYUV420orNV12(caps) != COLOR_FORMAT_NOT_SUPPORTED) {
          mimeTypes.add("HW " + mimeType);
        }
      }
    }
    for (final String typeit : mimeTypes) {
      Log.d(LOGTAG, "MIME support: " + typeit);
    }
    return mimeTypes.toArray(new String[0]);
  }

  public static boolean checkSupportsAdaptivePlayback(
      final MediaCodec aCodec, final String aMimeType) {
    if (isAdaptivePlaybackBlacklisted(aMimeType)) {
      return false;
    }

    try {
      final MediaCodecInfo info = aCodec.getCodecInfo();
      final MediaCodecInfo.CodecCapabilities capabilities = info.getCapabilitiesForType(aMimeType);
      return capabilities != null
          && capabilities.isFeatureSupported(
              MediaCodecInfo.CodecCapabilities.FEATURE_AdaptivePlayback);
    } catch (final IllegalArgumentException e) {
      Log.e(LOGTAG, "Retrieve codec information failed", e);
    }
    return false;
  }

  // See Bug1360626 and
  // https://codereview.chromium.org/1869103002 for details.
  private static boolean isAdaptivePlaybackBlacklisted(final String aMimeType) {
    Log.d(LOGTAG, "The device ModelID is " + Build.MODEL);
    if (!aMimeType.equals("video/avc") && !aMimeType.equals("video/avc1")) {
      return false;
    }

    if (!Build.MANUFACTURER.toLowerCase(Locale.ROOT).equals("samsung")) {
      return false;
    }

    for (final String model : adaptivePlaybackBlacklist) {
      if (Build.MODEL.startsWith(model)) {
        return true;
      }
    }
    return false;
  }

  // Check if a given MIME Type has HW decode or encode support.
  public static boolean getHWCodecCapability(final String aMimeType, final boolean aIsEncoder) {
    for (final MediaCodecInfo info : getCodecList()) {
      if (info.isEncoder() != aIsEncoder) {
        continue;
      }
      String name = null;
      for (final String mimeType : info.getSupportedTypes()) {
        if (mimeType.equals(aMimeType)) {
          name = info.getName();
          break;
        }
      }
      if (name == null) {
        continue; // No HW support in this codec; try the next one.
      }
      Log.d(LOGTAG, "Found candidate" + (aIsEncoder ? " encoder " : " decoder ") + name);

      // Check if this is supported codec.
      final String[] hwList = getSupportedHWCodecPrefixes(aMimeType, aIsEncoder);
      if (hwList == null) {
        continue;
      }
      boolean supportedCodec = false;
      for (final String codecPrefix : hwList) {
        if (name.startsWith(codecPrefix)) {
          supportedCodec = true;
          break;
        }
      }
      if (!supportedCodec) {
        continue;
      }

      // Check if codec supports either yuv420 or nv12.
      final CodecCapabilities capabilities = info.getCapabilitiesForType(aMimeType);
      for (final int colorFormat : capabilities.colorFormats) {
        Log.v(LOGTAG, "   Color: 0x" + Integer.toHexString(colorFormat));
      }
      if (Build.VERSION.SDK_INT >= 24) {
        for (final MediaCodecInfo.CodecProfileLevel pl : capabilities.profileLevels) {
          Log.v(
              LOGTAG,
              "   Profile: 0x"
                  + Integer.toHexString(pl.profile)
                  + "/Level=0x"
                  + Integer.toHexString(pl.level));
        }
      }
      final int codecColorFormat = getSupportsYUV420orNV12(capabilities);
      if (codecColorFormat != COLOR_FORMAT_NOT_SUPPORTED) {
        Log.d(
            LOGTAG,
            "Found target"
                + (aIsEncoder ? " encoder " : " decoder ")
                + name
                + ". Color: 0x"
                + Integer.toHexString(codecColorFormat));
        return true;
      }
    }
    // No HW codec.
    return false;
  }

  // Check if codec supports YUV420 or NV12
  private static int getSupportsYUV420orNV12(final CodecCapabilities aCodecCaps) {
    for (final int supportedColorFormat : supportedColorList) {
      for (final int codecColorFormat : aCodecCaps.colorFormats) {
        if (codecColorFormat == supportedColorFormat) {
          return codecColorFormat;
        }
      }
    }
    return COLOR_FORMAT_NOT_SUPPORTED;
  }

  // Check if MIME type string has HW prefix (encode or decode, VP8, VP9, and H264)
  private static String[] getSupportedHWCodecPrefixes(
      final String aMimeType, final boolean aIsEncoder) {
    if (aMimeType.equals(H264_MIME_TYPE)) {
      return supportedH264HwCodecPrefixes;
    }
    if (aMimeType.equals(VP9_MIME_TYPE)) {
      return supportedVp9HwCodecPrefixes;
    }
    if (aMimeType.equals(VP8_MIME_TYPE)) {
      return aIsEncoder ? supportedVp8HwEncCodecPrefixes : supportedVp8HwDecCodecPrefixes;
    }
    return null;
  }

  // Return list of HW codec prefixes (encode or decode, VP8, VP9, and H264)
  private static String[] getAllSupportedHWCodecPrefixes(final boolean aIsEncoder) {
    final Set<String> prefixes = new HashSet<>();
    final String[] mimeTypes = {H264_MIME_TYPE, VP8_MIME_TYPE, VP9_MIME_TYPE};
    for (final String mt : mimeTypes) {
      prefixes.addAll(Arrays.asList(getSupportedHWCodecPrefixes(mt, aIsEncoder)));
    }
    return prefixes.toArray(new String[0]);
  }

  @WrapForJNI
  public static boolean hasHWVP8(final boolean aIsEncoder) {
    return getHWCodecCapability(VP8_MIME_TYPE, aIsEncoder);
  }

  @WrapForJNI
  public static boolean hasHWVP9(final boolean aIsEncoder) {
    return getHWCodecCapability(VP9_MIME_TYPE, aIsEncoder);
  }

  @WrapForJNI
  public static boolean hasHWH264(final boolean aIsEncoder) {
    return getHWCodecCapability(H264_MIME_TYPE, aIsEncoder);
  }

  @WrapForJNI(calledFrom = "gecko")
  public static boolean hasHWH264() {
    return getHWCodecCapability(H264_MIME_TYPE, true)
        && getHWCodecCapability(H264_MIME_TYPE, false);
  }

  @WrapForJNI
  @SuppressLint("NewApi")
  public static boolean decodes10Bit(final String aMimeType) {
    if (Build.VERSION.SDK_INT < 24) {
      // Be conservative when we cannot get supported profile.
      return false;
    }

    final MediaCodecList codecs = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
    for (final MediaCodecInfo info : codecs.getCodecInfos()) {
      if (info.isEncoder()) {
        continue;
      }
      try {
        for (final MediaCodecInfo.CodecProfileLevel pl :
            info.getCapabilitiesForType(aMimeType).profileLevels) {
          if ((aMimeType.equals(H264_MIME_TYPE)
                  && pl.profile == MediaCodecInfo.CodecProfileLevel.AVCProfileHigh10)
              || (aMimeType.equals(VP9_MIME_TYPE) && is10BitVP9Profile(pl.profile))) {
            return true;
          }
        }
      } catch (final IllegalArgumentException e) {
        // Type not supported.
        continue;
      }
    }

    return false;
  }

  @SuppressLint("NewApi")
  private static boolean is10BitVP9Profile(final int profile) {
    if (Build.VERSION.SDK_INT < 24) {
      // Be conservative when we cannot get supported profile.
      return false;
    }

    if ((profile == MediaCodecInfo.CodecProfileLevel.VP9Profile2)
        || (profile == MediaCodecInfo.CodecProfileLevel.VP9Profile3)
        || (profile == MediaCodecInfo.CodecProfileLevel.VP9Profile2HDR)
        || (profile == MediaCodecInfo.CodecProfileLevel.VP9Profile3HDR)) {
      return true;
    }

    return Build.VERSION.SDK_INT >= 29
        && ((profile == MediaCodecInfo.CodecProfileLevel.VP9Profile2HDR10Plus)
            || (profile == MediaCodecInfo.CodecProfileLevel.VP9Profile3HDR10Plus));
  }
}
