const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let set = "foo=bar";
  cs.setCookieStringFromHttp(uri, null, null, set, null, null);

  let expected = "foo=bar";
  let actual = cs.getCookieStringFromHttp(uri, null, null);
  do_check_eq(actual, expected);
}

