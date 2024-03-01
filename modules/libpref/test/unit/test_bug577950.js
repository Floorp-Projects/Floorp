/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

    observe: function observe() {
      // Don't do anything.
    },
  };

  /* Set the same pref twice.  This shouldn't leak. */
  Services.prefs.addObserver("UserPref.nonexistent.setIntPref", observer);
  Services.prefs.addObserver("UserPref.nonexistent.setIntPref", observer);
}
