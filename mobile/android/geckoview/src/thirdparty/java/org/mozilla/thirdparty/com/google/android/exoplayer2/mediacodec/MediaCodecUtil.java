/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.mozilla.thirdparty.com.google.android.exoplayer2.mediacodec;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.media.MediaCodecList;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseIntArray;
import androidx.annotation.CheckResult;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import org.mozilla.thirdparty.com.google.android.exoplayer2.C;
import org.mozilla.thirdparty.com.google.android.exoplayer2.Format;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Log;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.MimeTypes;
import org.mozilla.thirdparty.com.google.android.exoplayer2.util.Util;
import org.mozilla.thirdparty.com.google.android.exoplayer2.video.ColorInfo;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.checkerframework.checker.nullness.qual.EnsuresNonNull;

/**
 * A utility class for querying the available codecs.
 */
@SuppressLint("InlinedApi")
public final class MediaCodecUtil {

  /**
   * Thrown when an error occurs querying the device for its underlying media capabilities.
   * <p>
   * Such failures are not expected in normal operation and are normally temporary (e.g. if the
   * mediaserver process has crashed and is yet to restart).
   */
  public static class DecoderQueryException extends Exception {

    private DecoderQueryException(Throwable cause) {
      super("Failed to query underlying media codecs", cause);
    }

  }

  private static final String TAG = "MediaCodecUtil";
  private static final Pattern PROFILE_PATTERN = Pattern.compile("^\\D?(\\d+)$");

  private static final HashMap<CodecKey, List<MediaCodecInfo>> decoderInfosCache = new HashMap<>();

  // Codecs to constant mappings.
  // AVC.
  private static final SparseIntArray AVC_PROFILE_NUMBER_TO_CONST;
  private static final SparseIntArray AVC_LEVEL_NUMBER_TO_CONST;
  private static final String CODEC_ID_AVC1 = "avc1";
  private static final String CODEC_ID_AVC2 = "avc2";
  // VP9
  private static final SparseIntArray VP9_PROFILE_NUMBER_TO_CONST;
  private static final SparseIntArray VP9_LEVEL_NUMBER_TO_CONST;
  private static final String CODEC_ID_VP09 = "vp09";
  // HEVC.
  private static final Map<String, Integer> HEVC_CODEC_STRING_TO_PROFILE_LEVEL;
  private static final String CODEC_ID_HEV1 = "hev1";
  private static final String CODEC_ID_HVC1 = "hvc1";
  // Dolby Vision.
  private static final Map<String, Integer> DOLBY_VISION_STRING_TO_PROFILE;
  private static final Map<String, Integer> DOLBY_VISION_STRING_TO_LEVEL;
  // AV1.
  private static final SparseIntArray AV1_LEVEL_NUMBER_TO_CONST;
  private static final String CODEC_ID_AV01 = "av01";
  // MP4A AAC.
  private static final SparseIntArray MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE;
  private static final String CODEC_ID_MP4A = "mp4a";

  // Lazily initialized.
  private static int maxH264DecodableFrameSize = -1;

  private MediaCodecUtil() {}

  /**
   * Optional call to warm the codec cache for a given mime type.
   *
   * <p>Calling this method may speed up subsequent calls to {@link #getDecoderInfo(String, boolean,
   * boolean)} and {@link #getDecoderInfos(String, boolean, boolean)}.
   *
   * @param mimeType The mime type.
   * @param secure Whether the decoder is required to support secure decryption. Always pass false
   *     unless secure decryption really is required.
   * @param tunneling Whether the decoder is required to support tunneling. Always pass false unless
   *     tunneling really is required.
   */
  public static void warmDecoderInfoCache(String mimeType, boolean secure, boolean tunneling) {
    try {
      getDecoderInfos(mimeType, secure, tunneling);
    } catch (DecoderQueryException e) {
      // Codec warming is best effort, so we can swallow the exception.
      Log.e(TAG, "Codec warming failed", e);
    }
  }

  /**
   * Returns information about a decoder suitable for audio passthrough.
   *
   * @return A {@link MediaCodecInfo} describing the decoder, or null if no suitable decoder exists.
   * @throws DecoderQueryException If there was an error querying the available decoders.
   */
  @Nullable
  public static MediaCodecInfo getPassthroughDecoderInfo() throws DecoderQueryException {
    @Nullable
    MediaCodecInfo decoderInfo =
        getDecoderInfo(MimeTypes.AUDIO_RAW, /* secure= */ false, /* tunneling= */ false);
    return decoderInfo == null ? null : MediaCodecInfo.newPassthroughInstance(decoderInfo.name);
  }

  /**
   * Returns information about the preferred decoder for a given mime type.
   *
   * @param mimeType The MIME type.
   * @param secure Whether the decoder is required to support secure decryption. Always pass false
   *     unless secure decryption really is required.
   * @param tunneling Whether the decoder is required to support tunneling. Always pass false unless
   *     tunneling really is required.
   * @return A {@link MediaCodecInfo} describing the decoder, or null if no suitable decoder exists.
   * @throws DecoderQueryException If there was an error querying the available decoders.
   */
  @Nullable
  public static MediaCodecInfo getDecoderInfo(String mimeType, boolean secure, boolean tunneling)
      throws DecoderQueryException {
    List<MediaCodecInfo> decoderInfos = getDecoderInfos(mimeType, secure, tunneling);
    return decoderInfos.isEmpty() ? null : decoderInfos.get(0);
  }

  /**
   * Returns all {@link MediaCodecInfo}s for the given mime type, in the order given by {@link
   * MediaCodecList}.
   *
   * @param mimeType The MIME type.
   * @param secure Whether the decoder is required to support secure decryption. Always pass false
   *     unless secure decryption really is required.
   * @param tunneling Whether the decoder is required to support tunneling. Always pass false unless
   *     tunneling really is required.
   * @return An unmodifiable list of all {@link MediaCodecInfo}s for the given mime type, in the
   *     order given by {@link MediaCodecList}.
   * @throws DecoderQueryException If there was an error querying the available decoders.
   */
  public static synchronized List<MediaCodecInfo> getDecoderInfos(
      String mimeType, boolean secure, boolean tunneling) throws DecoderQueryException {
    CodecKey key = new CodecKey(mimeType, secure, tunneling);
    @Nullable List<MediaCodecInfo> cachedDecoderInfos = decoderInfosCache.get(key);
    if (cachedDecoderInfos != null) {
      return cachedDecoderInfos;
    }
    MediaCodecListCompat mediaCodecList =
        Util.SDK_INT >= 21
            ? new MediaCodecListCompatV21(secure, tunneling)
            : new MediaCodecListCompatV16();
    ArrayList<MediaCodecInfo> decoderInfos = getDecoderInfosInternal(key, mediaCodecList);
    if (secure && decoderInfos.isEmpty() && 21 <= Util.SDK_INT && Util.SDK_INT <= 23) {
      // Some devices don't list secure decoders on API level 21 [Internal: b/18678462]. Try the
      // legacy path. We also try this path on API levels 22 and 23 as a defensive measure.
      mediaCodecList = new MediaCodecListCompatV16();
      decoderInfos = getDecoderInfosInternal(key, mediaCodecList);
      if (!decoderInfos.isEmpty()) {
        Log.w(TAG, "MediaCodecList API didn't list secure decoder for: " + mimeType
            + ". Assuming: " + decoderInfos.get(0).name);
      }
    }
    applyWorkarounds(mimeType, decoderInfos);
    List<MediaCodecInfo> unmodifiableDecoderInfos = Collections.unmodifiableList(decoderInfos);
    decoderInfosCache.put(key, unmodifiableDecoderInfos);
    return unmodifiableDecoderInfos;
  }

