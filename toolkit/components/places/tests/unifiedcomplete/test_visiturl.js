/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


add_task(function*() {
  do_print("visit url, no protocol");
  yield check_autocomplete({
    search: "mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "http://mozilla.org/", input: "mozilla.org"}), title: "http://mozilla.org/", style: [ "action", "visiturl" ] } ]
  });

  do_print("visit url, with protocol");
  yield check_autocomplete({
    search: "https://mozilla.org",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "https://mozilla.org/", input: "https://mozilla.org"}), title: "https://mozilla.org/", style: [ "action", "visiturl" ] } ]
  });

  do_print("visit url, about: protocol (no host)");
  yield check_autocomplete({
    search: "about:config",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("visiturl", {url: "about:config", input: "about:config"}), title: "about:config", style: [ "action", "visiturl" ] } ]
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
    matches: [ makeVisitMatch("mozilla.org/rum", "http://mozilla.org/rum") ]
  });

  // And hosts with no dot in them are special, due to requiring whitelisting.
  do_print("visit url, host matching visited host but not visited url, non-whitelisted host");
  yield PlacesTestUtils.addVisits([
    { uri: NetUtil.newURI("http://mozilla/bourbon/"), title: "Mozilla Bourbon", transition: TRANSITION_TYPED },
  ]);
  yield check_autocomplete({
    search: "mozilla/rum",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("mozilla/rum") ]
  });

  // ipv4 and ipv6 literal addresses should offer to visit.
  do_print("visit url, ipv4 literal");
  yield check_autocomplete({
    search: "127.0.0.1",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("127.0.0.1", "http://127.0.0.1/") ]
  });

  do_print("visit url, ipv6 literal");
  yield check_autocomplete({
    search: "[2001:db8::1]",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("[2001:db8::1]", "http://[2001:db8::1]/") ]
  });

  // Setting keyword.enabled to false should always try to visit.
  Services.prefs.setBoolPref("keyword.enabled", false);
  do_register_cleanup(() => {
    Services.prefs.clearUserPref("keyword.enabled");
  });
  do_print("visit url, keyword.enabled = false");
  yield check_autocomplete({
    search: "bacon",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("bacon", "http://bacon/") ]
  });
});
