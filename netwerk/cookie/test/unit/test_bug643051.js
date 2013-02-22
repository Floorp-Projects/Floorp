const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let set = "foo=bar\nbaz=foo";
  let expected = "foo=bar; baz=foo";
  cs.setCookieStringFromHttp(uri, null, null, set, null, null);

  let actual = cs.getCookieStringFromHttp(uri, null, null);
  do_check_eq(actual, expected);

  uri = NetUtil.newURI("http://example.com/");
  cs.setCookieString(uri, null, set, null);

  expected = "foo=bar";
  actual = cs.getCookieString(uri, null, null);
  do_check_eq(actual, expected);
}

