/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);
addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

async function createServer() {
  let server = new NodeHTTP2Server();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  await server.registerPathHandler("/", (req, resp) => {
    let content = `hello from ${req.authority} | ${req.socket.remotePort}`;
    resp.writeHead(200, {
      "Content-Type": "text/plain",
      "Content-Length": `${content.length}`,
    });
    resp.end(content);
  });
  return server;
}

let IP1 = "127.0.0.1";
let IP2 = "127.0.0.2";
if (AppConstants.platform == "macosx") {
  // OSX doesn't use 127.0.0.2 as a local interface
  IP2 = "::1";
} else if (AppConstants.platform == "android") {
  IP2 = "10.0.2.2";
}

async function openChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;

  let { req, buffer } = await new Promise(resolve => {
    function finish(r, b) {
      resolve({ req: r, buffer: b });
    }
    chan.asyncOpen(new ChannelListener(finish, null, CL_ALLOW_UNKNOWN_CL));
  });

  return {
    buffer,
    port: buffer.split("|")[1],
    addr: req.QueryInterface(Ci.nsIHttpChannelInternal).remoteAddress,
    status: req.QueryInterface(Ci.nsIHttpChannel).responseStatus,
  };
}

add_task(async function test_dontCoalesce() {
  let server = await createServer();
  Services.prefs.setBoolPref("network.http.http2.aggressive_coalescing", false);
  override.clearOverrides();
  Services.dns.clearCache(true);

  override.addIPOverride("foo.example.com", IP1);
  override.addIPOverride("foo.example.com", IP2);
  override.addIPOverride("alt1.example.com", IP2);

  let { addr: addr1 } = await openChan(
    `https://foo.example.com:${server.port()}/`
  );
  let { addr: addr2 } = await openChan(
    `https://alt1.example.com:${server.port()}/`
  );

  Assert.notEqual(addr1, addr2);
  await server.stop();
});

add_task(async function test_doCoalesce() {
  let server = await createServer();
  Services.prefs.setBoolPref("network.http.http2.aggressive_coalescing", false);
  override.clearOverrides();
  Services.dns.clearCache(true);

  override.addIPOverride("foo.example.com", IP1);
  override.addIPOverride("foo.example.com", IP2);
  override.addIPOverride("alt2.example.com", IP1);
  override.addIPOverride("alt2.example.com", IP2);

  let { port: port1, addr: addr1 } = await openChan(
    `https://foo.example.com:${server.port()}/`
  );
  let { port: port2, addr: addr2 } = await openChan(
    `https://alt2.example.com:${server.port()}/`
  );

  Assert.equal(addr1, addr2);
  Assert.equal(port1, port2);
  await server.stop();
});

add_task(async function test_doCoalesceAggresive() {
  let server = await createServer();

  Services.prefs.setBoolPref("network.http.http2.aggressive_coalescing", true);
  override.clearOverrides();
  Services.dns.clearCache(true);

  override.addIPOverride("foo.example.com", IP1);
  override.addIPOverride("foo.example.com", IP2);
  override.addIPOverride("alt1.example.com", IP2);

  let { port: port1, addr: addr1 } = await openChan(
    `https://foo.example.com:${server.port()}/`
  );
  let { port: port2, addr: addr2 } = await openChan(
    `https://alt1.example.com:${server.port()}/`
  );

  Assert.equal(addr1, addr2);
  Assert.equal(port1, port2);
  await server.stop();
});

// On android because of the way networking is set up the
// localAddress is always ::ffff:127.0.0.1 so it can't be
// used to make a decision.
add_task(
  { skip_if: () => AppConstants.platform == "android" },
  async function test_doCoalesceAggresive421() {
    let server = await createServer();

    await server.execute(`global.rightIP = "${IP2}"`);

    await server.registerPathHandler("/", (req, resp) => {
      let content = `hello from ${req.authority} | ${req.socket.remotePort}`;
      // Check that returning 421 when aggresively coalescing
      // makes Firefox not coalesce the connections.
      if (
        req.authority.startsWith("alt1.example.com") &&
        req.socket.localAddress != global.rightIP &&
        req.socket.localAddress != `::ffff:${global.rightIP}`
      ) {
        resp.writeHead(421, {
          "Content-Type": "text/plain",
          "Content-Length": `${content.length}`,
        });
        resp.end(content);
        return;
      }
      resp.writeHead(200, {
        "Content-Type": "text/plain",
        "Content-Length": `${content.length}`,
      });
      resp.end(content);
    });

    Services.prefs.setBoolPref(
      "network.http.http2.aggressive_coalescing",
      true
    );
    override.clearOverrides();
    Services.dns.clearCache(true);

    override.addIPOverride("foo.example.com", IP1);
    override.addIPOverride("foo.example.com", IP2);
    override.addIPOverride("alt1.example.com", IP2);

    let {
      addr: addr1,
      status: status1,
      port: port1,
    } = await openChan(`https://foo.example.com:${server.port()}/`);
    Assert.equal(status1, 200);
    Assert.equal(addr1, IP1);
    let {
      addr: addr2,
      status: status2,
      port: port2,
    } = await openChan(`https://alt1.example.com:${server.port()}/`);

    Assert.equal(status2, 200);
    Assert.equal(addr2, IP2);
    Assert.notEqual(port1, port2);
    await server.stop();
  }
);
