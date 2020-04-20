const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  }

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let set = "foo=bar";
  cs.setCookieStringFromHttp(uri, null, set, null);

  let expected = "foo=bar";
  let actual = cs.getCookieStringFromHttp(uri, null, null);
  Assert.equal(actual, expected);
}
