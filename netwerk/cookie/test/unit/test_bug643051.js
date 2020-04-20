const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let set = "foo=bar\nbaz=foo";
  let expected = "foo=bar; baz=foo";
  cs.setCookieStringFromHttp(uri, null, set, null, null);

  let actual = cs.getCookieStringFromHttp(uri, null, null);
  Assert.equal(actual, expected);

  uri = NetUtil.newURI("http://example.com/");
  cs.setCookieString(uri, set, null);

  expected = "foo=bar";
  actual = cs.getCookieStringForPrincipal(
    Services.scriptSecurityManager.createContentPrincipal(uri, {})
  );
  Assert.equal(actual, expected);
}
