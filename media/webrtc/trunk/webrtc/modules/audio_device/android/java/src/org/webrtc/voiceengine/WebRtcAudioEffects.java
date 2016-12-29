/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.voiceengine;

import android.annotation.TargetApi;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.media.audiofx.AutomaticGainControl;
import android.media.audiofx.NoiseSuppressor;
import android.os.Build;

import android.util.Log;

import java.util.List;

import java.util.UUID;

// This class wraps control of three different platform effects. Supported
// effects are: AcousticEchoCanceler (AEC), AutomaticGainControl (AGC) and
// NoiseSuppressor (NS). Calling enable() will active all effects that are
// supported by the device if the corresponding |shouldEnableXXX| member is set.
class WebRtcAudioEffects {
  private static final boolean DEBUG = false;

  private static final String TAG = "WebRtcAudioEffects";

  // UUIDs for Software Audio Effects that we want to avoid using.
  // The implementor field will be set to "The Android Open Source Project".
  private static final UUID AOSP_ACOUSTIC_ECHO_CANCELER =
      UUID.fromString("bb392ec0-8d4d-11e0-a896-0002a5d5c51b");
  private static final UUID AOSP_AUTOMATIC_GAIN_CONTROL =
      UUID.fromString("aa8130e0-66fc-11e0-bad0-0002a5d5c51b");
  private static final UUID AOSP_NOISE_SUPPRESSOR =
      UUID.fromString("c06c8400-8e06-11e0-9cb6-0002a5d5c51b");

  // Static Boolean objects used to avoid expensive queries more than once.
  // The first result is cached in these members and then reused if needed.
  // Each member is null until it has been evaluated/set for the first time.
  private static Boolean canUseAcousticEchoCanceler = null;
  private static Boolean canUseAutomaticGainControl = null;
  private static Boolean canUseNoiseSuppressor = null;

  // Contains the audio effect objects. Created in enable() and destroyed
  // in release().
  private AcousticEchoCanceler aec = null;
  private AutomaticGainControl agc = null;
  private NoiseSuppressor ns = null;

  // Affects the final state given to the setEnabled() method on each effect.
  // The default state is set to "disabled" but each effect can also be enabled
  // by calling setAEC(), setAGC() and setNS().
  // To enable an effect, both the shouldEnableXXX member and the static
  // canUseXXX() must be true.
  private boolean shouldEnableAec = false;
  private boolean shouldEnableAgc = false;
  private boolean shouldEnableNs = false;

  // Checks if the device implements Acoustic Echo Cancellation (AEC).
  // Returns true if the device implements AEC, false otherwise.
  public static boolean isAcousticEchoCancelerSupported() {
    return WebRtcAudioUtils.runningOnJellyBeanOrHigher()
        && AcousticEchoCanceler.isAvailable();
  }

  // Checks if the device implements Automatic Gain Control (AGC).
  // Returns true if the device implements AGC, false otherwise.
  public static boolean isAutomaticGainControlSupported() {
    return WebRtcAudioUtils.runningOnJellyBeanOrHigher()
        && AutomaticGainControl.isAvailable();
  }

  // Checks if the device implements Noise Suppression (NS).
  // Returns true if the device implements NS, false otherwise.
  public static boolean isNoiseSuppressorSupported() {
    return WebRtcAudioUtils.runningOnJellyBeanOrHigher()
        && NoiseSuppressor.isAvailable();
  }

  // Returns true if the device is blacklisted for HW AEC usage.
  public static boolean isAcousticEchoCancelerBlacklisted() {
    List<String> blackListedModels =
        WebRtcAudioUtils.getBlackListedModelsForAecUsage();
    boolean isBlacklisted = blackListedModels.contains(Build.MODEL);
    if (isBlacklisted) {
      Log.w(TAG, Build.MODEL + " is blacklisted for HW AEC usage!");
    }
    return isBlacklisted;
  }

