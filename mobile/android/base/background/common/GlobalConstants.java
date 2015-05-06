/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common;

import org.mozilla.gecko.AppConstants;

/**
 * Preprocessed class for storing preprocessed values common to all
 * Android services.
 */
public class GlobalConstants {
  public static final String BROWSER_INTENT_PACKAGE = AppConstants.ANDROID_PACKAGE_NAME;
  public static final String BROWSER_INTENT_CLASS = AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS;

  /**
   * Bug 800244: this signing-level permission protects broadcast intents that
   * should be received only by the Firefox versions with the given Android
   * package name.
   */
  public static final String PER_ANDROID_PACKAGE_PERMISSION = AppConstants.ANDROID_PACKAGE_NAME + ".permission.PER_ANDROID_PACKAGE";

  public static final int SHARED_PREFERENCES_MODE = 0;

  // These are used to ask Fennec (via reflection) to send
  // us a pref notification. This avoids us having to guess
  // Fennec's prefs branch and pref name.
  // Eventually Fennec might listen to startup notifications and
  // do this automatically, but this will do for now. See Bug 800244.
  public static String GECKO_PREFERENCES_CLASS = "org.mozilla.gecko.preferences.GeckoPreferences";
  public static String GECKO_BROADCAST_HEALTHREPORT_UPLOAD_PREF_METHOD  = "broadcastHealthReportUploadPref";
  public static String GECKO_BROADCAST_HEALTHREPORT_PRUNE_METHOD = "broadcastHealthReportPrune";

  // Common time values.
  public static final long MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;
  public static final long MILLISECONDS_PER_SIX_MONTHS = 180 * MILLISECONDS_PER_DAY;
}
