"use strict";

let httpsOrigin;
let h3AltSvc;
let h3Route;
let prefs;

let tests = [test_https_alt_svc, test_https_alt_svc_1, testsDone];

let current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    dump("starting test number " + current_test + "\n");
    tests[current_test]();
    current_test++;
  }
}

function run_test() {
  let h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");
  let h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  h3AltSvc = ":" + h3Port;

  h3Route = "foo.example.com:" + h3Port;
  do_get_profile();
  prefs = Services.prefs;

  prefs.setBoolPref("network.http.http3.enable", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  // We always resolve elements of localDomains as it's hardcoded without the
  // following pref:
  prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  // The certificate for the http3server server is for foo.example.com and
  // is signed by http2-ca.pem so add that cert to the trust list as a
  // signing cert.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  httpsOrigin = "https://foo.example.com:" + h2Port + "/";

  run_next_test();
}

function createPrincipal(url) {
  var ssm = Services.scriptSecurityManager;
  try {
    return ssm.createContentPrincipal(Services.io.newURI(url), {});
  } catch (e) {
    return null;
  }
}

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadingPrincipal: createPrincipal(uri),
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

let WaitForHttp3Listener = function (expectedH3Version) {
  this._expectedH3Version = expectedH3Version;
};

WaitForHttp3Listener.prototype = {
  onDataAvailableFired: false,
  expectedRoute: "",

  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);
    Assert.equal(request.responseStatus, 200);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    this.onDataAvailableFired = true;
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request) {
    let routed = "NA";
    try {
      routed = request.getRequestHeader("Alt-Used");
    } catch (e) {}
    dump("routed is " + routed + "\n");

    if (routed == this.expectedRoute) {
      let httpVersion = "";
      try {
        httpVersion = request.protocolVersion;
      } catch (e) {}
      Assert.equal(httpVersion, this._expectedH3Version);
      run_next_test();
    } else {
      dump("poll later for alt svc mapping\n");
      do_test_pending();
      do_timeout(500, () => {
        doTest(
          this.uri,
          this.expectedRoute,
          this.h3AltSvc,
          this._expectedH3Version
        );
      });
    }

    do_test_finished();
  },
};

function doTest(uri, expectedRoute, altSvc, expectedH3Version) {
  let chan = makeChan(uri);
  let listener = new WaitForHttp3Listener(expectedH3Version);
  listener.uri = uri;
  listener.expectedRoute = expectedRoute;
  listener.h3AltSvc = altSvc;
  chan.setRequestHeader("x-altsvc", altSvc, false);
  chan.asyncOpen(listener);
}

// Test Alt-Svc for http3.
// H2 server returns alt-svc=h2=foo2.example.com:8000,h3-29=:h3port
function test_https_alt_svc() {
  dump("test_https_alt_svc()\n");
  do_test_pending();
  doTest(httpsOrigin + "http3-test2", h3Route, h3AltSvc, "h3-29");
}

// Test if we use the latest version of HTTP/3.
// H2 server returns alt-svc=h3-29=:h3port,h3=:h3port
function test_https_alt_svc_1() {
  // Close the previous connection and clear alt-svc mappings.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  do_test_pending();
  do_timeout(1000, () => {
    doTest(httpsOrigin + "http3-test3", h3Route, h3AltSvc, "h3");
  });
}

function testsDone() {
  prefs.clearUserPref("network.http.http3.enable");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
  dump("testDone\n");
}