  // Returns true if the device is blacklisted for HW AGC usage.
  public static boolean isAutomaticGainControlBlacklisted() {
   List<String> blackListedModels =
        WebRtcAudioUtils.getBlackListedModelsForAgcUsage();
    boolean isBlacklisted = blackListedModels.contains(Build.MODEL);
    if (isBlacklisted) {
      Log.w(TAG, Build.MODEL + " is blacklisted for HW AGC usage!");
    }
    return isBlacklisted;
  }

  // Returns true if the device is blacklisted for HW NS usage.
  public static boolean isNoiseSuppressorBlacklisted() {
    List<String> blackListedModels =
        WebRtcAudioUtils.getBlackListedModelsForNsUsage();
    boolean isBlacklisted = blackListedModels.contains(Build.MODEL);
    if (isBlacklisted) {
      Log.w(TAG, Build.MODEL + " is blacklisted for HW NS usage!");
    }
    return isBlacklisted;
  }

  // Returns true if the platform AEC should be excluded based on its UUID.
  // AudioEffect.queryEffects() can throw IllegalStateException.
  @TargetApi(18)
  private static boolean isAcousticEchoCancelerExcludedByUUID() {
    for (Descriptor d : AudioEffect.queryEffects()) {
      if (d.type.equals(AudioEffect.EFFECT_TYPE_AEC) &&
          d.uuid.equals(AOSP_ACOUSTIC_ECHO_CANCELER)) {
        return true;
      }
    }
    return false;
  }

  // Returns true if the platform AGC should be excluded based on its UUID.
  // AudioEffect.queryEffects() can throw IllegalStateException.
  @TargetApi(18)
  private static boolean isAutomaticGainControlExcludedByUUID() {
    for (Descriptor d : AudioEffect.queryEffects()) {
      if (d.type.equals(AudioEffect.EFFECT_TYPE_AGC) &&
          d.uuid.equals(AOSP_AUTOMATIC_GAIN_CONTROL)) {
        return true;
      }
    }
    return false;
  }

  // Returns true if the platform NS should be excluded based on its UUID.
  // AudioEffect.queryEffects() can throw IllegalStateException.
  @TargetApi(18)
  private static boolean isNoiseSuppressorExcludedByUUID() {
    for (Descriptor d : AudioEffect.queryEffects()) {
      if (d.type.equals(AudioEffect.EFFECT_TYPE_NS) &&
          d.uuid.equals(AOSP_NOISE_SUPPRESSOR)) {
        return true;
      }
    }
    return false;
  }

  // Returns true if all conditions for supporting the HW AEC are fulfilled.
  // It will not be possible to enable the HW AEC if this method returns false.
  public static boolean canUseAcousticEchoCanceler() {
    if (canUseAcousticEchoCanceler == null) {
      canUseAcousticEchoCanceler = new Boolean(
          isAcousticEchoCancelerSupported()
          && !WebRtcAudioUtils.useWebRtcBasedAcousticEchoCanceler()
          && !isAcousticEchoCancelerBlacklisted()
          && !isAcousticEchoCancelerExcludedByUUID());
      Log.d(TAG, "canUseAcousticEchoCanceler: "
          + canUseAcousticEchoCanceler);
    }
    return canUseAcousticEchoCanceler;
  }

  // Returns true if all conditions for supporting the HW AGC are fulfilled.
  // It will not be possible to enable the HW AGC if this method returns false.
  public static boolean canUseAutomaticGainControl() {
    if (canUseAutomaticGainControl == null) {
      canUseAutomaticGainControl = new Boolean(
          isAutomaticGainControlSupported()
          && !WebRtcAudioUtils.useWebRtcBasedAutomaticGainControl()
          && !isAutomaticGainControlBlacklisted()
          && !isAutomaticGainControlExcludedByUUID());
      Log.d(TAG, "canUseAutomaticGainControl: "
          + canUseAutomaticGainControl);
    }
    return canUseAutomaticGainControl;
  }

