/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.provider.Settings;
import android.util.Log;
import androidx.annotation.UiThread;
import org.mozilla.gecko.util.ThreadUtils;

/**
 * A class that automatically adjusts font size settings for web content in Gecko in accordance with
 * the device's OS font scale setting.
 *
 * @see android.provider.Settings.System#FONT_SCALE
 */
/* package */ final class GeckoFontScaleListener extends ContentObserver {
  private static final String LOGTAG = "GeckoFontScaleListener";

  private static final float DEFAULT_FONT_SCALE = 1.0f;

  // We're referencing the *application* context, so this is in fact okay.
  @SuppressLint("StaticFieldLeak")
  private static final GeckoFontScaleListener sInstance = new GeckoFontScaleListener();

  private Context mApplicationContext;
  private GeckoRuntimeSettings mSettings;

  private boolean mAttached;
  private boolean mEnabled;
  private boolean mRunning;

  private float mPrevGeckoFontScale;

  public static GeckoFontScaleListener getInstance() {
    return sInstance;
  }

  private GeckoFontScaleListener() {
    // Ensure the ContentObserver callback runs on the UI thread.
    super(ThreadUtils.getUiHandler());
  }

  /**
   * Prepare the GeckoFontScaleListener for usage. If it has been previously enabled, it will now
   * start actively working.
   */
  public void attachToContext(final Context context, final GeckoRuntimeSettings settings) {
    ThreadUtils.assertOnUiThread();

    if (mAttached) {
      Log.w(LOGTAG, "Already attached!");
      return;
    }

    mAttached = true;
    mSettings = settings;
    mApplicationContext = context.getApplicationContext();
    onEnabledChange();
  }

  /**
   * Detaches the context and also stops the GeckoFontScaleListener if it was previously enabled.
   * This will also restore the previously used font size settings.
   */
  public void detachFromContext() {
    ThreadUtils.assertOnUiThread();

    if (!mAttached) {
      Log.w(LOGTAG, "Already detached!");
      return;
    }

    stop();
    mApplicationContext = null;
    mSettings = null;
    mAttached = false;
  }

  /**
   * Controls whether the GeckoFontScaleListener should automatically adjust font sizes for web
   * content in Gecko. When disabling, this will restore the previously used font size settings.
   *
   * <p>This method can be called at any time, but the GeckoFontScaleListener won't start actively
   * adjusting font sizes until it has been attached to a context.
   *
   * @param enabled True if automatic font size setting should be enabled.
   */
  public void setEnabled(final boolean enabled) {
    ThreadUtils.assertOnUiThread();
    mEnabled = enabled;
    onEnabledChange();
  }

  /**
   * Get whether the GeckoFontScaleListener is currently enabled.
   *
   * @return True if the GeckoFontScaleListener is currently enabled.
   */
  public boolean getEnabled() {
    return mEnabled;
  }

  private void onEnabledChange() {
    if (!mAttached) {
      return;
    }

    if (mEnabled) {
      start();
    } else {
      stop();
    }
  }

  private void start() {
    if (mRunning) {
      return;
    }

    mPrevGeckoFontScale = mSettings.getFontSizeFactor();
    final ContentResolver contentResolver = mApplicationContext.getContentResolver();
    final Uri fontSizeSetting = Settings.System.getUriFor(Settings.System.FONT_SCALE);
    contentResolver.registerContentObserver(fontSizeSetting, false, this);
    onSystemFontScaleChange(contentResolver, false);

    mRunning = true;
  }

  private void stop() {
    if (!mRunning) {
      return;
    }

    final ContentResolver contentResolver = mApplicationContext.getContentResolver();
    contentResolver.unregisterContentObserver(this);
    onSystemFontScaleChange(contentResolver, /*stopping*/ true);

    mRunning = false;
  }

  private void onSystemFontScaleChange(
      final ContentResolver contentResolver, final boolean stopping) {
    float fontScale;

    if (!stopping) { // Either we were enabled, or else the system font scale changed.
      fontScale =
          Settings.System.getFloat(contentResolver, Settings.System.FONT_SCALE, DEFAULT_FONT_SCALE);
      // Older Android versions don't sanitize the FONT_SCALE value. See Bug 1656078.
      if (fontScale < 0) {
        fontScale = DEFAULT_FONT_SCALE;
      }
    } else { // We were turned off.
      fontScale = mPrevGeckoFontScale;
    }

    mSettings.setFontSizeFactorInternal(fontScale);
  }

  @UiThread // See constructor.
  @Override
  public void onChange(final boolean selfChange) {
    onSystemFontScaleChange(mApplicationContext.getContentResolver(), false);
  }
}
