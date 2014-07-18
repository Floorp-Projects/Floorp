/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 422698 to make sure searches with urls from the location bar
 * correctly match itself when it contains escaped characters.
 */

add_task(function* test_escape() {
  let uri1 = NetUtil.newURI("http://unescapeduri/");
  let uri2 = NetUtil.newURI("http://escapeduri/%40/");
  yield promiseAddVisits([ { uri: uri1, title: "title" },
                           { uri: uri2, title: "title" } ]);

  do_log_info("Unescaped location matches itself");
  yield check_autocomplete({
    search: "http://unescapeduri/",
    matches: [ { uri: uri1, title: "title" } ]
  });

  do_log_info("Escaped location matches itself");
  yield check_autocomplete({
    search: "http://escapeduri/%40/",
    matches: [ { uri: uri2, title: "title" } ]
  });

  yield cleanup();
});
