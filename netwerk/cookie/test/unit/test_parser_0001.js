var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/NetUtil.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function inChildProcess() {
  return Cc["@mozilla.org/xre/app-info;1"]
           .getService(Ci.nsIXULRuntime)
           .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess())
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let set = "foo=bar";
  cs.setCookieStringFromHttp(uri, null, null, set, null, null);

  let expected = "foo=bar";
  let actual = cs.getCookieStringFromHttp(uri, null, null);
  do_check_eq(actual, expected);
}

