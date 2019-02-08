/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["LightweightThemeManager"];

ChromeUtils.defineModuleGetter(this, "LightweightThemeImageOptimizer",
  "resource://gre/modules/addons/LightweightThemeImageOptimizer.jsm");

// Holds optional fallback theme data that will be returned when no data for an
// active theme can be found. This the case for WebExtension Themes, for example.
var _fallbackThemeData = null;

var LightweightThemeManager = {
  set fallbackThemeData(data) {
    if (data && Object.getOwnPropertyNames(data).length) {
      _fallbackThemeData = Object.assign({}, data);
      LightweightThemeImageOptimizer.purge();
    } else {
      _fallbackThemeData = null;
    }
    return _fallbackThemeData;
  },

  /*
   * Returns the currently active theme, taking the fallback theme into account
   * if we'd be using the default theme otherwise.
   *
   * This will always return the original theme data and not make use of
   * locally persisted resources.
   */
  get currentThemeWithFallback() {
    return _fallbackThemeData;
  },

  systemThemeChanged() {
  },

  /**
   * Handles system theme changes.
   *
   * @param  aEvent
   *         The MediaQueryListEvent associated with the system theme change.
   */
  handleEvent(aEvent) {
    if (aEvent.media == "(-moz-system-dark-theme)") {
      // Meh.
    }
  },
};
