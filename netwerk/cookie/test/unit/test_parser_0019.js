const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref(
      "network.cookieJarSettings.unblocked_for_testing",
      true
    );
  }

  let uri = NetUtil.newURI("http://example.org/");
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  let set = "foo=b;max-age=3600, c=d;path=/";
  Services.cookies.setCookieStringFromHttp(uri, set, channel);

  let expected = "foo=b";
  let actual = Services.cookies.getCookieStringFromHttp(uri, channel);
  Assert.equal(actual, expected);
}