  /**
   * Returns a copy of the provided decoder list sorted such that decoders with format support are
   * listed first. The returned list is modifiable for convenience.
   */
  @CheckResult
  public static List<MediaCodecInfo> getDecoderInfosSortedByFormatSupport(
      List<MediaCodecInfo> decoderInfos, Format format) {
    decoderInfos = new ArrayList<>(decoderInfos);
    sortByScore(
        decoderInfos,
        decoderInfo -> {
          try {
            return decoderInfo.isFormatSupported(format) ? 1 : 0;
          } catch (DecoderQueryException e) {
            return -1;
          }
        });
    return decoderInfos;
  }

  /**
   * Returns the maximum frame size supported by the default H264 decoder.
   *
   * @return The maximum frame size for an H264 stream that can be decoded on the device.
   */
  public static int maxH264DecodableFrameSize() throws DecoderQueryException {
    if (maxH264DecodableFrameSize == -1) {
      int result = 0;
      @Nullable
      MediaCodecInfo decoderInfo =
          getDecoderInfo(MimeTypes.VIDEO_H264, /* secure= */ false, /* tunneling= */ false);
      if (decoderInfo != null) {
        for (CodecProfileLevel profileLevel : decoderInfo.getProfileLevels()) {
          result = Math.max(avcLevelToMaxFrameSize(profileLevel.level), result);
        }
        // We assume support for at least 480p (SDK_INT >= 21) or 360p (SDK_INT < 21), which are
        // the levels mandated by the Android CDD.
        result = Math.max(result, Util.SDK_INT >= 21 ? (720 * 480) : (480 * 360));
      }
      maxH264DecodableFrameSize = result;
    }
    return maxH264DecodableFrameSize;
  }

  /**
   * Returns profile and level (as defined by {@link CodecProfileLevel}) corresponding to the codec
   * description string (as defined by RFC 6381) of the given format.
   *
   * @param format Media format with a codec description string, as defined by RFC 6381.
   * @return A pair (profile constant, level constant) if the codec of the {@code format} is
   *     well-formed and recognized, or null otherwise.
   */
  @Nullable
  public static Pair<Integer, Integer> getCodecProfileAndLevel(Format format) {
    if (format.codecs == null) {
      return null;
    }
    String[] parts = format.codecs.split("\\.");
    // Dolby Vision can use DV, AVC or HEVC codec IDs, so check the MIME type first.
    if (MimeTypes.VIDEO_DOLBY_VISION.equals(format.sampleMimeType)) {
      return getDolbyVisionProfileAndLevel(format.codecs, parts);
    }
    switch (parts[0]) {
      case CODEC_ID_AVC1:
      case CODEC_ID_AVC2:
        return getAvcProfileAndLevel(format.codecs, parts);
      case CODEC_ID_VP09:
        return getVp9ProfileAndLevel(format.codecs, parts);
      case CODEC_ID_HEV1:
      case CODEC_ID_HVC1:
        return getHevcProfileAndLevel(format.codecs, parts);
      case CODEC_ID_AV01:
        return getAv1ProfileAndLevel(format.codecs, parts, format.colorInfo);
      case CODEC_ID_MP4A:
        return getAacCodecProfileAndLevel(format.codecs, parts);
      default:
        return null;
    }
  }

  // Internal methods.

  /**
   * Returns {@link MediaCodecInfo}s for the given codec {@link CodecKey} in the order given by
   * {@code mediaCodecList}.
   *
   * @param key The codec key.
   * @param mediaCodecList The codec list.
   * @return The codec information for usable codecs matching the specified key.
   * @throws DecoderQueryException If there was an error querying the available decoders.
   */
  private static ArrayList<MediaCodecInfo> getDecoderInfosInternal(
      CodecKey key, MediaCodecListCompat mediaCodecList) throws DecoderQueryException {
    try {
      ArrayList<MediaCodecInfo> decoderInfos = new ArrayList<>();
      String mimeType = key.mimeType;
      int numberOfCodecs = mediaCodecList.getCodecCount();
      boolean secureDecodersExplicit = mediaCodecList.secureDecodersExplicit();
      // Note: MediaCodecList is sorted by the framework such that the best decoders come first.
      for (int i = 0; i < numberOfCodecs; i++) {
        android.media.MediaCodecInfo codecInfo = mediaCodecList.getCodecInfoAt(i);
        if (isAlias(codecInfo)) {
          // Skip aliases of other codecs, since they will also be listed under their canonical
          // names.
          continue;
        }
        String name = codecInfo.getName();
        if (!isCodecUsableDecoder(codecInfo, name, secureDecodersExplicit, mimeType)) {
          continue;
        }
        @Nullable String codecMimeType = getCodecMimeType(codecInfo, name, mimeType);
        if (codecMimeType == null) {
          continue;
        }
        try {
          CodecCapabilities capabilities = codecInfo.getCapabilitiesForType(codecMimeType);
          boolean tunnelingSupported =
              mediaCodecList.isFeatureSupported(
                  CodecCapabilities.FEATURE_TunneledPlayback, codecMimeType, capabilities);
          boolean tunnelingRequired =
              mediaCodecList.isFeatureRequired(
                  CodecCapabilities.FEATURE_TunneledPlayback, codecMimeType, capabilities);
          if ((!key.tunneling && tunnelingRequired) || (key.tunneling && !tunnelingSupported)) {
            continue;
          }
          boolean secureSupported =
              mediaCodecList.isFeatureSupported(
                  CodecCapabilities.FEATURE_SecurePlayback, codecMimeType, capabilities);
          boolean secureRequired =
              mediaCodecList.isFeatureRequired(
                  CodecCapabilities.FEATURE_SecurePlayback, codecMimeType, capabilities);
          if ((!key.secure && secureRequired) || (key.secure && !secureSupported)) {
            continue;
          }
          boolean hardwareAccelerated = isHardwareAccelerated(codecInfo);
          boolean softwareOnly = isSoftwareOnly(codecInfo);
          boolean vendor = isVendor(codecInfo);
          boolean forceDisableAdaptive = codecNeedsDisableAdaptationWorkaround(name);
          if ((secureDecodersExplicit && key.secure == secureSupported)
              || (!secureDecodersExplicit && !key.secure)) {
            decoderInfos.add(
                MediaCodecInfo.newInstance(
                    name,
                    mimeType,
                    codecMimeType,
                    capabilities,
                    hardwareAccelerated,
                    softwareOnly,
                    vendor,
                    forceDisableAdaptive,
                    /* forceSecure= */ false));
          } else if (!secureDecodersExplicit && secureSupported) {
            decoderInfos.add(
                MediaCodecInfo.newInstance(
                    name + ".secure",
                    mimeType,
                    codecMimeType,
                    capabilities,
                    hardwareAccelerated,
                    softwareOnly,
                    vendor,
                    forceDisableAdaptive,
                    /* forceSecure= */ true));
            // It only makes sense to have one synthesized secure decoder, return immediately.
            return decoderInfos;
          }
        } catch (Exception e) {
          if (Util.SDK_INT <= 23 && !decoderInfos.isEmpty()) {
            // Suppress error querying secondary codec capabilities up to API level 23.
            Log.e(TAG, "Skipping codec " + name + " (failed to query capabilities)");
          } else {
            // Rethrow error querying primary codec capabilities, or secondary codec
            // capabilities if API level is greater than 23.
            Log.e(TAG, "Failed to query codec " + name + " (" + codecMimeType + ")");
            throw e;
          }
        }
      }
      return decoderInfos;
    } catch (Exception e) {
      // If the underlying mediaserver is in a bad state, we may catch an IllegalStateException
      // or an IllegalArgumentException here.
      throw new DecoderQueryException(e);
    }
  }

