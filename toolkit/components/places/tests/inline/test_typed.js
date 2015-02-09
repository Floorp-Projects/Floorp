/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// First do searches with typed behavior forced to false, so later tests will
// ensure autocomplete is able to dinamically switch behavior.

add_autocomplete_test([
  "Searching for domain should autoFill it",
  "moz",
  "mozilla.org/",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
    PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  }
]);

add_autocomplete_test([
  "Searching for url should autoFill it",
  "mozilla.org/li",
  "mozilla.org/link/",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
    PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  }
]);

// Now do searches with typed behavior forced to true.

add_autocomplete_test([
  "Searching for non-typed domain should not autoFill it",
  "moz",
  "moz",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);
    PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  }
]);

add_autocomplete_test([
  "Searching for typed domain should autoFill it",
  "moz",
  "mozilla.org/",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/typed/"),
                       transition: TRANSITION_TYPED });
  }
]);

add_autocomplete_test([
  "Searching for non-typed url should not autoFill it",
  "mozilla.org/li",
  "mozilla.org/li",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);
    PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  }
]);

add_autocomplete_test([
  "Searching for typed url should autoFill it",
  "mozilla.org/li",
  "mozilla.org/link/",
  function () {
    Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);
    PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/link/"),
                       transition: TRANSITION_TYPED });
  }
]);
