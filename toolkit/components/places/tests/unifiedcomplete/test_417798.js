/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 417798 to make sure javascript: URIs don't show up unless the
 * user searches for javascript: explicitly.
 */

add_task(function* test_javascript_match() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);

  let uri1 = NetUtil.newURI("http://abc/def");
  let uri2 = NetUtil.newURI("javascript:5");
  yield promiseAddVisits([ { uri: uri1, title: "Title with javascript:" } ]);
  addBookmark({ uri: uri2,
                title: "Title with javascript:" });

  do_log_info("Match non-javascript: with plain search");
  yield check_autocomplete({
    search: "a",
    matches: [ { uri: uri1, title: "Title with javascript:" } ]
  });

  do_log_info("Match non-javascript: with almost javascript:");
  yield check_autocomplete({
    search: "javascript",
    matches: [ { uri: uri1, title: "Title with javascript:" } ]
  });

  do_log_info("Match javascript:");
  yield check_autocomplete({
    search: "javascript:",
    matches: [ { uri: uri1, title: "Title with javascript:" },
               { uri: uri2, title: "Title with javascript:" } ]
  });

  do_log_info("Match nothing with non-first javascript:");
  yield check_autocomplete({
    search: "5 javascript:",
    matches: [ ]
  });

  do_log_info("Match javascript: with multi-word search");
  yield check_autocomplete({
    search: "javascript: 5",
    matches: [ { uri: uri2, title: "Title with javascript:" } ]
  });

  yield cleanup();
});
