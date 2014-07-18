/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let gTabRestrictChar = "%";

add_task(function* test_tab_matches() {
  let uri1 = NetUtil.newURI("http://abc.com/");
  let uri2 = NetUtil.newURI("http://xyz.net/");
  let uri3 = NetUtil.newURI("about:mozilla");
  let uri4 = NetUtil.newURI("data:text/html,test");
  yield promiseAddVisits([ { uri: uri1, title: "ABC rocks" },
                           { uri: uri2, title: "xyz.net - we're better than ABC" } ]);
  addOpenPages(uri1, 1);
  // Pages that cannot be registered in history.
  addOpenPages(uri3, 1);
  addOpenPages(uri4, 1);

  do_log_info("single result, that is also a tab match");
  yield check_autocomplete({
    search: "abc.com",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,http://abc.com/"), title: "ABC rocks" } ]
  });

  do_log_info("two results, one tab match");
  yield check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,http://abc.com/"), title: "ABC rocks" },
               { uri: uri2, title: "xyz.net - we're better than ABC" } ]
  });

  do_log_info("two results, both tab matches");
  addOpenPages(uri2, 1);
  yield check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,http://abc.com/"), title: "ABC rocks" },
               { uri: NetUtil.newURI("moz-action:switchtab,http://xyz.net/"), title: "xyz.net - we're better than ABC" } ]
  });

  do_log_info("two results, both tab matches, one has multiple tabs");
  addOpenPages(uri2, 5);
  yield check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,http://abc.com/"), title: "ABC rocks" },
               { uri: NetUtil.newURI("moz-action:switchtab,http://xyz.net/"), title: "xyz.net - we're better than ABC" } ]
  });

  do_log_info("two results, no tab matches");
  removeOpenPages(uri1, 1);
  removeOpenPages(uri2, 6);
  yield check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ { uri: uri1, title: "ABC rocks" },
               { uri: uri2, title: "xyz.net - we're better than ABC" } ]
  });

  do_log_info("tab match search with restriction character");
  addOpenPages(uri1, 1);
  yield check_autocomplete({
    search: gTabRestrictChar + " abc",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,http://abc.com/"), title: "ABC rocks" } ]
  });

  do_log_info("tab match with not-addable pages");
  yield check_autocomplete({
    search: "mozilla",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,about:mozilla"), title: "about:mozilla" } ]
  });

  do_log_info("tab match with not-addable pages and restriction character");
  yield check_autocomplete({
    search: gTabRestrictChar + " mozilla",
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,about:mozilla"), title: "about:mozilla" } ]
  });

  do_log_info("tab match with not-addable pages and only restriction character");
  yield check_autocomplete({
    search: gTabRestrictChar,
    searchParam: "enable-actions",
    matches: [ { uri: NetUtil.newURI("moz-action:switchtab,http://abc.com/"), title: "ABC rocks" },
               { uri: NetUtil.newURI("moz-action:switchtab,about:mozilla"), title: "about:mozilla" },
               { uri: NetUtil.newURI("moz-action:switchtab,data:text/html,test"), title: "data:text/html,test" } ]
  });

  yield cleanup();
});
