"use strict";

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

function getTestReferrer(server_uri, referer_uri, isPrivate = false) {
  var uri = NetUtil.newURI(server_uri);
  let referrer = NetUtil.newURI(referer_uri);
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    referrer,
    { privateBrowsingId: isPrivate ? 1 : 0 }
  );
  var chan = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
  });

  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.EMPTY,
    true,
    referrer
  );
  var header = null;
  try {
    header = chan.getRequestHeader("Referer");
  } catch (NS_ERROR_NOT_AVAILABLE) {}
  return header;
}

function run_test() {
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );

  var server_uri = "http://bar.examplesite.com/path2";
  var server_uri_2 = "http://bar.example.com/anotherpath";
  var referer_uri = "http://foo.example.com/path";
  var referer_uri_2 = "http://bar.examplesite.com/path3?q=blah";
  var referer_uri_2_anchor = "http://bar.examplesite.com/path3?q=blah#anchor";
  var referer_uri_idn = "http://sub1.\xe4lt.example/path";

  // for https tests
  var server_uri_https = "https://bar.example.com/anotherpath";
  var referer_uri_https = "https://bar.example.com/path3?q=blah";
  var referer_uri_2_https = "https://bar.examplesite.com/path3?q=blah";

  // tests for sendRefererHeader
  prefs.setIntPref("network.http.sendRefererHeader", 0);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri));
  prefs.setIntPref("network.http.sendRefererHeader", 2);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);

  // test that https ref is not sent to http
  Assert.equal(null, getTestReferrer(server_uri_2, referer_uri_https));

  // tests for referer.defaultPolicy
  prefs.setIntPref("network.http.referer.defaultPolicy", 0);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri));
  prefs.setIntPref("network.http.referer.defaultPolicy", 1);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri));
  Assert.equal(getTestReferrer(server_uri, referer_uri_2), referer_uri_2);
  prefs.setIntPref("network.http.referer.defaultPolicy", 2);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri_https));
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_https),
    referer_uri_https
  );
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_2_https),
    "https://bar.examplesite.com/"
  );
  Assert.equal(getTestReferrer(server_uri, referer_uri_2), referer_uri_2);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri),
    "http://foo.example.com/"
  );
  prefs.setIntPref("network.http.referer.defaultPolicy", 3);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);
  Assert.equal(null, getTestReferrer(server_uri_2, referer_uri_https));

  // tests for referer.defaultPolicy.pbmode
  prefs.setIntPref("network.http.referer.defaultPolicy.pbmode", 0);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri, true));
  prefs.setIntPref("network.http.referer.defaultPolicy.pbmode", 1);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri, true));
  Assert.equal(getTestReferrer(server_uri, referer_uri_2, true), referer_uri_2);
  prefs.setIntPref("network.http.referer.defaultPolicy.pbmode", 2);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri_https, true));
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_https, true),
    referer_uri_https
  );
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_2_https, true),
    "https://bar.examplesite.com/"
  );
  Assert.equal(getTestReferrer(server_uri, referer_uri_2, true), referer_uri_2);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri, true),
    "http://foo.example.com/"
  );
  prefs.setIntPref("network.http.referer.defaultPolicy.pbmode", 3);
  Assert.equal(getTestReferrer(server_uri, referer_uri, true), referer_uri);
  Assert.equal(null, getTestReferrer(server_uri_2, referer_uri_https, true));

  // tests for referer.spoofSource
  prefs.setBoolPref("network.http.referer.spoofSource", true);
  Assert.equal(getTestReferrer(server_uri, referer_uri), server_uri);
  prefs.setBoolPref("network.http.referer.spoofSource", false);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);

  // tests for referer.XOriginPolicy
  prefs.setIntPref("network.http.referer.XOriginPolicy", 2);
  Assert.equal(null, getTestReferrer(server_uri_2, referer_uri));
  Assert.equal(getTestReferrer(server_uri, referer_uri_2), referer_uri_2);
  prefs.setIntPref("network.http.referer.XOriginPolicy", 1);
  Assert.equal(getTestReferrer(server_uri_2, referer_uri), referer_uri);
  Assert.equal(null, getTestReferrer(server_uri, referer_uri));
  // https test
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_https),
    referer_uri_https
  );
  prefs.setIntPref("network.http.referer.XOriginPolicy", 0);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);

  // tests for referer.trimmingPolicy
  prefs.setIntPref("network.http.referer.trimmingPolicy", 1);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2),
    "http://bar.examplesite.com/path3"
  );
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_idn),
    "http://sub1.xn--lt-uia.example/path"
  );
  prefs.setIntPref("network.http.referer.trimmingPolicy", 2);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2),
    "http://bar.examplesite.com/"
  );
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_idn),
    "http://sub1.xn--lt-uia.example/"
  );
  // https test
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_https),
    "https://bar.example.com/"
  );
  prefs.setIntPref("network.http.referer.trimmingPolicy", 0);
  // test that anchor is lopped off in ordinary case
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2_anchor),
    referer_uri_2
  );

  // tests for referer.XOriginTrimmingPolicy
  prefs.setIntPref("network.http.referer.XOriginTrimmingPolicy", 1);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri),
    "http://foo.example.com/path"
  );
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_idn),
    "http://sub1.xn--lt-uia.example/path"
  );
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2),
    "http://bar.examplesite.com/path3?q=blah"
  );
  prefs.setIntPref("network.http.referer.trimmingPolicy", 1);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2),
    "http://bar.examplesite.com/path3"
  );
  prefs.setIntPref("network.http.referer.XOriginTrimmingPolicy", 2);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri),
    "http://foo.example.com/"
  );
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_idn),
    "http://sub1.xn--lt-uia.example/"
  );
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2),
    "http://bar.examplesite.com/path3"
  );
  prefs.setIntPref("network.http.referer.trimmingPolicy", 0);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2),
    "http://bar.examplesite.com/path3?q=blah"
  );
  // https tests
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_https),
    "https://bar.example.com/path3?q=blah"
  );
  Assert.equal(
    getTestReferrer(server_uri_https, referer_uri_2_https),
    "https://bar.examplesite.com/"
  );
  prefs.setIntPref("network.http.referer.XOriginTrimmingPolicy", 0);
  // test that anchor is lopped off in ordinary case
  Assert.equal(
    getTestReferrer(server_uri, referer_uri_2_anchor),
    referer_uri_2
  );

  // test referrer length limitation
  // referer_uri's length is 27 and origin's length is 23
  prefs.setIntPref("network.http.referer.referrerLengthLimit", 27);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);
  prefs.setIntPref("network.http.referer.referrerLengthLimit", 26);
  Assert.equal(
    getTestReferrer(server_uri, referer_uri),
    "http://foo.example.com/"
  );
  prefs.setIntPref("network.http.referer.referrerLengthLimit", 22);
  Assert.equal(getTestReferrer(server_uri, referer_uri), null);
  prefs.setIntPref("network.http.referer.referrerLengthLimit", 0);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);
  prefs.setIntPref("network.http.referer.referrerLengthLimit", 4096);
  Assert.equal(getTestReferrer(server_uri, referer_uri), referer_uri);

  // combination test: send spoofed path-only when hosts match
  var combo_referer_uri = "http://blah.foo.com/path?q=hot";
  var dest_uri = "http://blah.foo.com:9999/spoofedpath?q=bad";
  prefs.setIntPref("network.http.referer.trimmingPolicy", 1);
  prefs.setBoolPref("network.http.referer.spoofSource", true);
  prefs.setIntPref("network.http.referer.XOriginPolicy", 2);
  Assert.equal(
    getTestReferrer(dest_uri, combo_referer_uri),
    "http://blah.foo.com:9999/spoofedpath"
  );
  Assert.equal(
    null,
    getTestReferrer(dest_uri, "http://gah.foo.com/anotherpath")
  );
}
