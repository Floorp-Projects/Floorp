/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;
import android.view.InputDevice;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.InputDeviceUtils;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoSystemStateListener implements InputManager.InputDeviceListener {
  private static final String LOGTAG = "SystemStateListener";

  private static final GeckoSystemStateListener listenerInstance = new GeckoSystemStateListener();

  private boolean mInitialized;
  private ContentObserver mContentObserver;
  private static Context sApplicationContext;
  private InputManager mInputManager;
  private boolean mIsNightMode;

  public static GeckoSystemStateListener getInstance() {
    return listenerInstance;
  }

  private GeckoSystemStateListener() {}

  public synchronized void initialize(final Context context) {
    if (mInitialized) {
      Log.w(LOGTAG, "Already initialized!");
      return;
    }
    mInputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
    mInputManager.registerInputDeviceListener(listenerInstance, ThreadUtils.getUiHandler());

    sApplicationContext = context;
    final ContentResolver contentResolver = sApplicationContext.getContentResolver();
    final Uri animationSetting = Settings.System.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE);
    mContentObserver =
        new ContentObserver(new Handler(Looper.getMainLooper())) {
          @Override
          public void onChange(final boolean selfChange) {
            onDeviceChanged();
          }
        };
    contentResolver.registerContentObserver(animationSetting, false, mContentObserver);

    final Uri invertSetting =
        Settings.Secure.getUriFor(Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED);
    contentResolver.registerContentObserver(invertSetting, false, mContentObserver);

    final Uri textContrastSetting =
        Settings.Secure.getUriFor(
            /*Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED*/ "high_text_contrast_enabled");
    contentResolver.registerContentObserver(textContrastSetting, false, mContentObserver);

    mIsNightMode =
        (sApplicationContext.getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK)
            == Configuration.UI_MODE_NIGHT_YES;

    mInitialized = true;
  }

  public synchronized void shutdown() {
    if (!mInitialized) {
      Log.w(LOGTAG, "Already shut down!");
      return;
    }

    if (mInputManager == null) {
      Log.e(LOGTAG, "mInputManager should be valid!");
      return;
    }

    mInputManager.unregisterInputDeviceListener(listenerInstance);

    final ContentResolver contentResolver = sApplicationContext.getContentResolver();
    contentResolver.unregisterContentObserver(mContentObserver);

    mInitialized = false;
    mInputManager = null;
    mContentObserver = null;
  }

  @WrapForJNI(calledFrom = "gecko")
  /**
   * For prefers-reduced-motion media queries feature.
   *
   * <p>Uses `Settings.Global` which was introduced in API version 17.
   */
  private static boolean prefersReducedMotion() {
    final ContentResolver contentResolver = sApplicationContext.getContentResolver();

    return Settings.Global.getFloat(contentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1)
        == 0.0f;
  }

  @WrapForJNI(calledFrom = "gecko")
  /**
   * For inverted-colors queries feature.
   *
   * <p>Uses `Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED` which was introduced in API
   * version 21.
   */
  private static boolean isInvertedColors() {
    final ContentResolver contentResolver = sApplicationContext.getContentResolver();

    return Settings.Secure.getInt(
            contentResolver, Settings.Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED, 0)
        == 1;
  }

  @WrapForJNI(calledFrom = "gecko")
  /**
   * For prefers-contrast queries feature.
   *
   * <p>Uses `Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED` which was introduced in API
   * version 21.
   */
  private static boolean prefersContrast() {
    final ContentResolver contentResolver = sApplicationContext.getContentResolver();

    return Settings.Secure.getInt(
            contentResolver, /*Settings.Secure.ACCESSIBILITY_HIGH_TEXT_CONTRAST_ENABLED*/
            "high_text_contrast_enabled",
            0)
        == 1;
  }

  /** For prefers-color-scheme media queries feature. */
  public boolean isNightMode() {
    return mIsNightMode;
  }

  public void updateNightMode(final int newUIMode) {
    final boolean isNightMode =
        (newUIMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
    if (isNightMode == mIsNightMode) {
      return;
    }
    mIsNightMode = isNightMode;
    onDeviceChanged();
  }

  @WrapForJNI(stubName = "OnDeviceChanged", calledFrom = "any", dispatchTo = "gecko")
  private static native void nativeOnDeviceChanged();

  public static void onDeviceChanged() {
    if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
      nativeOnDeviceChanged();
    } else {
      GeckoThread.queueNativeCallUntil(
          GeckoThread.State.PROFILE_READY, GeckoSystemStateListener.class, "nativeOnDeviceChanged");
    }
  }

  private void notifyDeviceChanged(final int deviceId) {
    final InputDevice device = InputDevice.getDevice(deviceId);
    if (device == null || !InputDeviceUtils.isPointerTypeDevice(device)) {
      return;
    }
    onDeviceChanged();
  }

  @Override
  public void onInputDeviceAdded(final int deviceId) {
    notifyDeviceChanged(deviceId);
  }

  @Override
  public void onInputDeviceRemoved(final int deviceId) {
    // Call onDeviceChanged directly without checking device source types
    // since we can no longer get a valid `InputDevice` in the case of
    // device removal.
    onDeviceChanged();
  }

  @Override
  public void onInputDeviceChanged(final int deviceId) {
    notifyDeviceChanged(deviceId);
  }
}
