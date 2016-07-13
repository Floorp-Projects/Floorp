Cu.import("resource://gre/modules/NetUtil.jsm");

function getTestReferrer(server_uri, referer_uri) {
  var uri = NetUtil.newURI(server_uri, "", null)
  var chan = NetUtil.newChannel({
    uri: uri,
    loadUsingSystemPrincipal: true
  });

  chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  chan.referrer = NetUtil.newURI(referer_uri, null, null);
  var header = null;
  try {
    header = chan.getRequestHeader("Referer");
  }
  catch (NS_ERROR_NOT_AVAILABLE) {}
  return header;
}

function run_test() {
  var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);

  var server_uri = "http://bar.examplesite.com/path2";
  var server_uri_2 = "http://bar.example.com/anotherpath";
  var referer_uri = "http://foo.example.com/path";
  var referer_uri_2 = "http://bar.examplesite.com/path3?q=blah";
  var referer_uri_2_anchor = "http://bar.examplesite.com/path3?q=blah#anchor";
  var referer_uri_idn = "http://sub1.\xe4lt.example/path";

  // for https tests
  var server_uri_https = "https://bar.example.com/anotherpath";
  var referer_uri_https = "https://bar.example.com/path3?q=blah";

  // tests for sendRefererHeader
  prefs.setIntPref("network.http.sendRefererHeader", 0);
  do_check_null(getTestReferrer(server_uri, referer_uri));
  prefs.setIntPref("network.http.sendRefererHeader", 2);
  do_check_eq(getTestReferrer(server_uri, referer_uri), referer_uri);

  // test that https ref is not sent to http
  do_check_null(getTestReferrer(server_uri_2, referer_uri_https));

  // tests for referer.spoofSource
  prefs.setBoolPref("network.http.referer.spoofSource", true);
  do_check_eq(getTestReferrer(server_uri, referer_uri), server_uri);
  prefs.setBoolPref("network.http.referer.spoofSource", false);
  do_check_eq(getTestReferrer(server_uri, referer_uri), referer_uri);

  // tests for referer.XOriginPolicy
  prefs.setIntPref("network.http.referer.XOriginPolicy", 2);
  do_check_null(getTestReferrer(server_uri_2, referer_uri));
  do_check_eq(getTestReferrer(server_uri, referer_uri_2), referer_uri_2);
  prefs.setIntPref("network.http.referer.XOriginPolicy", 1);
  do_check_eq(getTestReferrer(server_uri_2, referer_uri), referer_uri);
  do_check_null(getTestReferrer(server_uri, referer_uri));
  // https test
  do_check_eq(getTestReferrer(server_uri_https, referer_uri_https), referer_uri_https);
  prefs.setIntPref("network.http.referer.XOriginPolicy", 0);
  do_check_eq(getTestReferrer(server_uri, referer_uri), referer_uri);

  // tests for referer.trimmingPolicy
  prefs.setIntPref("network.http.referer.trimmingPolicy", 1);
  do_check_eq(getTestReferrer(server_uri, referer_uri_2), "http://bar.examplesite.com/path3");
  do_check_eq(getTestReferrer(server_uri, referer_uri_idn), "http://sub1.xn--lt-uia.example/path");
  prefs.setIntPref("network.http.referer.trimmingPolicy", 2);
  do_check_eq(getTestReferrer(server_uri, referer_uri_2), "http://bar.examplesite.com/");
  do_check_eq(getTestReferrer(server_uri, referer_uri_idn), "http://sub1.xn--lt-uia.example/");
  // https test
  do_check_eq(getTestReferrer(server_uri_https, referer_uri_https), "https://bar.example.com/");
  prefs.setIntPref("network.http.referer.trimmingPolicy", 0);
  // test that anchor is lopped off in ordinary case
  do_check_eq(getTestReferrer(server_uri, referer_uri_2_anchor), referer_uri_2);

  // combination test: send spoofed path-only when hosts match
  var combo_referer_uri = "http://blah.foo.com/path?q=hot";
  var dest_uri = "http://blah.foo.com:9999/spoofedpath?q=bad";
  prefs.setIntPref("network.http.referer.trimmingPolicy", 1);
  prefs.setBoolPref("network.http.referer.spoofSource", true);
  prefs.setIntPref("network.http.referer.XOriginPolicy", 2);
  do_check_eq(getTestReferrer(dest_uri, combo_referer_uri), "http://blah.foo.com:9999/spoofedpath");
  do_check_null(getTestReferrer(dest_uri, "http://gah.foo.com/anotherpath"));
}
