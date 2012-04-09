/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure inline autocomplete doesn't return zero frecency pages.

add_autocomplete_test([
  "Searching for zero frecency domain should not autoFill it",
  "moz",
  "moz",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
    addVisits({ uri: NetUtil.newURI("http://mozilla.org/framed_link/"),
                transition: TRANSITION_FRAMED_LINK });
  }
]);

add_autocomplete_test([
  "Searching for zero frecency url should not autoFill it",
  "mozilla.org/f",
  "mozilla.org/f",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
    addVisits({ uri: NetUtil.newURI("http://mozilla.org/framed_link/"),
                transition: TRANSITION_FRAMED_LINK });
  }
]);
