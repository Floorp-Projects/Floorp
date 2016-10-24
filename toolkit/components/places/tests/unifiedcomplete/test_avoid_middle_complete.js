/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_prefix_space_noautofill() {
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://moz.org/test/"),
    transition: TRANSITION_TYPED
  });

  do_print("Should not try to autoFill if search string contains a space");
  yield check_autocomplete({
    search: " mo",
    autofilled: " mo",
    completed: " mo"
  });

  yield cleanup();
});

add_task(function* test_trailing_space_noautofill() {
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://moz.org/test/"),
    transition: TRANSITION_TYPED
  });

  do_print("Should not try to autoFill if search string contains a space");
  yield check_autocomplete({
    search: "mo ",
    autofilled: "mo ",
    completed: "mo "
  });

  yield cleanup();
});

add_task(function* test_searchEngine_autofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("CakeSearch", "", "", "",
                                       "GET", "http://cake.search/");
  let engine = Services.search.getEngineByName("CakeSearch");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));

  do_print("Should autoFill search engine if search string does not contains a space");
  yield check_autocomplete({
    search: "ca",
    autofilled: "cake.search",
    completed: "http://cake.search"
  });

  yield cleanup();
});

add_task(function* test_searchEngine_prefix_space_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("CupcakeSearch", "", "", "",
                                       "GET", "http://cupcake.search/");
  let engine = Services.search.getEngineByName("CupcakeSearch");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));

  do_print("Should not try to autoFill search engine if search string contains a space");
  yield check_autocomplete({
    search: " cu",
    autofilled: " cu",
    completed: " cu"
  });

  yield cleanup();
});

add_task(function* test_searchEngine_trailing_space_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("BaconSearch", "", "", "",
                                       "GET", "http://bacon.search/");
  let engine = Services.search.getEngineByName("BaconSearch");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));

  do_print("Should not try to autoFill search engine if search string contains a space");
  yield check_autocomplete({
    search: "ba ",
    autofilled: "ba ",
    completed: "ba "
  });

  yield cleanup();
});

add_task(function* test_searchEngine_www_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("HamSearch", "", "", "",
                                       "GET", "http://ham.search/");
  let engine = Services.search.getEngineByName("HamSearch");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));

  do_print("Should not autoFill search engine if search string contains www. but engine doesn't");
  yield check_autocomplete({
    search: "www.ham",
    autofilled: "www.ham",
    completed: "www.ham"
  });

  yield cleanup();
});

add_task(function* test_searchEngine_different_scheme_noautofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("PieSearch", "", "", "",
                                       "GET", "https://pie.search/");
  let engine = Services.search.getEngineByName("PieSearch");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));

  do_print("Should not autoFill search engine if search string has a different scheme.");
  yield check_autocomplete({
    search: "http://pie",
    autofilled: "http://pie",
    completed: "http://pie"
  });

  yield cleanup();
});

add_task(function* test_searchEngine_matching_prefix_autofill() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", true);
  Services.search.addEngineWithDetails("BeanSearch", "", "", "",
                                       "GET", "http://www.bean.search/");
  let engine = Services.search.getEngineByName("BeanSearch");
  engine.addParam("q", "{searchTerms}", null);
  do_register_cleanup(() => Services.search.removeEngine(engine));


  do_print("Should autoFill search engine if search string has matching prefix.");
  yield check_autocomplete({
    search: "http://www.be",
    autofilled: "http://www.bean.search",
    completed: "http://www.bean.search"
  })

  do_print("Should autoFill search engine if search string has www prefix.");
  yield check_autocomplete({
    search: "www.be",
    autofilled: "www.bean.search",
    completed: "http://www.bean.search"
  });

  do_print("Should autoFill search engine if search string has matching scheme.");
  yield check_autocomplete({
    search: "http://be",
    autofilled: "http://bean.search",
    completed: "http://www.bean.search"
  });

  yield cleanup();
});

add_task(function* test_prefix_autofill() {
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://moz.org/test/"),
    transition: TRANSITION_TYPED
  });

  do_print("Should not try to autoFill in-the-middle if a search is canceled immediately");
  yield check_autocomplete({
    incompleteSearch: "moz",
    search: "mozi",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });

  yield cleanup();
});
