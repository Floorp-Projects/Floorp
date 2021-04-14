"use strict";

let httpsOrigin;
let h3AltSvc;
let h3Port;
let h3Route;
let prefs;

let tests = [test_https_alt_svc, testsDone];

let current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    dump("starting test number " + current_test + "\n");
    tests[current_test]();
    current_test++;
  }
}

function run_test() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");
  let h3Port = env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  h3AltSvc = ":" + h3Port;

  h3Route = "foo.example.com:" + h3Port;
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("network.http.http3.enabled", true);
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

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

let WaitForHttp3Listener = function() {};

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

  onStopRequest: function testOnStopRequest(request, status) {
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
      Assert.equal(httpVersion, "h3-27");
      run_next_test();
    } else {
      dump("poll later for alt svc mapping\n");
      do_test_pending();
      do_timeout(500, () => {
        doTest(this.uri, this.expectedRoute, this.h3AltSvc);
      });
    }

    do_test_finished();
  },
};

function doTest(uri, expectedRoute, altSvc) {
  let chan = makeChan(uri);
  let listener = new WaitForHttp3Listener();
  listener.uri = uri;
  listener.expectedRoute = expectedRoute;
  listener.h3AltSvc = altSvc;
  chan.setRequestHeader("x-altsvc", altSvc, false);
  chan.asyncOpen(listener);
}

// Test Alt-Svc for http3.
// H2 server returns alt-svc=h2=foo2.example.com:8000,h3-27=:h3port,h3-29=foo2.example.com:8443
function test_https_alt_svc() {
  dump("test_https_alt_svc()\n");
  do_test_pending();
  doTest(httpsOrigin + "http3-test2", h3Route, h3AltSvc);
}

function testsDone() {
  prefs.clearUserPref("network.http.http3.enabled");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.proxy.allow_hijacking_localhost");
  dump("testDone\n");
}
