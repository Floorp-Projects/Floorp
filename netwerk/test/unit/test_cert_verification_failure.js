/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

async function test_cert_failure(server_or_proxy, server_cert) {
  let server = new server_or_proxy();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  let chan = makeChan(`https://localhost:${server.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
  equal(req.status, 0x805a1ff3); // SEC_ERROR_UNKNOWN_ISSUER
  let secinfo = req.securityInfo;
  secinfo.QueryInterface(Ci.nsITransportSecurityInfo);
  if (server_cert) {
    Assert.equal(secinfo.serverCert.commonName, " HTTP2 Test Cert");
  } else {
    Assert.equal(secinfo.serverCert.commonName, " Proxy Test Cert");
  }
}

add_task(async function test_https() {
  await test_cert_failure(NodeHTTPSServer, true);
});

add_task(async function test_http2() {
  await test_cert_failure(NodeHTTP2Server, true);
});

add_task(async function test_https_proxy() {
  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();
  registerCleanupFunction(() => {
    proxy.stop();
  });
  await test_cert_failure(NodeHTTPSServer, false);
});

add_task(async function test_http2_proxy() {
  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start();
  registerCleanupFunction(() => {
    proxy.stop();
  });

  await test_cert_failure(NodeHTTPSServer, false);
});
