"use strict";

let h3Port;
let h3Route;
let h3AltSvc;
let prefs;
let httpsOrigin;

let tests = [
  // The altSvc storage may not be up imediately, therefore run test_no_altsvc_pref
  // for a couple times to wait for the storage.
  test_no_altsvc_pref,
  test_no_altsvc_pref,
  test_no_altsvc_pref,
  test_altsvc_pref,
  testsDone,
];

let current_test = 0;

function run_next_test() {
  if (current_test < tests.length) {
    dump("starting test number " + current_test + "\n");
    tests[current_test]();
    current_test++;
  }
}

function run_test() {
  h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  h3AltSvc = ":" + h3Port;

  h3Route = "foo.example.com:" + h3Port;
  do_get_profile();
  prefs = Services.prefs;

  prefs.setBoolPref("network.http.http3.enable", true);
  prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  prefs.setBoolPref("network.dns.disableIPv6", true);

  // The certificate for the http3server server is for foo.example.com and
  // is signed by http2-ca.pem so add that cert to the trust list as a
  // signing cert.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  httpsOrigin = "https://foo.example.com/";

  run_next_test();
}

let Http3CheckListener = function () {};

Http3CheckListener.prototype = {
  expectedRoute: "",
  expectedStatus: Cr.NS_OK,

  onStartRequest: function testOnStartRequest(request) {
    Assert.ok(request instanceof Ci.nsIHttpChannel);
    Assert.equal(request.status, this.expectedStatus);
    if (Components.isSuccessCode(this.expectedStatus)) {
      Assert.equal(request.responseStatus, 200);
    }
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    Assert.equal(status, this.expectedStatus);
    if (Components.isSuccessCode(this.expectedStatus)) {
      Assert.equal(request.responseStatus, 200);
      let routed = "NA";
      try {
        routed = request.getRequestHeader("Alt-Used");
      } catch (e) {}
      dump("routed is " + routed + "\n");

      Assert.equal(routed, this.expectedRoute);

      let httpVersion = "";
      try {
        httpVersion = request.protocolVersion;
      } catch (e) {}
      Assert.equal(httpVersion, "h3");
    }

    do_test_finished();
  },
};

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function test_no_altsvc_pref() {
  dump("test_no_altsvc_pref");
  do_test_pending();

  let chan = makeChan(httpsOrigin + "http3-test");
  let listener = new Http3CheckListener();
  listener.expectedStatus = Cr.NS_ERROR_CONNECTION_REFUSED;
  chan.asyncOpen(listener);
}

function test_altsvc_pref() {
  dump("test_altsvc_pref");
  do_test_pending();

  prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    "foo.example.com;h3-29=" + h3AltSvc
  );

  let chan = makeChan(httpsOrigin + "http3-test");
  let listener = new Http3CheckListener();
  listener.expectedRoute = h3Route;
  chan.asyncOpen(listener);
}

function testsDone() {
  prefs.clearUserPref("network.http.http3.enable");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.dns.disableIPv6");
  prefs.clearUserPref("network.http.http3.alt-svc-mapping-for-testing");
  dump("testDone\n");
}