  /**
   * Returns the codec's supported MIME type for media of type {@code mimeType}, or {@code null} if
   * the codec can't be used.
   *
   * @param info The codec information.
   * @param name The name of the codec
   * @param mimeType The MIME type.
   * @return The codec's supported MIME type for media of type {@code mimeType}, or {@code null} if
   *     the codec can't be used. If non-null, the returned type will be equal to {@code mimeType}
   *     except in cases where the codec is known to use a non-standard MIME type alias.
   */
  @Nullable
  private static String getCodecMimeType(
      android.media.MediaCodecInfo info,
      String name,
      String mimeType) {
    String[] supportedTypes = info.getSupportedTypes();
    for (String supportedType : supportedTypes) {
      if (supportedType.equalsIgnoreCase(mimeType)) {
        return supportedType;
      }
    }

    if (mimeType.equals(MimeTypes.VIDEO_DOLBY_VISION)) {
      // Handle decoders that declare support for DV via MIME types that aren't
      // video/dolby-vision.
      if ("OMX.MS.HEVCDV.Decoder".equals(name)) {
        return "video/hevcdv";
      } else if ("OMX.RTK.video.decoder".equals(name)
          || "OMX.realtek.video.decoder.tunneled".equals(name)) {
        return "video/dv_hevc";
      }
    } else if (mimeType.equals(MimeTypes.AUDIO_ALAC) && "OMX.lge.alac.decoder".equals(name)) {
      return "audio/x-lg-alac";
    } else if (mimeType.equals(MimeTypes.AUDIO_FLAC) && "OMX.lge.flac.decoder".equals(name)) {
      return "audio/x-lg-flac";
    }

    return null;
  }

  /**
   * Returns whether the specified codec is usable for decoding on the current device.
   *
   * @param info The codec information.
   * @param name The name of the codec
   * @param secureDecodersExplicit Whether secure decoders were explicitly listed, if present.
   * @param mimeType The MIME type.
   * @return Whether the specified codec is usable for decoding on the current device.
   */
  private static boolean isCodecUsableDecoder(
      android.media.MediaCodecInfo info,
      String name,
      boolean secureDecodersExplicit,
      String mimeType) {
    if (info.isEncoder() || (!secureDecodersExplicit && name.endsWith(".secure"))) {
      return false;
    }

    // Work around broken audio decoders.
    if (Util.SDK_INT < 21
        && ("CIPAACDecoder".equals(name)
            || "CIPMP3Decoder".equals(name)
            || "CIPVorbisDecoder".equals(name)
            || "CIPAMRNBDecoder".equals(name)
            || "AACDecoder".equals(name)
            || "MP3Decoder".equals(name))) {
      return false;
    }

    // Work around https://github.com/google/ExoPlayer/issues/1528 and
    // https://github.com/google/ExoPlayer/issues/3171.
    if (Util.SDK_INT < 18
        && "OMX.MTK.AUDIO.DECODER.AAC".equals(name)
        && ("a70".equals(Util.DEVICE)
            || ("Xiaomi".equals(Util.MANUFACTURER) && Util.DEVICE.startsWith("HM")))) {
      return false;
    }

    // Work around an issue where querying/creating a particular MP3 decoder on some devices on
    // platform API version 16 fails.
    if (Util.SDK_INT == 16
        && "OMX.qcom.audio.decoder.mp3".equals(name)
        && ("dlxu".equals(Util.DEVICE) // HTC Butterfly
            || "protou".equals(Util.DEVICE) // HTC Desire X
            || "ville".equals(Util.DEVICE) // HTC One S
            || "villeplus".equals(Util.DEVICE)
            || "villec2".equals(Util.DEVICE)
            || Util.DEVICE.startsWith("gee") // LGE Optimus G
            || "C6602".equals(Util.DEVICE) // Sony Xperia Z
            || "C6603".equals(Util.DEVICE)
            || "C6606".equals(Util.DEVICE)
            || "C6616".equals(Util.DEVICE)
            || "L36h".equals(Util.DEVICE)
            || "SO-02E".equals(Util.DEVICE))) {
      return false;
    }

    // Work around an issue where large timestamps are not propagated correctly.
    if (Util.SDK_INT == 16
        && "OMX.qcom.audio.decoder.aac".equals(name)
        && ("C1504".equals(Util.DEVICE) // Sony Xperia E
            || "C1505".equals(Util.DEVICE)
            || "C1604".equals(Util.DEVICE) // Sony Xperia E dual
            || "C1605".equals(Util.DEVICE))) {
      return false;
    }

    // Work around https://github.com/google/ExoPlayer/issues/3249.
    if (Util.SDK_INT < 24
        && ("OMX.SEC.aac.dec".equals(name) || "OMX.Exynos.AAC.Decoder".equals(name))
        && "samsung".equals(Util.MANUFACTURER)
        && (Util.DEVICE.startsWith("zeroflte") // Galaxy S6
            || Util.DEVICE.startsWith("zerolte") // Galaxy S6 Edge
            || Util.DEVICE.startsWith("zenlte") // Galaxy S6 Edge+
            || "SC-05G".equals(Util.DEVICE) // Galaxy S6
            || "marinelteatt".equals(Util.DEVICE) // Galaxy S6 Active
            || "404SC".equals(Util.DEVICE) // Galaxy S6 Edge
            || "SC-04G".equals(Util.DEVICE)
            || "SCV31".equals(Util.DEVICE))) {
      return false;
    }

    // Work around https://github.com/google/ExoPlayer/issues/548.
    // VP8 decoder on Samsung Galaxy S3/S4/S4 Mini/Tab 3/Note 2 does not render video.
    if (Util.SDK_INT <= 19
        && "OMX.SEC.vp8.dec".equals(name)
        && "samsung".equals(Util.MANUFACTURER)
        && (Util.DEVICE.startsWith("d2")
            || Util.DEVICE.startsWith("serrano")
            || Util.DEVICE.startsWith("jflte")
            || Util.DEVICE.startsWith("santos")
            || Util.DEVICE.startsWith("t0"))) {
      return false;
    }

    // VP8 decoder on Samsung Galaxy S4 cannot be queried.
    if (Util.SDK_INT <= 19 && Util.DEVICE.startsWith("jflte")
        && "OMX.qcom.video.decoder.vp8".equals(name)) {
      return false;
    }

    // MTK E-AC3 decoder doesn't support decoding JOC streams in 2-D. See [Internal: b/69400041].
    if (MimeTypes.AUDIO_E_AC3_JOC.equals(mimeType) && "OMX.MTK.AUDIO.DECODER.DSPAC3".equals(name)) {
      return false;
    }

    return true;
  }

