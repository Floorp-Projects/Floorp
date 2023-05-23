const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
const { CookieXPCShellUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CookieXPCShellUtils.sys.mjs"
);

CookieXPCShellUtils.init(this);
CookieXPCShellUtils.createServer({ hosts: ["example.net"] });

add_task(async () => {
  Services.prefs.setBoolPref("dom.security.https_first", false);

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  let uri = NetUtil.newURI("http://example.org/");
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  let set = "foo=bar\nbaz=foo";
  let expected = "foo=bar; baz=foo";
  Services.cookies.setCookieStringFromHttp(uri, set, channel);

  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected);

  await CookieXPCShellUtils.setCookieToDocument("http://example.net/", set);
  actual = await CookieXPCShellUtils.getCookieStringFromDocument(
    "http://example.net/"
  );

  expected = "foo=bar";
  Assert.equal(actual, expected);
  Services.prefs.clearUserPref("dom.security.https_first");
});
