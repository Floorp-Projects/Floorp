/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 471903 to make sure searching in autocomplete can be turned on
 * and off. Also test bug 463535 for pref changing search.
 */

add_task(function* test_enabled() {
  let uri = NetUtil.newURI("http://url/0");
  yield PlacesTestUtils.addVisits([ { uri: uri, title: "title" } ]);

  do_print("plain search");
  yield check_autocomplete({
    search: "url",
    matches: [ { uri: uri, title: "title" } ]
  });

  do_print("search disabled");
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", false);
  yield check_autocomplete({
    search: "url",
    matches: [ ]
  });

  do_print("resume normal search");
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", true);
  yield check_autocomplete({
    search: "url",
    matches: [ { uri: uri, title: "title" } ]
  });

  yield cleanup();
});
