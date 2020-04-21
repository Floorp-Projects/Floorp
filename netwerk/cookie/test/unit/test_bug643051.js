const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  let set = "foo=bar\nbaz=foo";
  let expected = "foo=bar; baz=foo";
  cs.setCookieStringFromHttp(uri, set, null);

  let actual = cs.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected);

  uri = NetUtil.newURI("http://example.com/");
  cs.setCookieString(uri, set, null);

  expected = "foo=bar";
  actual = cs.getCookieStringForPrincipal(
    Services.scriptSecurityManager.createContentPrincipal(uri, {})
  );
  Assert.equal(actual, expected);
}
