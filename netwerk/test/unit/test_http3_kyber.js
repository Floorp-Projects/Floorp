/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("security.tls.enable_kyber");
  Services.prefs.clearUserPref("network.http.http3.enable_kyber");
  http3_clear_prefs();
});

add_task(async function setup() {
  Services.prefs.setBoolPref("security.tls.enable_kyber", true);
  Services.prefs.setBoolPref("network.http.http3.enable_kyber", true);
  await http3_setup_tests("h3");
});

let Http3Listener = function () {};

Http3Listener.prototype = {
  expectedKeaGroup: undefined,

  onStartRequest: function testOnStartRequest(request) {
    Assert.equal(request.status, Cr.NS_OK);
    Assert.equal(request.responseStatus, 200);

    Assert.equal(request.securityInfo.keaGroupName, this.expectedKeaGroup);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3");

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

add_task(async function test_kyber_success() {
  let listener = new Http3Listener();
  listener.expectedKeaGroup = "xyber768d00";
  let chan = makeChan("https://foo.example.com");
  await chanPromise(chan, listener);
});

add_task(async function test_no_kyber_on_retry() {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");

  let listener = new Http3Listener();
  listener.expectedKeaGroup = "x25519";
  let chan = makeChan("https://foo.example.com");
  chan.QueryInterface(Ci.nsIHttpChannelInternal).tlsFlags =
    Ci.nsIHttpChannelInternal.TLS_FLAG_CONFIGURE_AS_RETRY;
  await chanPromise(chan, listener);
});
