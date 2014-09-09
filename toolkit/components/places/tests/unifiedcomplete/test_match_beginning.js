/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 451760 which allows matching only at the beginning of urls or
 * titles to simulate Firefox 2 functionality.
 */

add_task(function* test_match_beginning() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);

  let uri1 = NetUtil.newURI("http://x.com/y");
  let uri2 = NetUtil.newURI("https://y.com/x");
  yield promiseAddVisits([ { uri: uri1, title: "a b" },
                           { uri: uri2, title: "b a" } ]);

  do_log_info("Match at the beginning of titles");
  Services.prefs.setIntPref("browser.urlbar.matchBehavior", 3);
  yield check_autocomplete({
    search: "a",
    matches: [ { uri: uri1, title: "a b" } ]
  });

  do_log_info("Match at the beginning of titles");
  yield check_autocomplete({
    search: "b",
    matches: [ { uri: uri2, title: "b a" } ]
  });

  do_log_info("Match at the beginning of urls");
  yield check_autocomplete({
    search: "x",
    matches: [ { uri: uri1, title: "a b" } ]
  });

  do_log_info("Match at the beginning of urls");
  yield check_autocomplete({
    search: "y",
    matches: [ { uri: uri2, title: "b a" } ]
  });

  do_log_info("Sanity check that matching anywhere finds more");
  Services.prefs.setIntPref("browser.urlbar.matchBehavior", 1);
  yield check_autocomplete({
    search: "a",
    matches: [ { uri: uri1, title: "a b" },
               { uri: uri2, title: "b a" } ]
  });

  yield cleanup();
});
