"use strict";

function makeURI(str) {
  return Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService)
    .newURI(str);
}

add_task(async () => {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  var serv = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var uri = makeURI("http://example.com/");
  var channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });
  Services.scriptSecurityManager.createContentPrincipal(uri, {});

  CookieXPCShellUtils.createServer({ hosts: ["example.com"] });

  // Try an expiration time before the epoch

  await CookieXPCShellUtils.setCookieToDocument(
    uri.spec,
    "test=test; path=/; domain=example.com; expires=Sun, 31-Dec-1899 16:00:00 GMT;"
  );
  Assert.equal(
    await CookieXPCShellUtils.getCookieStringFromDocument(uri.spec),
    ""
  );

  // Now sanity check
  serv.setCookieStringFromHttp(
    uri,
    "test2=test2; path=/; domain=example.com;",
    channel
  );

  Assert.equal(
    await CookieXPCShellUtils.getCookieStringFromDocument(uri.spec),
    "test2=test2"
  );
});
