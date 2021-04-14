/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head_channels.js */
/* import-globals-from head_cookies.js */

async function http3_setup_tests() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  let h3Port = env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  let h3Route = "foo.example.com:" + h3Port;
  do_get_profile();

  Services.prefs.setBoolPref("network.http.http3.enabled", true);
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  Services.prefs.setBoolPref("network.dns.disableIPv6", true);
  Services.prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    "foo.example.com;h3-27=:" + h3Port
  );

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  await setup_altsvc("https://foo.example.com/", h3Route);
}

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

let CheckHttp3Listener = function() {};

CheckHttp3Listener.prototype = {
  expectedRoute: "",

  onStartRequest: function testOnStartRequest(request) {},

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
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
      this.finish(true);
    } else {
      dump("try again to get alt svc mapping\n");
      this.finish(false);
    }
  },
};

async function setup_altsvc(uri, expectedRoute) {
  let result = false;
  do {
    let chan = makeChan(uri);
    let listener = new CheckHttp3Listener();
    listener.expectedRoute = expectedRoute;
    result = await altsvcSetupPromise(chan, listener);
    dump("results=" + result);
  } while (result === false);
}

function altsvcSetupPromise(chan, listener) {
  return new Promise(resolve => {
    function finish(result) {
      resolve(result);
    }
    listener.finish = finish;
    chan.asyncOpen(listener);
  });
}

function http3_clear_prefs() {
  Services.prefs.clearUserPref("network.http.http3.enabled");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.dns.disableIPv6");
  Services.prefs.clearUserPref(
    "network.http.http3.alt-svc-mapping-for-testing"
  );
  dump("cleanup done\n");
}