  // Returns true if all conditions for supporting the HW NS are fulfilled.
  // It will not be possible to enable the HW NS if this method returns false.
  public static boolean canUseNoiseSuppressor() {
    if (canUseNoiseSuppressor == null) {
      canUseNoiseSuppressor = new Boolean(
          isNoiseSuppressorSupported()
          && !WebRtcAudioUtils.useWebRtcBasedNoiseSuppressor()
          && !isNoiseSuppressorBlacklisted()
          && !isNoiseSuppressorExcludedByUUID());
      Log.d(TAG, "canUseNoiseSuppressor: " + canUseNoiseSuppressor);
    }
    return canUseNoiseSuppressor;
  }

  static WebRtcAudioEffects create() {
    // Return null if VoIP effects (AEC, AGC and NS) are not supported.
    if (!WebRtcAudioUtils.runningOnJellyBeanOrHigher()) {
      Log.w(TAG, "API level 16 or higher is required!");
      return null;
    }
    return new WebRtcAudioEffects();
  }

  private WebRtcAudioEffects() {
    Log.d(TAG, "ctor" + WebRtcAudioUtils.getThreadInfo());
  }

  // Call this method to enable or disable the platform AEC. It modifies
  // |shouldEnableAec| which is used in enable() where the actual state
  // of the AEC effect is modified. Returns true if HW AEC is supported and
  // false otherwise.
  public boolean setAEC(boolean enable) {
    Log.d(TAG, "setAEC(" + enable + ")");
    if (!canUseAcousticEchoCanceler()) {
      Log.w(TAG, "Platform AEC is not supported");
      shouldEnableAec = false;
      return false;
    }
    if (aec != null && (enable != shouldEnableAec)) {
      Log.e(TAG, "Platform AEC state can't be modified while recording");
      return false;
    }
    shouldEnableAec = enable;
    return true;
  }

  // Call this method to enable or disable the platform AGC. It modifies
  // |shouldEnableAgc| which is used in enable() where the actual state
  // of the AGC effect is modified. Returns true if HW AGC is supported and
  // false otherwise.
  public boolean setAGC(boolean enable) {
    Log.d(TAG, "setAGC(" + enable + ")");
    if (!canUseAutomaticGainControl()) {
      Log.w(TAG, "Platform AGC is not supported");
      shouldEnableAgc = false;
      return false;
    }
    if (agc != null && (enable != shouldEnableAgc)) {
      Log.e(TAG, "Platform AGC state can't be modified while recording");
      return false;
    }
    shouldEnableAgc = enable;
    return true;
  }

  // Call this method to enable or disable the platform NS. It modifies
  // |shouldEnableNs| which is used in enable() where the actual state
  // of the NS effect is modified. Returns true if HW NS is supported and
  // false otherwise.
  public boolean setNS(boolean enable) {
    Log.d(TAG, "setNS(" + enable + ")");
    if (!canUseNoiseSuppressor()) {
      Log.w(TAG, "Platform NS is not supported");
      shouldEnableNs = false;
      return false;
    }
    if (ns != null && (enable != shouldEnableNs)) {
      Log.e(TAG, "Platform NS state can't be modified while recording");
      return false;
    }
    shouldEnableNs = enable;
    return true;
  }