  /**
   * Modifies a list of {@link MediaCodecInfo}s to apply workarounds where we know better than the
   * platform.
   *
   * @param mimeType The MIME type of input media.
   * @param decoderInfos The list to modify.
   */
  private static void applyWorkarounds(String mimeType, List<MediaCodecInfo> decoderInfos) {
    if (MimeTypes.AUDIO_RAW.equals(mimeType)) {
      if (Util.SDK_INT < 26
          && Util.DEVICE.equals("R9")
          && decoderInfos.size() == 1
          && decoderInfos.get(0).name.equals("OMX.MTK.AUDIO.DECODER.RAW")) {
        // This device does not list a generic raw audio decoder, yet it can be instantiated by
        // name. See <a href="https://github.com/google/ExoPlayer/issues/5782">Issue #5782</a>.
        decoderInfos.add(
            MediaCodecInfo.newInstance(
                /* name= */ "OMX.google.raw.decoder",
                /* mimeType= */ MimeTypes.AUDIO_RAW,
                /* codecMimeType= */ MimeTypes.AUDIO_RAW,
                /* capabilities= */ null,
                /* hardwareAccelerated= */ false,
                /* softwareOnly= */ true,
                /* vendor= */ false,
                /* forceDisableAdaptive= */ false,
                /* forceSecure= */ false));
      }
      // Work around inconsistent raw audio decoding behavior across different devices.
      sortByScore(
          decoderInfos,
          decoderInfo -> {
            String name = decoderInfo.name;
            if (name.startsWith("OMX.google") || name.startsWith("c2.android")) {
              // Prefer generic decoders over ones provided by the device.
              return 1;
            }
            if (Util.SDK_INT < 26 && name.equals("OMX.MTK.AUDIO.DECODER.RAW")) {
              // This decoder may modify the audio, so any other compatible decoders take
              // precedence. See [Internal: b/62337687].
              return -1;
            }
            return 0;
          });
    }

    if (Util.SDK_INT < 21 && decoderInfos.size() > 1) {
      String firstCodecName = decoderInfos.get(0).name;
      if ("OMX.SEC.mp3.dec".equals(firstCodecName)
          || "OMX.SEC.MP3.Decoder".equals(firstCodecName)
          || "OMX.brcm.audio.mp3.decoder".equals(firstCodecName)) {
        // Prefer OMX.google codecs over OMX.SEC.mp3.dec, OMX.SEC.MP3.Decoder and
        // OMX.brcm.audio.mp3.decoder on older devices. See:
        // https://github.com/google/ExoPlayer/issues/398 and
        // https://github.com/google/ExoPlayer/issues/4519.
        sortByScore(decoderInfos, decoderInfo -> decoderInfo.name.startsWith("OMX.google") ? 1 : 0);
      }
    }

    if (Util.SDK_INT < 30 && decoderInfos.size() > 1) {
      String firstCodecName = decoderInfos.get(0).name;
      // Prefer anything other than OMX.qti.audio.decoder.flac on older devices. See [Internal
      // ref: b/147278539] and [Internal ref: b/147354613].
      if ("OMX.qti.audio.decoder.flac".equals(firstCodecName)) {
        decoderInfos.add(decoderInfos.remove(0));
      }
    }
  }

  private static boolean isAlias(android.media.MediaCodecInfo info) {
    return Util.SDK_INT >= 29 && isAliasV29(info);
  }

  @RequiresApi(29)
  private static boolean isAliasV29(android.media.MediaCodecInfo info) {
    return info.isAlias();
  }

  /**
   * The result of {@link android.media.MediaCodecInfo#isHardwareAccelerated()} for API levels 29+,
   * or a best-effort approximation for lower levels.
   */
  private static boolean isHardwareAccelerated(android.media.MediaCodecInfo codecInfo) {
    if (Util.SDK_INT >= 29) {
      return isHardwareAcceleratedV29(codecInfo);
    }
    // codecInfo.isHardwareAccelerated() != codecInfo.isSoftwareOnly() is not necessarily true.
    // However, we assume this to be true as an approximation.
    return !isSoftwareOnly(codecInfo);
  }

  @TargetApi(29)
  private static boolean isHardwareAcceleratedV29(android.media.MediaCodecInfo codecInfo) {
    return codecInfo.isHardwareAccelerated();
  }

  /**
   * The result of {@link android.media.MediaCodecInfo#isSoftwareOnly()} for API levels 29+, or a
   * best-effort approximation for lower levels.
   */
  private static boolean isSoftwareOnly(android.media.MediaCodecInfo codecInfo) {
    if (Util.SDK_INT >= 29) {
      return isSoftwareOnlyV29(codecInfo);
    }
    String codecName = Util.toLowerInvariant(codecInfo.getName());
    if (codecName.startsWith("arc.")) { // App Runtime for Chrome (ARC) codecs
      return false;
    }
    return codecName.startsWith("omx.google.")
        || codecName.startsWith("omx.ffmpeg.")
        || (codecName.startsWith("omx.sec.") && codecName.contains(".sw."))
        || codecName.equals("omx.qcom.video.decoder.hevcswvdec")
        || codecName.startsWith("c2.android.")
        || codecName.startsWith("c2.google.")
        || (!codecName.startsWith("omx.") && !codecName.startsWith("c2."));
  }

  @TargetApi(29)
  private static boolean isSoftwareOnlyV29(android.media.MediaCodecInfo codecInfo) {
    return codecInfo.isSoftwareOnly();
  }

  /**
   * The result of {@link android.media.MediaCodecInfo#isVendor()} for API levels 29+, or a
   * best-effort approximation for lower levels.
   */
  private static boolean isVendor(android.media.MediaCodecInfo codecInfo) {
    if (Util.SDK_INT >= 29) {
      return isVendorV29(codecInfo);
    }
    String codecName = Util.toLowerInvariant(codecInfo.getName());
    return !codecName.startsWith("omx.google.")
        && !codecName.startsWith("c2.android.")
        && !codecName.startsWith("c2.google.");
  }

  @TargetApi(29)
  private static boolean isVendorV29(android.media.MediaCodecInfo codecInfo) {
    return codecInfo.isVendor();
  }

