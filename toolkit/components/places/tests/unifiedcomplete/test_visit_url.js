add_task(function*() {
  do_print("visit url, no protocol");
  yield check_autocomplete({
    search: "mozilla.org",
    searchParam: "enable-actions",
    matches: [
      { uri: makeActionURI("visiturl", {url: "http://mozilla.org/", input: "mozilla.org"}), title: "http://mozilla.org/", style: [ "action", "visiturl", "heuristic" ] },
      { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "mozilla.org", searchQuery: "mozilla.org"}), title: "MozSearch", style: ["action", "searchengine"] }
    ]
  });

  do_print("visit url, no protocol but with 2 dots");
  yield check_autocomplete({
    search: "www.mozilla.org",
    searchParam: "enable-actions",
    matches: [
      { uri: makeActionURI("visiturl", {url: "http://www.mozilla.org/", input: "www.mozilla.org"}), title: "http://www.mozilla.org/", style: [ "action", "visiturl", "heuristic" ] },
      { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "www.mozilla.org", searchQuery: "www.mozilla.org"}), title: "MozSearch", style: ["action", "searchengine"] }
    ]
  });

  do_print("visit url, no protocol but with 3 dots");
  yield check_autocomplete({
    search: "www.mozilla.org.tw",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "http://www.mozilla.org.tw/", input: "www.mozilla.org.tw"}), title: "http://www.mozilla.org.tw/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  do_print("visit url, with protocol but with 2 dots");
  yield check_autocomplete({
    search: "https://www.mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://www.mozilla.org/", input: "https://www.mozilla.org"}), title: "https://www.mozilla.org/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  do_print("visit url, with protocol but with 3 dots");
  yield check_autocomplete({
    search: "https://www.mozilla.org.tw",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://www.mozilla.org.tw/", input: "https://www.mozilla.org.tw"}), title: "https://www.mozilla.org.tw/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  do_print("visit url, with protocol");
  yield check_autocomplete({
    search: "https://mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://mozilla.org/", input: "https://mozilla.org"}), title: "https://mozilla.org/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  do_print("visit url, about: protocol (no host)");
  yield check_autocomplete({
    search: "about:config",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "about:config", input: "about:config"}), title: "about:config", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  // This is distinct because of how we predict being able to url autofill via
  // host lookups.
  do_print("visit url, host matching visited host but not visited url");
  yield PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("http://mozilla.org/wine/"), title: "Mozilla Wine", transition: TRANSITION_TYPED },
  ]);
  yield check_autocomplete({
    search: "mozilla.org/rum",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("mozilla.org/rum", "http://mozilla.org/rum", { heuristic: true }) ]
  });

  // And hosts with no dot in them are special, due to requiring whitelisting.
  do_print("non-whitelisted host");
  yield check_autocomplete({
    search: "firefox",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("firefox", { heuristic: true }) ]
  });

  do_print("url with non-whitelisted host");
  yield check_autocomplete({
    search: "firefox/get",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("firefox/get", "http://firefox/get", { heuristic: true }) ]
  });

  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.firefox", true);
  do_register_cleanup(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.firefox");
  });

  do_print("whitelisted host");
  yield check_autocomplete({
    search: "firefox",
    searchParam: "enable-actions",
    matches: [
      makeVisitMatch("firefox", "http://firefox/", { heuristic: true }),
      makeSearchMatch("firefox", { heuristic: false })
    ]
  });

  do_print("url with whitelisted host");
  yield check_autocomplete({
    search: "firefox/get",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("firefox/get", "http://firefox/get", { heuristic: true }) ]
  });

  do_print("visit url, host matching visited host but not visited url, whitelisted host");
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.mozilla", true);
  do_register_cleanup(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.mozilla");
  });
  yield check_autocomplete({
    search: "mozilla/rum",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("mozilla/rum", "http://mozilla/rum", { heuristic: true }) ]
  });

  // ipv4 and ipv6 literal addresses should offer to visit.
  do_print("visit url, ipv4 literal");
  yield check_autocomplete({
    search: "127.0.0.1",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("127.0.0.1", "http://127.0.0.1/", { heuristic: true }) ]
  });

  do_print("visit url, ipv6 literal");
  yield check_autocomplete({
    search: "[2001:db8::1]",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("[2001:db8::1]", "http://[2001:db8::1]/", { heuristic: true }) ]
  });

  // Setting keyword.enabled to false should always try to visit.
  let keywordEnabled = Services.prefs.getBoolPref("keyword.enabled");
  Services.prefs.setBoolPref("keyword.enabled", false);
  do_register_cleanup(() => {
    Services.prefs.clearUserPref("keyword.enabled");
  });
  do_print("visit url, keyword.enabled = false");
  yield check_autocomplete({
    search: "bacon",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("bacon", "http://bacon/", { heuristic: true }) ]
  });
  do_print("visit two word query, keyword.enabled = false");
  yield check_autocomplete({
    search: "bacon lovers",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("bacon lovers", "bacon lovers",  { heuristic: true }) ]
  });
  Services.prefs.setBoolPref("keyword.enabled", keywordEnabled);

  do_print("visit url, scheme+host");
  yield check_autocomplete({
    search: "http://example",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("http://example", "http://example/", { heuristic: true }) ]
  });

  do_print("visit url, scheme+host");
  yield check_autocomplete({
    search: "ftp://example",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("ftp://example", "ftp://example/", { heuristic: true }) ]
  });

  do_print("visit url, host+port");
  yield check_autocomplete({
    search: "example:8080",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("example:8080", "http://example:8080/", { heuristic: true }) ]
  });

  do_print("numerical operations that look like urls should search");
  yield check_autocomplete({
    search: "123/12",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("123/12", { heuristic: true }) ]
  });

  do_print("numerical operations that look like urls should search");
  yield check_autocomplete({
    search: "123.12/12.1",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("123.12/12.1", { heuristic: true }) ]
  });
});