  public void enable(int audioSession) {
    Log.d(TAG, "enable(audioSession=" + audioSession + ")");
    assertTrue(aec == null);
    assertTrue(agc == null);
    assertTrue(ns == null);

    // Add logging of supported effects but filter out "VoIP effects", i.e.,
    // AEC, AEC and NS.
    for (Descriptor d : AudioEffect.queryEffects()) {
      if (effectTypeIsVoIP(d.type) || DEBUG) {
        Log.d(TAG, "name: " + d.name + ", "
            + "mode: " + d.connectMode + ", "
            + "implementor: " + d.implementor + ", "
            + "UUID: " + d.uuid);
      }
    }

    if (isAcousticEchoCancelerSupported()) {
      // Create an AcousticEchoCanceler and attach it to the AudioRecord on
      // the specified audio session.
      aec = AcousticEchoCanceler.create(audioSession);
      if (aec != null) {
        boolean enabled = aec.getEnabled();
        boolean enable = shouldEnableAec && canUseAcousticEchoCanceler();
        if (aec.setEnabled(enable) != AudioEffect.SUCCESS) {
          Log.e(TAG, "Failed to set the AcousticEchoCanceler state");
        }
        Log.d(TAG, "AcousticEchoCanceler: was "
            + (enabled ? "enabled" : "disabled")
            + ", enable: " + enable + ", is now: "
            + (aec.getEnabled() ? "enabled" : "disabled"));
      } else {
        Log.e(TAG, "Failed to create the AcousticEchoCanceler instance");
      }
    }

    if (isAutomaticGainControlSupported()) {
      // Create an AutomaticGainControl and attach it to the AudioRecord on
      // the specified audio session.
      agc = AutomaticGainControl.create(audioSession);
      if (agc != null) {
        boolean enabled = agc.getEnabled();
        boolean enable = shouldEnableAgc && canUseAutomaticGainControl();
        if (agc.setEnabled(enable) != AudioEffect.SUCCESS) {
          Log.e(TAG, "Failed to set the AutomaticGainControl state");
        }
        Log.d(TAG, "AutomaticGainControl: was "
            + (enabled ? "enabled" : "disabled")
            + ", enable: " + enable + ", is now: "
            + (agc.getEnabled() ? "enabled" : "disabled"));
      } else {
        Log.e(TAG, "Failed to create the AutomaticGainControl instance");
      }
    }

    if (isNoiseSuppressorSupported()) {
      // Create an NoiseSuppressor and attach it to the AudioRecord on the
      // specified audio session.
      ns = NoiseSuppressor.create(audioSession);
      if (ns != null) {
        boolean enabled = ns.getEnabled();
        boolean enable = shouldEnableNs && canUseNoiseSuppressor();
        if (ns.setEnabled(enable) != AudioEffect.SUCCESS) {
          Log.e(TAG, "Failed to set the NoiseSuppressor state");
        }
        Log.d(TAG, "NoiseSuppressor: was "
            + (enabled ? "enabled" : "disabled")
            + ", enable: " + enable + ", is now: "
            + (ns.getEnabled() ? "enabled" : "disabled"));
      } else {
        Log.e(TAG, "Failed to create the NoiseSuppressor instance");
      }
    }
  }

  // Releases all native audio effect resources. It is a good practice to
  // release the effect engine when not in use as control can be returned
  // to other applications or the native resources released.
  public void release() {
    Log.d(TAG, "release");
    if (aec != null) {
      aec.release();
      aec = null;
    }
    if (agc != null) {
      agc.release();
      agc = null;
    }
    if (ns != null) {
      ns.release();
      ns = null;
    }
  }

  // Returns true for effect types in |type| that are of "VoIP" types:
  // Acoustic Echo Canceler (AEC) or Automatic Gain Control (AGC) or
  // Noise Suppressor (NS). Note that, an extra check for support is needed
  // in each comparison since some devices includes effects in the
  // AudioEffect.Descriptor array that are actually not available on the device.
  // As an example: Samsung Galaxy S6 includes an AGC in the descriptor but
  // AutomaticGainControl.isAvailable() returns false.
  @TargetApi(18)
  private boolean effectTypeIsVoIP(UUID type) {
    if (!WebRtcAudioUtils.runningOnJellyBeanMR2OrHigher())
      return false;

    return (AudioEffect.EFFECT_TYPE_AEC.equals(type)
        && isAcousticEchoCancelerSupported())
        || (AudioEffect.EFFECT_TYPE_AGC.equals(type)
        && isAutomaticGainControlSupported())
        || (AudioEffect.EFFECT_TYPE_NS.equals(type)
        && isNoiseSuppressorSupported());
  }

  // Helper method which throws an exception when an assertion has failed.
  private static void assertTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected condition to be true");
    }
  }
}
