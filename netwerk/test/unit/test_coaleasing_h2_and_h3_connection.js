/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

let h2Port;
let h3Port;

add_setup(async function setup() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");

  Services.prefs.setBoolPref("network.http.http3.enable", true);
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);
  Services.prefs.setBoolPref("network.http.altsvc.oe", true);

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
});

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.http.http3.enable");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  Services.prefs.clearUserPref("network.http.altsvc.oe");
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

add_task(async function testNotCoaleasingH2Connection() {
  const host = "foo.example.com";
  Services.prefs.setCharPref("network.dns.localDomains", host);

  let server = new NodeHTTPSServer();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });

  await server.execute(`global.h3Port = "${h3Port}";`);
  await server.registerPathHandler("/altsvc", (req, resp) => {
    const body = "done";
    resp.setHeader("Content-Length", body.length);
    resp.setHeader("Alt-Svc", `h3-29=:${global.h3Port}`);
    resp.writeHead(200);
    resp.write(body);
    resp.end("");
  });

  let chan = makeChan(`https://${host}:${server.port()}/altsvc`);
  let [req] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "http/1.1");

  // Some delay to make sure the H3 speculative connection is created.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  // To clear the altsvc cache.
  Services.obs.notifyObservers(null, "last-pb-context-exited");

  // Add another alt-svc header to route to moz-http2.js.
  Services.prefs.setCharPref(
    "network.http.http3.alt-svc-mapping-for-testing",
    `${host};h2=:${h2Port}`
  );

  let start = new Date().getTime();
  chan = makeChan(`https://${host}:${server.port()}/server-timing`);
  chan.QueryInterface(Ci.nsIHttpChannelInternal).beConservative = true;
  [req] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "h2");

  // The time this request takes should be way more less than the
  // neqo idle timeout (30s).
  let duration = (new Date().getTime() - start) / 1000;
  Assert.less(duration, 10);
});