  /**
   * Returns whether the decoder is known to fail when adapting, despite advertising itself as an
   * adaptive decoder.
   *
   * @param name The decoder name.
   * @return True if the decoder is known to fail when adapting.
   */
  private static boolean codecNeedsDisableAdaptationWorkaround(String name) {
    return Util.SDK_INT <= 22
        && ("ODROID-XU3".equals(Util.MODEL) || "Nexus 10".equals(Util.MODEL))
        && ("OMX.Exynos.AVC.Decoder".equals(name) || "OMX.Exynos.AVC.Decoder.secure".equals(name));
  }

  @Nullable
  private static Pair<Integer, Integer> getDolbyVisionProfileAndLevel(
      String codec, String[] parts) {
    if (parts.length < 3) {
      // The codec has fewer parts than required by the Dolby Vision codec string format.
      Log.w(TAG, "Ignoring malformed Dolby Vision codec string: " + codec);
      return null;
    }
    // The profile_space gets ignored.
    Matcher matcher = PROFILE_PATTERN.matcher(parts[1]);
    if (!matcher.matches()) {
      Log.w(TAG, "Ignoring malformed Dolby Vision codec string: " + codec);
      return null;
    }
    @Nullable String profileString = matcher.group(1);
    @Nullable Integer profile = DOLBY_VISION_STRING_TO_PROFILE.get(profileString);
    if (profile == null) {
      Log.w(TAG, "Unknown Dolby Vision profile string: " + profileString);
      return null;
    }
    String levelString = parts[2];
    @Nullable Integer level = DOLBY_VISION_STRING_TO_LEVEL.get(levelString);
    if (level == null) {
      Log.w(TAG, "Unknown Dolby Vision level string: " + levelString);
      return null;
    }
    return new Pair<>(profile, level);
  }

  @Nullable
  private static Pair<Integer, Integer> getHevcProfileAndLevel(String codec, String[] parts) {
    if (parts.length < 4) {
      // The codec has fewer parts than required by the HEVC codec string format.
      Log.w(TAG, "Ignoring malformed HEVC codec string: " + codec);
      return null;
    }
    // The profile_space gets ignored.
    Matcher matcher = PROFILE_PATTERN.matcher(parts[1]);
    if (!matcher.matches()) {
      Log.w(TAG, "Ignoring malformed HEVC codec string: " + codec);
      return null;
    }
    @Nullable String profileString = matcher.group(1);
    int profile;
    if ("1".equals(profileString)) {
      profile = CodecProfileLevel.HEVCProfileMain;
    } else if ("2".equals(profileString)) {
      profile = CodecProfileLevel.HEVCProfileMain10;
    } else {
      Log.w(TAG, "Unknown HEVC profile string: " + profileString);
      return null;
    }
    @Nullable String levelString = parts[3];
    @Nullable Integer level = HEVC_CODEC_STRING_TO_PROFILE_LEVEL.get(levelString);
    if (level == null) {
      Log.w(TAG, "Unknown HEVC level string: " + levelString);
      return null;
    }
    return new Pair<>(profile, level);
  }

  @Nullable
  private static Pair<Integer, Integer> getAvcProfileAndLevel(String codec, String[] parts) {
    if (parts.length < 2) {
      // The codec has fewer parts than required by the AVC codec string format.
      Log.w(TAG, "Ignoring malformed AVC codec string: " + codec);
      return null;
    }
    int profileInteger;
    int levelInteger;
    try {
      if (parts[1].length() == 6) {
        // Format: avc1.xxccyy, where xx is profile and yy level, both hexadecimal.
        profileInteger = Integer.parseInt(parts[1].substring(0, 2), 16);
        levelInteger = Integer.parseInt(parts[1].substring(4), 16);
      } else if (parts.length >= 3) {
        // Format: avc1.xx.[y]yy where xx is profile and [y]yy level, both decimal.
        profileInteger = Integer.parseInt(parts[1]);
        levelInteger = Integer.parseInt(parts[2]);
      } else {
        // We don't recognize the format.
        Log.w(TAG, "Ignoring malformed AVC codec string: " + codec);
        return null;
      }
    } catch (NumberFormatException e) {
      Log.w(TAG, "Ignoring malformed AVC codec string: " + codec);
      return null;
    }

    int profile = AVC_PROFILE_NUMBER_TO_CONST.get(profileInteger, -1);
    if (profile == -1) {
      Log.w(TAG, "Unknown AVC profile: " + profileInteger);
      return null;
    }
    int level = AVC_LEVEL_NUMBER_TO_CONST.get(levelInteger, -1);
    if (level == -1) {
      Log.w(TAG, "Unknown AVC level: " + levelInteger);
      return null;
    }
    return new Pair<>(profile, level);
  }

  @Nullable
  private static Pair<Integer, Integer> getVp9ProfileAndLevel(String codec, String[] parts) {
    if (parts.length < 3) {
      Log.w(TAG, "Ignoring malformed VP9 codec string: " + codec);
      return null;
    }
    int profileInteger;
    int levelInteger;
    try {
      profileInteger = Integer.parseInt(parts[1]);
      levelInteger = Integer.parseInt(parts[2]);
    } catch (NumberFormatException e) {
      Log.w(TAG, "Ignoring malformed VP9 codec string: " + codec);
      return null;
    }

    int profile = VP9_PROFILE_NUMBER_TO_CONST.get(profileInteger, -1);
    if (profile == -1) {
      Log.w(TAG, "Unknown VP9 profile: " + profileInteger);
      return null;
    }
    int level = VP9_LEVEL_NUMBER_TO_CONST.get(levelInteger, -1);
    if (level == -1) {
      Log.w(TAG, "Unknown VP9 level: " + levelInteger);
      return null;
    }
    return new Pair<>(profile, level);
  }

  @Nullable
  private static Pair<Integer, Integer> getAv1ProfileAndLevel(
      String codec, String[] parts, @Nullable ColorInfo colorInfo) {
    if (parts.length < 4) {
      Log.w(TAG, "Ignoring malformed AV1 codec string: " + codec);
      return null;
    }
    int profileInteger;
    int levelInteger;
    int bitDepthInteger;
    try {
      profileInteger = Integer.parseInt(parts[1]);
      levelInteger = Integer.parseInt(parts[2].substring(0, 2));
      bitDepthInteger = Integer.parseInt(parts[3]);
    } catch (NumberFormatException e) {
      Log.w(TAG, "Ignoring malformed AV1 codec string: " + codec);
      return null;
    }

    if (profileInteger != 0) {
      Log.w(TAG, "Unknown AV1 profile: " + profileInteger);
      return null;
    }
    if (bitDepthInteger != 8 && bitDepthInteger != 10) {
      Log.w(TAG, "Unknown AV1 bit depth: " + bitDepthInteger);
      return null;
    }
    int profile;
    if (bitDepthInteger == 8) {
      profile = CodecProfileLevel.AV1ProfileMain8;
    } else if (colorInfo != null
        && (colorInfo.hdrStaticInfo != null
            || colorInfo.colorTransfer == C.COLOR_TRANSFER_HLG
            || colorInfo.colorTransfer == C.COLOR_TRANSFER_ST2084)) {
      profile = CodecProfileLevel.AV1ProfileMain10HDR10;
    } else {
      profile = CodecProfileLevel.AV1ProfileMain10;
    }

    int level = AV1_LEVEL_NUMBER_TO_CONST.get(levelInteger, -1);
    if (level == -1) {
      Log.w(TAG, "Unknown AV1 level: " + levelInteger);
      return null;
    }
    return new Pair<>(profile, level);
  }

