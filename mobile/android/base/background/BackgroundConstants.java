/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background;

/**
 * Constants that are not specific to any individual background service.
 */
public class BackgroundConstants {

  public static final int SHARED_PREFERENCES_MODE = 0;
  // These are used to ask Fennec (via reflection) to send
  // us a pref notification. This avoids us having to guess
  // Fennec's prefs branch and pref name.
  // Eventually Fennec might listen to startup notifications and
  // do this automatically, but this will do for now. See Bug 800244.
  public static String GECKO_PREFERENCES_CLASS = "org.mozilla.gecko.GeckoPreferences";
  public static String GECKO_BROADCAST_METHOD  = "broadcastAnnouncementsPref";
}