const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let set = "foo=b;max-age=3600, c=d;path=/";
  cs.setCookieStringFromHttp(uri, null, null, set, null, null);

  let expected = "foo=b";
  let actual = cs.getCookieStringFromHttp(uri, null, null);
  do_check_eq(actual, expected);
}

