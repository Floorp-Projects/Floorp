/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(function*() {
  do_log_info("visit url, no protocol");
  yield check_autocomplete({
    search: "mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "http://mozilla.org/", input: "mozilla.org"}), title: "http://mozilla.org/" }, ]
  });

  do_log_info("visit url, with protocol");
  yield check_autocomplete({
    search: "https://mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://mozilla.org/", input: "https://mozilla.org"}), title: "https://mozilla.org/" }, ]
  });

  do_log_info("visit url, about: protocol (no host)");
  yield check_autocomplete({
    search: "about:config",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "about:config", input: "about:config"}), title: "about:config" }, ]
  });

  // This is distinct because of how we predict being able to url autofill via
  // host lookups.
  do_log_info("visit url, host matching visited host but not visited url");
  yield promiseAddVisits([
    { uri: NetUtil.newURI("http://mozilla.org/wine/"), title: "Mozilla Wine", transition: TRANSITION_TYPED },
  ]);
  yield check_autocomplete({
    search: "mozilla.org/rum",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "http://mozilla.org/rum", input: "mozilla.org/rum"}), title: "http://mozilla.org/rum" }, ]
  });

  // And hosts with no dot in them are special, due to requiring whitelisting.
  do_log_info("visit url, host matching visited host but not visited url, non-whitelisted host");
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://s.example.com/search");
  let engine = Services.search.getEngineByName("MozSearch");
  Services.search.currentEngine = engine;
  yield promiseAddVisits([
    { uri: NetUtil.newURI("http://mozilla/bourbon/"), title: "Mozilla Bourbon", transition: TRANSITION_TYPED },
  ]);
  yield check_autocomplete({
    search: "mozilla/rum",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "mozilla/rum", searchQuery: "mozilla/rum"}), title: "MozSearch" }, ]
  });
});
