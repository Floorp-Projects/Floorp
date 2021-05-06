/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.ssl_tokens_cache_enabled");
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests("h3-27");
});

let Http3Listener = function() {};

Http3Listener.prototype = {
  resumed: false,

  onStartRequest: function testOnStartRequest(request) {
    Assert.equal(request.status, Cr.NS_OK);
    Assert.equal(request.responseStatus, 200);

    let secinfo = request.securityInfo;
    secinfo.QueryInterface(Ci.nsITransportSecurityInfo);
    Assert.equal(secinfo.resumed, this.resumed);
    Assert.ok(secinfo.serverCert != null);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3-27");

    this.finish();
  },
};

function chanPromise(chan, listener) {
  return new Promise(resolve => {
    function finish(result) {
      resolve(result);
    }
    listener.finish = finish;
    chan.asyncOpen(listener);
  });
}

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

async function test_first_conn_no_resumed() {
  let listener = new Http3Listener();
  listener.resumed = false;
  let chan = makeChan("https://foo.example.com/30");
  await chanPromise(chan, listener);
}

async function test_0RTT(enable_ssl_tokens_cache, enable_0rtt, resumed) {
  info(
    `enable_ssl_tokens_cache=${enable_ssl_tokens_cache} enable_0rtt=${enable_0rtt} resumed=${resumed}`
  );
  Services.prefs.setBoolPref(
    "network.ssl_tokens_cache_enabled",
    enable_ssl_tokens_cache
  );
  Services.prefs.setBoolPref("network.http.http3.enable_0rtt", enable_0rtt);

  // Make sure the h3 connection created by the previous test is cleared.
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  await new Promise(resolve => setTimeout(resolve, 1000));

  // This connecion should be resumed.
  let listener = new Http3Listener();
  listener.resumed = resumed;
  let chan = makeChan("https://foo.example.com/30");
  await chanPromise(chan, listener);
}

add_task(async function test_0RTT_setups() {
  await test_first_conn_no_resumed();
  // http3.0RTT and network.ssl_tokens_cache_enabled enabled
  // ssl_tokens_cache
  await test_0RTT(true, true, true);

  // http3.0RTT enabled and network.ssl_tokens_cache_enabled disabled
  await test_0RTT(false, true, true);

  // http3.0RTT disabled and network.ssl_tokens_cache_enabled enabled
  await test_0RTT(true, false, false);

  // http3.0RTT disabled and network.ssl_tokens_cache_enabled disabled
  await test_0RTT(false, false, false);
});
