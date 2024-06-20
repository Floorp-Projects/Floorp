/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head_channels.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_servers.js */

async function http3_setup_tests(http3version, reload) {
  let h3Port;
  if (reload) {
    let h3ServerPath = Services.env.get("MOZ_HTTP3_SERVER_PATH");
    let h3DBPath = Services.env.get("MOZ_HTTP3_CERT_DB_PATH");
    let server = new HTTP3Server();
    h3Port = await server.start(h3ServerPath, h3DBPath);
    registerCleanupFunction(async () => {
      await server.stop();
    });
  } else {
    h3Port = Services.env.get("MOZHTTP3_PORT");
  }

  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");

  let h3Route = "foo.example.com:" + h3Port;
  do_get_profile();

  Services.prefs.setBoolPref("network.http.http3.enable", true);
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  Services.prefs.setBoolPref("network.dns.disableIPv6", true);
  Services.prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    `foo.example.com;${http3version}=:${h3Port}`
  );

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // `../unit/` so that unit_ipc tests can use as well
  addCertFromFile(certdb, "../unit/http2-ca.pem", "CTu,u,u");

  await setup_altsvc("https://foo.example.com/", h3Route, http3version);
}

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

let CheckHttp3Listener = function () {};

CheckHttp3Listener.prototype = {
  expectedRoute: "",
  http3version: "",

  onStartRequest: function testOnStartRequest() {},

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
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
      Assert.equal(httpVersion, this.http3version);
      this.finish(true);
    } else {
      dump("try again to get alt svc mapping\n");
      this.finish(false);
    }
  },
};

async function setup_altsvc(uri, expectedRoute, http3version) {
  let result = false;
  do {
    let chan = makeChan(uri);
    let listener = new CheckHttp3Listener();
    listener.expectedRoute = expectedRoute;
    listener.http3version = http3version;
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
  Services.prefs.clearUserPref("network.http.http3.enable");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.dns.disableIPv6");
  Services.prefs.clearUserPref(
    "network.http.http3.alt-svc-mapping-for-testing"
  );
  Services.prefs.clearUserPref("network.http.http3.support_version1");
  dump("cleanup done\n");
}
