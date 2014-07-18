/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 422277 to make sure bad escaped uris don't get escaped. This makes
 * sure we don't hit an assertion for "not a UTF8 string".
 */

add_task(function* test_javascript_match() {
  do_log_info("Bad escaped uri stays escaped");
  let uri1 = NetUtil.newURI("http://site/%EAid");
  yield promiseAddVisits([ { uri: uri1, title: "title" } ]);
  yield check_autocomplete({
    search: "site",
    matches: [ { uri: uri1, title: "title" } ]
  });
  yield cleanup();
});