  /**
   * Conversion values taken from ISO 14496-10 Table A-1.
   *
   * @param avcLevel one of CodecProfileLevel.AVCLevel* constants.
   * @return maximum frame size that can be decoded by a decoder with the specified avc level
   *     (or {@code -1} if the level is not recognized)
   */
  private static int avcLevelToMaxFrameSize(int avcLevel) {
    switch (avcLevel) {
      case CodecProfileLevel.AVCLevel1:
      case CodecProfileLevel.AVCLevel1b:
        return 99 * 16 * 16;
      case CodecProfileLevel.AVCLevel12:
      case CodecProfileLevel.AVCLevel13:
      case CodecProfileLevel.AVCLevel2:
        return 396 * 16 * 16;
      case CodecProfileLevel.AVCLevel21:
        return 792 * 16 * 16;
      case CodecProfileLevel.AVCLevel22:
      case CodecProfileLevel.AVCLevel3:
        return 1620 * 16 * 16;
      case CodecProfileLevel.AVCLevel31:
        return 3600 * 16 * 16;
      case CodecProfileLevel.AVCLevel32:
        return 5120 * 16 * 16;
      case CodecProfileLevel.AVCLevel4:
      case CodecProfileLevel.AVCLevel41:
        return 8192 * 16 * 16;
      case CodecProfileLevel.AVCLevel42:
        return 8704 * 16 * 16;
      case CodecProfileLevel.AVCLevel5:
        return 22080 * 16 * 16;
      case CodecProfileLevel.AVCLevel51:
      case CodecProfileLevel.AVCLevel52:
        return 36864 * 16 * 16;
      default:
        return -1;
    }
  }

  @Nullable
  private static Pair<Integer, Integer> getAacCodecProfileAndLevel(String codec, String[] parts) {
    if (parts.length != 3) {
      Log.w(TAG, "Ignoring malformed MP4A codec string: " + codec);
      return null;
    }
    try {
      // Get the object type indication, which is a hexadecimal value (see RFC 6381/ISO 14496-1).
      int objectTypeIndication = Integer.parseInt(parts[1], 16);
      @Nullable String mimeType = MimeTypes.getMimeTypeFromMp4ObjectType(objectTypeIndication);
      if (MimeTypes.AUDIO_AAC.equals(mimeType)) {
        // For MPEG-4 audio this is followed by an audio object type indication as a decimal number.
        int audioObjectTypeIndication = Integer.parseInt(parts[2]);
        int profile = MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.get(audioObjectTypeIndication, -1);
        if (profile != -1) {
          // Level is set to zero in AAC decoder CodecProfileLevels.
          return new Pair<>(profile, 0);
        }
      }
    } catch (NumberFormatException e) {
      Log.w(TAG, "Ignoring malformed MP4A codec string: " + codec);
    }
    return null;
  }

  /** Stably sorts the provided {@code list} in-place, in order of decreasing score. */
  private static <T> void sortByScore(List<T> list, ScoreProvider<T> scoreProvider) {
    Collections.sort(list, (a, b) -> scoreProvider.getScore(b) - scoreProvider.getScore(a));
  }

  /** Interface for providers of item scores. */
  private interface ScoreProvider<T> {
    /** Returns the score of the provided item. */
    int getScore(T t);
  }

  private interface MediaCodecListCompat {

    /**
     * The number of codecs in the list.
     */
    int getCodecCount();

    /**
     * The info at the specified index in the list.
     *
     * @param index The index.
     */
    android.media.MediaCodecInfo getCodecInfoAt(int index);

    /**
     * Returns whether secure decoders are explicitly listed, if present.
     */
    boolean secureDecodersExplicit();

    /** Whether the specified {@link CodecCapabilities} {@code feature} is supported. */
    boolean isFeatureSupported(String feature, String mimeType, CodecCapabilities capabilities);

    /** Whether the specified {@link CodecCapabilities} {@code feature} is required. */
    boolean isFeatureRequired(String feature, String mimeType, CodecCapabilities capabilities);
  }

  @TargetApi(21)
  private static final class MediaCodecListCompatV21 implements MediaCodecListCompat {

    private final int codecKind;

    @Nullable private android.media.MediaCodecInfo[] mediaCodecInfos;

    // the constructor does not initialize fields: mediaCodecInfos
    @SuppressWarnings("nullness:initialization.fields.uninitialized")
    public MediaCodecListCompatV21(boolean includeSecure, boolean includeTunneling) {
      codecKind =
          includeSecure || includeTunneling
              ? MediaCodecList.ALL_CODECS
              : MediaCodecList.REGULAR_CODECS;
    }

    @Override
    public int getCodecCount() {
      ensureMediaCodecInfosInitialized();
      return mediaCodecInfos.length;
    }

    // incompatible types in return.
    @SuppressWarnings("nullness:return.type.incompatible")
    @Override
    public android.media.MediaCodecInfo getCodecInfoAt(int index) {
      ensureMediaCodecInfosInitialized();
      return mediaCodecInfos[index];
    }

    @Override
    public boolean secureDecodersExplicit() {
      return true;
    }

    @Override
    public boolean isFeatureSupported(
        String feature, String mimeType, CodecCapabilities capabilities) {
      return capabilities.isFeatureSupported(feature);
    }

    @Override
    public boolean isFeatureRequired(
        String feature, String mimeType, CodecCapabilities capabilities) {
      return capabilities.isFeatureRequired(feature);
    }

    @EnsuresNonNull({"mediaCodecInfos"})
    private void ensureMediaCodecInfosInitialized() {
      if (mediaCodecInfos == null) {
        mediaCodecInfos = new MediaCodecList(codecKind).getCodecInfos();
      }
    }

  }

  private static final class MediaCodecListCompatV16 implements MediaCodecListCompat {

    @Override
    public int getCodecCount() {
      return MediaCodecList.getCodecCount();
    }

    @Override
    public android.media.MediaCodecInfo getCodecInfoAt(int index) {
      return MediaCodecList.getCodecInfoAt(index);
    }

    @Override
    public boolean secureDecodersExplicit() {
      return false;
    }

    @Override
    public boolean isFeatureSupported(
        String feature, String mimeType, CodecCapabilities capabilities) {
      // Secure decoders weren't explicitly listed prior to API level 21. We assume that a secure
      // H264 decoder exists.
      return CodecCapabilities.FEATURE_SecurePlayback.equals(feature)
          && MimeTypes.VIDEO_H264.equals(mimeType);
    }

    @Override
    public boolean isFeatureRequired(
        String feature, String mimeType, CodecCapabilities capabilities) {
      return false;
    }

  }

  private static final class CodecKey {

    public final String mimeType;
    public final boolean secure;
    public final boolean tunneling;

    public CodecKey(String mimeType, boolean secure, boolean tunneling) {
      this.mimeType = mimeType;
      this.secure = secure;
      this.tunneling = tunneling;
    }

