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

import java.util.Arrays;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

public final class HardwareCodecCapabilityUtils {
    private static final String LOGTAG = "HardwareCodecCapability";

    // List of supported HW VP8 encoders.
    private static final String[] supportedVp8HwEncCodecPrefixes = {
        "OMX.qcom.", "OMX.Intel."
    };
    // List of supported HW VP8 decoders.
    private static final String[] supportedVp8HwDecCodecPrefixes = {
        "OMX.qcom.", "OMX.Nvidia.", "OMX.Exynos.", "OMX.Intel."
    };
    private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
    // List of supported HW VP9 codecs.
    private static final String[] supportedVp9HwCodecPrefixes = {
        "OMX.qcom.", "OMX.Exynos."
    };
    private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
    // List of supported HW H.264 codecs.
    private static final String[] supportedH264HwCodecPrefixes = {
        "OMX.qcom.", "OMX.Intel.", "OMX.Exynos.", "OMX.Nvidia", "OMX.SEC.",
        "OMX.IMG.", "OMX.k3.", "OMX.hisi.", "OMX.TI.", "OMX.MTK."
    };
    private static final String H264_MIME_TYPE = "video/avc";
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
    private static final String[] adaptivePlaybackBlacklist = {
        "GT-I9300",         // S3 (I9300 / I9300I)
        "SCH-I535",         // S3
        "SGH-T999",         // S3 (T-Mobile)
        "SAMSUNG-SGH-T999", // S3 (T-Mobile)
        "SGH-M919",         // S4
        "GT-I9505",         // S4
        "GT-I9515",         // S4
        "SCH-R970",         // S4
        "SGH-I337",         // S4
        "SPH-L720",         // S4 (Sprint)
        "SAMSUNG-SGH-I337", // S4
        "GT-I9195",         // S4 Mini
        "300E5EV/300E4EV/270E5EV/270E4EV/2470EV/2470EE",
        "LG-D605"           // LG Optimus L9 II
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

    @WrapForJNI
    public static String[] getDecoderSupportedMimeTypes() {
        final MediaCodecInfo[] codecList;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            codecList = getCodecListWithOldAPI();
        } else {
            final MediaCodecList list = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            codecList = list.getCodecInfos();
        }

        final Set<String> supportedTypes = new HashSet<>();

        for (final MediaCodecInfo codec : codecList) {
            if (codec.isEncoder()) {
                continue;
            }
            supportedTypes.addAll(Arrays.asList(codec.getSupportedTypes()));
        }

        return supportedTypes.toArray(new String[0]);
    }

    @WrapForJNI
    public static boolean checkSupportsAdaptivePlayback(final MediaCodec aCodec,
                                                        final String aMimeType) {
        // isFeatureSupported supported on API level >= 19.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT ||
                isAdaptivePlaybackBlacklisted(aMimeType)) {
            return false;
        }

        try {
            final MediaCodecInfo info = aCodec.getCodecInfo();
            final MediaCodecInfo.CodecCapabilities capabilities = info.getCapabilitiesForType(aMimeType);
            return capabilities != null &&
                    capabilities.isFeatureSupported(
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

    public static boolean getHWCodecCapability(final String aMimeType, final boolean aIsEncoder) {
        if (Build.VERSION.SDK_INT >= 20) {
            for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
                final MediaCodecInfo info = MediaCodecList.getCodecInfoAt(i);
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
                    continue;  // No HW support in this codec; try the next one.
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
                final CodecCapabilities capabilities =
                        info.getCapabilitiesForType(aMimeType);
                for (final int colorFormat : capabilities.colorFormats) {
                    Log.v(LOGTAG, "   Color: 0x" + Integer.toHexString(colorFormat));
                }
                for (final int supportedColorFormat : supportedColorList) {
                    for (final int codecColorFormat : capabilities.colorFormats) {
                        if (codecColorFormat == supportedColorFormat) {
                            // Found supported HW Codec.
                            Log.d(LOGTAG, "Found target" +
                                    (aIsEncoder ? " encoder " : " decoder ") + name +
                                    ". Color: 0x" + Integer.toHexString(codecColorFormat));
                            return true;
                        }
                    }
                }
            }
        }
        // No HW codec.
        return false;
    }

    private static String[] getSupportedHWCodecPrefixes(final String aMimeType, final boolean aIsEncoder) {
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
        return getHWCodecCapability(H264_MIME_TYPE, true) &&
               getHWCodecCapability(H264_MIME_TYPE, false);
    }
}
