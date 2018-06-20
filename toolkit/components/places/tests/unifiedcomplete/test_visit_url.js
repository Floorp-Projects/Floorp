add_task(async function() {
  info("visit url, no protocol");
  await check_autocomplete({
    search: "mozilla.org",
    searchParam: "enable-actions",
    matches: [
      { uri: makeActionURI("visiturl", {url: "http://mozilla.org/", input: "mozilla.org"}), title: "http://mozilla.org/", style: [ "action", "visiturl", "heuristic" ] },
      { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "mozilla.org", searchQuery: "mozilla.org"}), title: "MozSearch", style: ["action", "searchengine"] }
    ]
  });

  info("visit url, no protocol but with 2 dots");
  await check_autocomplete({
    search: "www.mozilla.org",
    searchParam: "enable-actions",
    matches: [
      { uri: makeActionURI("visiturl", {url: "http://www.mozilla.org/", input: "www.mozilla.org"}), title: "http://www.mozilla.org/", style: [ "action", "visiturl", "heuristic" ] },
      { uri: makeActionURI("searchengine", {engineName: "MozSearch", input: "www.mozilla.org", searchQuery: "www.mozilla.org"}), title: "MozSearch", style: ["action", "searchengine"] }
    ]
  });

  info("visit url, no protocol but with 3 dots");
  await check_autocomplete({
    search: "www.mozilla.org.tw",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "http://www.mozilla.org.tw/", input: "www.mozilla.org.tw"}), title: "http://www.mozilla.org.tw/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  info("visit url, with protocol but with 2 dots");
  await check_autocomplete({
    search: "https://www.mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://www.mozilla.org/", input: "https://www.mozilla.org"}), title: "https://www.mozilla.org/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  info("visit url, with protocol but with 3 dots");
  await check_autocomplete({
    search: "https://www.mozilla.org.tw",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://www.mozilla.org.tw/", input: "https://www.mozilla.org.tw"}), title: "https://www.mozilla.org.tw/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  info("visit url, with protocol");
  await check_autocomplete({
    search: "https://mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://mozilla.org/", input: "https://mozilla.org"}), title: "https://mozilla.org/", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  info("visit url, about: protocol (no host)");
  await check_autocomplete({
    search: "about:config",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "about:config", input: "about:config"}), title: "about:config", style: [ "action", "visiturl", "heuristic" ] } ]
  });

  info("visit url, with non-standard whitespace");
  await check_autocomplete({
    search: "https://www.mozilla.org\u2028",
    searchParam: "enable-actions",
    matches: [{
      uri: makeActionURI("visiturl", {url: "https://www.mozilla.org/",
                                      input: "https://www.mozilla.org"}),
      title: "https://www.mozilla.org/",
      style: [ "action", "visiturl", "heuristic" ]}]
  });

  // This is distinct because of how we predict being able to url autofill via
  // host lookups.
  info("visit url, host matching visited host but not visited url");
  await PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("http://mozilla.org/wine/"), title: "Mozilla Wine", transition: TRANSITION_TYPED },
  ]);
  await check_autocomplete({
    search: "mozilla.org/rum",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("mozilla.org/rum", "http://mozilla.org/rum", { heuristic: true }) ]
  });

  // And hosts with no dot in them are special, due to requiring whitelisting.
  info("non-whitelisted host");
  await check_autocomplete({
    search: "firefox",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("firefox", { heuristic: true }) ]
  });

  info("url with non-whitelisted host");
  await check_autocomplete({
    search: "firefox/get",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("firefox/get", "http://firefox/get", { heuristic: true }) ]
  });

  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.firefox", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.firefox");
  });

  info("whitelisted host");
  await check_autocomplete({
    search: "firefox",
    searchParam: "enable-actions",
    matches: [
      makeVisitMatch("firefox", "http://firefox/", { heuristic: true }),
      makeSearchMatch("firefox", { heuristic: false })
    ]
  });

  info("url with whitelisted host");
  await check_autocomplete({
    search: "firefox/get",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("firefox/get", "http://firefox/get", { heuristic: true }) ]
  });

  info("visit url, host matching visited host but not visited url, whitelisted host");
  Services.prefs.setBoolPref("browser.fixup.domainwhitelist.mozilla", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.fixup.domainwhitelist.mozilla");
  });
  await check_autocomplete({
    search: "mozilla/rum",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("mozilla/rum", "http://mozilla/rum", { heuristic: true }) ]
  });

  // ipv4 and ipv6 literal addresses should offer to visit.
  info("visit url, ipv4 literal");
  await check_autocomplete({
    search: "127.0.0.1",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("127.0.0.1", "http://127.0.0.1/", { heuristic: true }) ]
  });

  info("visit url, ipv6 literal");
  await check_autocomplete({
    search: "[2001:db8::1]",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("[2001:db8::1]", "http://[2001:db8::1]/", { heuristic: true }) ]
  });

  // Setting keyword.enabled to false should always try to visit.
  let keywordEnabled = Services.prefs.getBoolPref("keyword.enabled");
  Services.prefs.setBoolPref("keyword.enabled", false);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("keyword.enabled");
  });
  info("visit url, keyword.enabled = false");
  await check_autocomplete({
    search: "bacon",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("bacon", "http://bacon/", { heuristic: true }) ]
  });
  info("visit two word query, keyword.enabled = false");
  await check_autocomplete({
    search: "bacon lovers",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("bacon lovers", "bacon lovers", { heuristic: true }) ]
  });
  Services.prefs.setBoolPref("keyword.enabled", keywordEnabled);

  info("visit url, scheme+host");
  await check_autocomplete({
    search: "http://example",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("http://example", "http://example/", { heuristic: true }) ]
  });

  info("visit url, scheme+host");
  await check_autocomplete({
    search: "ftp://example",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("ftp://example", "ftp://example/", { heuristic: true }) ]
  });

  info("visit url, host+port");
  await check_autocomplete({
    search: "example:8080",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("example:8080", "http://example:8080/", { heuristic: true }) ]
  });

  info("numerical operations that look like urls should search");
  await check_autocomplete({
    search: "123/12",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("123/12", { heuristic: true }) ]
  });

  info("numerical operations that look like urls should search");
  await check_autocomplete({
    search: "123.12/12.1",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("123.12/12.1", { heuristic: true }) ]
  });

  info("access resource:///modules");
  await check_autocomplete({
      search: "resource:///modules",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("resource:///modules", "resource:///modules", { heuristic: true }) ]
  });

  info("access resource://app/modules");
  await check_autocomplete({
      search: "resource://app/modules",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("resource://app/modules", "resource://app/modules", { heuristic: true }) ]
  });
});
