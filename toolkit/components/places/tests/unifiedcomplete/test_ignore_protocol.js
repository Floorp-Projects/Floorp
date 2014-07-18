/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424509 to make sure searching for "h" doesn't match "http" of urls.
 */

add_task(function* test_escape() {
  let uri1 = NetUtil.newURI("http://site/");
  let uri2 = NetUtil.newURI("http://happytimes/");
  yield promiseAddVisits([ { uri: uri1, title: "title" },
                           { uri: uri2, title: "title" } ]);

  do_log_info("Searching for h matches site and not http://");
  yield check_autocomplete({
    search: "h",
    matches: [ { uri: uri2, title: "title" } ]
  });

  yield cleanup();
});
