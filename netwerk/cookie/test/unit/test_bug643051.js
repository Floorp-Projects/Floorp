const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { CookieXPCShellUtils } = ChromeUtils.import(
  "resource://testing-common/CookieXPCShellUtils.jsm"
);

CookieXPCShellUtils.init(this);
CookieXPCShellUtils.createServer({ hosts: ["example.net"] });

add_task(async () => {
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
  cs.setCookieStringFromHttp(uri, set, channel);

  let actual = cs.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected);

  uri = NetUtil.newURI("http://example.net/");

  const contentPage = await CookieXPCShellUtils.loadContentPage(uri.spec);
  // eslint-disable-next-line no-undef
  await contentPage.spawn(set, cookie => (content.document.cookie = cookie));
  await contentPage.close();

  expected = "foo=bar";
  actual = cs.getCookieStringForPrincipal(
    Services.scriptSecurityManager.createContentPrincipal(uri, {})
  );
  Assert.equal(actual, expected);
});
