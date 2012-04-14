/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import android.content.SharedPreferences;

/**
 * Implement PrefsSource to allow other components to fetch a SharedPreferences
 * instance via a Context that you provide.
 *
 * This allows components to use SharedPreferences without being tightly
 * coupled to an Activity.
 *
 * @author rnewman
 *
 */
public interface PrefsSource {
  /**
   * Return a SharedPreferences instance.
   * @param name
   *        A String, used to identify a preferences 'branch'. Must not be null.
   * @param mode
   *        A bitmask mode, as described in http://developer.android.com/reference/android/content/Context.html#getSharedPreferences%28java.lang.String,%20int%29.
   * @return
   *        A new or existing SharedPreferences instance.
   */
  public SharedPreferences getPrefs(String name, int mode);
}
