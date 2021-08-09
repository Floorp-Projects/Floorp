/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test if a HTTP3 connection can be established when a proxy info says
// to use direct connection

"use strict";

registerCleanupFunction(async () => {
  http3_clear_prefs();
  Services.prefs.clearUserPref("network.proxy.type");
  Services.prefs.clearUserPref("network.proxy.autoconfig_url");
});

add_task(async function setup() {
  await http3_setup_tests("h3-29");
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_task(async function testHttp3WithDirectProxy() {
  var pac =
    "data:text/plain," +
    "function FindProxyForURL(url, host) {" +
    '  return "DIRECT; PROXY foopy:8080;"' +
    "}";

  // Configure PAC
  Services.prefs.setIntPref("network.proxy.type", 2);
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pac);

  let chan = makeChan(`https://foo.example.com`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3-29");
});