    @Override
    public int hashCode() {
      final int prime = 31;
      int result = 1;
      result = prime * result + mimeType.hashCode();
      result = prime * result + (secure ? 1231 : 1237);
      result = prime * result + (tunneling ? 1231 : 1237);
      return result;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
      if (this == obj) {
        return true;
      }
      if (obj == null || obj.getClass() != CodecKey.class) {
        return false;
      }
      CodecKey other = (CodecKey) obj;
      return TextUtils.equals(mimeType, other.mimeType)
          && secure == other.secure
          && tunneling == other.tunneling;
    }

  }

  static {
    AVC_PROFILE_NUMBER_TO_CONST = new SparseIntArray();
    AVC_PROFILE_NUMBER_TO_CONST.put(66, CodecProfileLevel.AVCProfileBaseline);
    AVC_PROFILE_NUMBER_TO_CONST.put(77, CodecProfileLevel.AVCProfileMain);
    AVC_PROFILE_NUMBER_TO_CONST.put(88, CodecProfileLevel.AVCProfileExtended);
    AVC_PROFILE_NUMBER_TO_CONST.put(100, CodecProfileLevel.AVCProfileHigh);
    AVC_PROFILE_NUMBER_TO_CONST.put(110, CodecProfileLevel.AVCProfileHigh10);
    AVC_PROFILE_NUMBER_TO_CONST.put(122, CodecProfileLevel.AVCProfileHigh422);
    AVC_PROFILE_NUMBER_TO_CONST.put(244, CodecProfileLevel.AVCProfileHigh444);

    AVC_LEVEL_NUMBER_TO_CONST = new SparseIntArray();
    AVC_LEVEL_NUMBER_TO_CONST.put(10, CodecProfileLevel.AVCLevel1);
    // TODO: Find int for CodecProfileLevel.AVCLevel1b.
    AVC_LEVEL_NUMBER_TO_CONST.put(11, CodecProfileLevel.AVCLevel11);
    AVC_LEVEL_NUMBER_TO_CONST.put(12, CodecProfileLevel.AVCLevel12);
    AVC_LEVEL_NUMBER_TO_CONST.put(13, CodecProfileLevel.AVCLevel13);
    AVC_LEVEL_NUMBER_TO_CONST.put(20, CodecProfileLevel.AVCLevel2);
    AVC_LEVEL_NUMBER_TO_CONST.put(21, CodecProfileLevel.AVCLevel21);
    AVC_LEVEL_NUMBER_TO_CONST.put(22, CodecProfileLevel.AVCLevel22);
    AVC_LEVEL_NUMBER_TO_CONST.put(30, CodecProfileLevel.AVCLevel3);
    AVC_LEVEL_NUMBER_TO_CONST.put(31, CodecProfileLevel.AVCLevel31);
    AVC_LEVEL_NUMBER_TO_CONST.put(32, CodecProfileLevel.AVCLevel32);
    AVC_LEVEL_NUMBER_TO_CONST.put(40, CodecProfileLevel.AVCLevel4);
    AVC_LEVEL_NUMBER_TO_CONST.put(41, CodecProfileLevel.AVCLevel41);
    AVC_LEVEL_NUMBER_TO_CONST.put(42, CodecProfileLevel.AVCLevel42);
    AVC_LEVEL_NUMBER_TO_CONST.put(50, CodecProfileLevel.AVCLevel5);
    AVC_LEVEL_NUMBER_TO_CONST.put(51, CodecProfileLevel.AVCLevel51);
    AVC_LEVEL_NUMBER_TO_CONST.put(52, CodecProfileLevel.AVCLevel52);

    VP9_PROFILE_NUMBER_TO_CONST = new SparseIntArray();
    VP9_PROFILE_NUMBER_TO_CONST.put(0, CodecProfileLevel.VP9Profile0);
    VP9_PROFILE_NUMBER_TO_CONST.put(1, CodecProfileLevel.VP9Profile1);
    VP9_PROFILE_NUMBER_TO_CONST.put(2, CodecProfileLevel.VP9Profile2);
    VP9_PROFILE_NUMBER_TO_CONST.put(3, CodecProfileLevel.VP9Profile3);
    VP9_LEVEL_NUMBER_TO_CONST = new SparseIntArray();
    VP9_LEVEL_NUMBER_TO_CONST.put(10, CodecProfileLevel.VP9Level1);
    VP9_LEVEL_NUMBER_TO_CONST.put(11, CodecProfileLevel.VP9Level11);
    VP9_LEVEL_NUMBER_TO_CONST.put(20, CodecProfileLevel.VP9Level2);
    VP9_LEVEL_NUMBER_TO_CONST.put(21, CodecProfileLevel.VP9Level21);
    VP9_LEVEL_NUMBER_TO_CONST.put(30, CodecProfileLevel.VP9Level3);
    VP9_LEVEL_NUMBER_TO_CONST.put(31, CodecProfileLevel.VP9Level31);
    VP9_LEVEL_NUMBER_TO_CONST.put(40, CodecProfileLevel.VP9Level4);
    VP9_LEVEL_NUMBER_TO_CONST.put(41, CodecProfileLevel.VP9Level41);
    VP9_LEVEL_NUMBER_TO_CONST.put(50, CodecProfileLevel.VP9Level5);
    VP9_LEVEL_NUMBER_TO_CONST.put(51, CodecProfileLevel.VP9Level51);
    VP9_LEVEL_NUMBER_TO_CONST.put(60, CodecProfileLevel.VP9Level6);
    VP9_LEVEL_NUMBER_TO_CONST.put(61, CodecProfileLevel.VP9Level61);
    VP9_LEVEL_NUMBER_TO_CONST.put(62, CodecProfileLevel.VP9Level62);

    HEVC_CODEC_STRING_TO_PROFILE_LEVEL = new HashMap<>();
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L30", CodecProfileLevel.HEVCMainTierLevel1);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L60", CodecProfileLevel.HEVCMainTierLevel2);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L63", CodecProfileLevel.HEVCMainTierLevel21);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L90", CodecProfileLevel.HEVCMainTierLevel3);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L93", CodecProfileLevel.HEVCMainTierLevel31);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L120", CodecProfileLevel.HEVCMainTierLevel4);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L123", CodecProfileLevel.HEVCMainTierLevel41);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L150", CodecProfileLevel.HEVCMainTierLevel5);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L153", CodecProfileLevel.HEVCMainTierLevel51);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L156", CodecProfileLevel.HEVCMainTierLevel52);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L180", CodecProfileLevel.HEVCMainTierLevel6);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L183", CodecProfileLevel.HEVCMainTierLevel61);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("L186", CodecProfileLevel.HEVCMainTierLevel62);

    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H30", CodecProfileLevel.HEVCHighTierLevel1);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H60", CodecProfileLevel.HEVCHighTierLevel2);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H63", CodecProfileLevel.HEVCHighTierLevel21);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H90", CodecProfileLevel.HEVCHighTierLevel3);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H93", CodecProfileLevel.HEVCHighTierLevel31);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H120", CodecProfileLevel.HEVCHighTierLevel4);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H123", CodecProfileLevel.HEVCHighTierLevel41);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H150", CodecProfileLevel.HEVCHighTierLevel5);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H153", CodecProfileLevel.HEVCHighTierLevel51);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H156", CodecProfileLevel.HEVCHighTierLevel52);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H180", CodecProfileLevel.HEVCHighTierLevel6);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H183", CodecProfileLevel.HEVCHighTierLevel61);
    HEVC_CODEC_STRING_TO_PROFILE_LEVEL.put("H186", CodecProfileLevel.HEVCHighTierLevel62);

    DOLBY_VISION_STRING_TO_PROFILE = new HashMap<>();
    DOLBY_VISION_STRING_TO_PROFILE.put("00", CodecProfileLevel.DolbyVisionProfileDvavPer);
    DOLBY_VISION_STRING_TO_PROFILE.put("01", CodecProfileLevel.DolbyVisionProfileDvavPen);
    DOLBY_VISION_STRING_TO_PROFILE.put("02", CodecProfileLevel.DolbyVisionProfileDvheDer);
    DOLBY_VISION_STRING_TO_PROFILE.put("03", CodecProfileLevel.DolbyVisionProfileDvheDen);
    DOLBY_VISION_STRING_TO_PROFILE.put("04", CodecProfileLevel.DolbyVisionProfileDvheDtr);
    DOLBY_VISION_STRING_TO_PROFILE.put("05", CodecProfileLevel.DolbyVisionProfileDvheStn);
    DOLBY_VISION_STRING_TO_PROFILE.put("06", CodecProfileLevel.DolbyVisionProfileDvheDth);
    DOLBY_VISION_STRING_TO_PROFILE.put("07", CodecProfileLevel.DolbyVisionProfileDvheDtb);
    DOLBY_VISION_STRING_TO_PROFILE.put("08", CodecProfileLevel.DolbyVisionProfileDvheSt);
    DOLBY_VISION_STRING_TO_PROFILE.put("09", CodecProfileLevel.DolbyVisionProfileDvavSe);

    DOLBY_VISION_STRING_TO_LEVEL = new HashMap<>();
    DOLBY_VISION_STRING_TO_LEVEL.put("01", CodecProfileLevel.DolbyVisionLevelHd24);
    DOLBY_VISION_STRING_TO_LEVEL.put("02", CodecProfileLevel.DolbyVisionLevelHd30);
    DOLBY_VISION_STRING_TO_LEVEL.put("03", CodecProfileLevel.DolbyVisionLevelFhd24);
    DOLBY_VISION_STRING_TO_LEVEL.put("04", CodecProfileLevel.DolbyVisionLevelFhd30);
    DOLBY_VISION_STRING_TO_LEVEL.put("05", CodecProfileLevel.DolbyVisionLevelFhd60);
    DOLBY_VISION_STRING_TO_LEVEL.put("06", CodecProfileLevel.DolbyVisionLevelUhd24);
    DOLBY_VISION_STRING_TO_LEVEL.put("07", CodecProfileLevel.DolbyVisionLevelUhd30);
    DOLBY_VISION_STRING_TO_LEVEL.put("08", CodecProfileLevel.DolbyVisionLevelUhd48);
    DOLBY_VISION_STRING_TO_LEVEL.put("09", CodecProfileLevel.DolbyVisionLevelUhd60);

    // See https://aomediacodec.github.io/av1-spec/av1-spec.pdf Annex A: Profiles and levels for
    // more information on mapping AV1 codec strings to levels.
    AV1_LEVEL_NUMBER_TO_CONST = new SparseIntArray();
    AV1_LEVEL_NUMBER_TO_CONST.put(0, CodecProfileLevel.AV1Level2);
    AV1_LEVEL_NUMBER_TO_CONST.put(1, CodecProfileLevel.AV1Level21);
    AV1_LEVEL_NUMBER_TO_CONST.put(2, CodecProfileLevel.AV1Level22);
    AV1_LEVEL_NUMBER_TO_CONST.put(3, CodecProfileLevel.AV1Level23);
    AV1_LEVEL_NUMBER_TO_CONST.put(4, CodecProfileLevel.AV1Level3);
    AV1_LEVEL_NUMBER_TO_CONST.put(5, CodecProfileLevel.AV1Level31);
    AV1_LEVEL_NUMBER_TO_CONST.put(6, CodecProfileLevel.AV1Level32);
    AV1_LEVEL_NUMBER_TO_CONST.put(7, CodecProfileLevel.AV1Level33);
    AV1_LEVEL_NUMBER_TO_CONST.put(8, CodecProfileLevel.AV1Level4);
    AV1_LEVEL_NUMBER_TO_CONST.put(9, CodecProfileLevel.AV1Level41);
    AV1_LEVEL_NUMBER_TO_CONST.put(10, CodecProfileLevel.AV1Level42);
    AV1_LEVEL_NUMBER_TO_CONST.put(11, CodecProfileLevel.AV1Level43);
    AV1_LEVEL_NUMBER_TO_CONST.put(12, CodecProfileLevel.AV1Level5);
    AV1_LEVEL_NUMBER_TO_CONST.put(13, CodecProfileLevel.AV1Level51);
    AV1_LEVEL_NUMBER_TO_CONST.put(14, CodecProfileLevel.AV1Level52);
    AV1_LEVEL_NUMBER_TO_CONST.put(15, CodecProfileLevel.AV1Level53);
    AV1_LEVEL_NUMBER_TO_CONST.put(16, CodecProfileLevel.AV1Level6);
    AV1_LEVEL_NUMBER_TO_CONST.put(17, CodecProfileLevel.AV1Level61);
    AV1_LEVEL_NUMBER_TO_CONST.put(18, CodecProfileLevel.AV1Level62);
    AV1_LEVEL_NUMBER_TO_CONST.put(19, CodecProfileLevel.AV1Level63);
    AV1_LEVEL_NUMBER_TO_CONST.put(20, CodecProfileLevel.AV1Level7);
    AV1_LEVEL_NUMBER_TO_CONST.put(21, CodecProfileLevel.AV1Level71);
    AV1_LEVEL_NUMBER_TO_CONST.put(22, CodecProfileLevel.AV1Level72);
    AV1_LEVEL_NUMBER_TO_CONST.put(23, CodecProfileLevel.AV1Level73);

    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE = new SparseIntArray();
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(1, CodecProfileLevel.AACObjectMain);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(2, CodecProfileLevel.AACObjectLC);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(3, CodecProfileLevel.AACObjectSSR);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(4, CodecProfileLevel.AACObjectLTP);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(5, CodecProfileLevel.AACObjectHE);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(6, CodecProfileLevel.AACObjectScalable);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(17, CodecProfileLevel.AACObjectERLC);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(20, CodecProfileLevel.AACObjectERScalable);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(23, CodecProfileLevel.AACObjectLD);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(29, CodecProfileLevel.AACObjectHE_PS);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(39, CodecProfileLevel.AACObjectELD);
    MP4A_AUDIO_OBJECT_TYPE_TO_PROFILE.put(42, CodecProfileLevel.AACObjectXHE);
  }
}
