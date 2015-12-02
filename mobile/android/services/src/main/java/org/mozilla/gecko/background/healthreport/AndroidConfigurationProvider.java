/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.healthreport;

import org.mozilla.gecko.background.healthreport.Environment.UIType;
import org.mozilla.gecko.background.healthreport.EnvironmentBuilder.ConfigurationProvider;
import org.mozilla.gecko.util.HardwareUtils;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;

public class AndroidConfigurationProvider implements ConfigurationProvider {
  private static final float MILLIMETERS_PER_INCH = 25.4f;

  private final Configuration configuration;
  private final DisplayMetrics displayMetrics;

  public AndroidConfigurationProvider(final Context context) {
    final Resources resources = context.getResources();
    this.configuration = resources.getConfiguration();
    this.displayMetrics = resources.getDisplayMetrics();

    HardwareUtils.init(context);
  }

  @Override
  public boolean hasHardwareKeyboard() {
    return configuration.keyboard != Configuration.KEYBOARD_NOKEYS;
  }

  @Override
  public UIType getUIType() {
    if (HardwareUtils.isLargeTablet()) {
      return UIType.LARGE_TABLET;
    }

    if (HardwareUtils.isSmallTablet()) {
      return UIType.SMALL_TABLET;
    }

    return UIType.DEFAULT;
  }

  @Override
  public int getUIModeType() {
    return configuration.uiMode & Configuration.UI_MODE_TYPE_MASK;
  }

  @Override
  public int getScreenLayoutSize() {
    return configuration.screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK;
  }

  /**
   * Calculate screen horizontal width, in millimeters.
   * This is approximate, will be wrong on some devices, and
   * most likely doesn't include screen area that the app doesn't own.
   * http://stackoverflow.com/questions/2193457/is-there-a-way-to-determine-android-physical-screen-height-in-cm-or-inches
   */
  @Override
  public int getScreenXInMM() {
    return Math.round((displayMetrics.widthPixels / displayMetrics.xdpi) * MILLIMETERS_PER_INCH);
  }

  /**
   * @see #getScreenXInMM() for caveats.
   */
  @Override
  public int getScreenYInMM() {
    return Math.round((displayMetrics.heightPixels / displayMetrics.ydpi) * MILLIMETERS_PER_INCH);
  }
}
