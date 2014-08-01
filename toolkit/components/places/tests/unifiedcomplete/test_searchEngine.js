/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_searchEngine() {
  Services.search.addEngineWithDetails("SearchEngine", "", "", "",
                                       "GET", "http://s.example.com/search");
  let engine = Services.search.getEngineByName("SearchEngine");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));

  let uri1 = NetUtil.newURI("http://s.example.com/search?q=Terms&client=1");
  let uri2 = NetUtil.newURI("http://s.example.com/search?q=Terms&client=2");
  yield promiseAddVisits({ uri: uri1, title: "Terms - SearchEngine Search" });
  addBookmark({ uri: uri2, title: "Terms - SearchEngine Search" });

  do_log_info("Past search terms should be styled, unless bookmarked");
  yield check_autocomplete({
    search: "term",
    matches: [ { uri: uri1, title: "Terms", searchEngine: "SearchEngine" },
               { uri: uri2, title: "Terms - SearchEngine Search" } ]
  });

  yield cleanup();
});
