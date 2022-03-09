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

add_task(async function test_http() {
  let server = new NodeHTTPServer();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  let chan = makeChan(`http://localhost:${server.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 404);
  await server.registerPathHandler("/test", (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
  chan = makeChan(`http://localhost:${server.port()}/test`);
  req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
  equal(req.QueryInterface(Ci.nsIHttpChannel).protocolVersion, "http/1.1");
});

add_task(async function test_https() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let server = new NodeHTTPSServer();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  let chan = makeChan(`https://localhost:${server.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 404);
  await server.registerPathHandler("/test", (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
  chan = makeChan(`https://localhost:${server.port()}/test`);
  req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
  equal(req.QueryInterface(Ci.nsIHttpChannel).protocolVersion, "http/1.1");
});

add_task(async function test_http2() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let server = new NodeHTTP2Server();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  let chan = makeChan(`https://localhost:${server.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 404);
  await server.registerPathHandler("/test", (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
  chan = makeChan(`https://localhost:${server.port()}/test`);
  req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
  equal(req.QueryInterface(Ci.nsIHttpChannel).protocolVersion, "h2");
});

add_task(async function test_http1_proxy() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPProxyServer();
  await proxy.start();
  registerCleanupFunction(() => {
    proxy.stop();
  });

  let chan = makeChan(`http://localhost:${proxy.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 405);

  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    async server => {
      await server.execute(
        `global.server_name = "${server.constructor.name}";`
      );
      await server.registerPathHandler("/test", (req, resp) => {
        resp.writeHead(200);
        resp.end(global.server_name);
      });
      let chan = makeChan(`${server.origin()}/test`);
      let { req, buff } = await new Promise(resolve => {
        chan.asyncOpen(
          new ChannelListener(
            (req, buff) => resolve({ req, buff }),
            null,
            CL_ALLOW_UNKNOWN_CL
          )
        );
      });
      equal(req.status, Cr.NS_OK);
      equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
      equal(buff, server.constructor.name);
      equal(
        req.QueryInterface(Ci.nsIHttpChannel).protocolVersion,
        server.constructor.name == "NodeHTTP2Server" ? "h2" : "http/1.1"
      );
    }
  );
});

add_task(async function test_https_proxy() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();
  registerCleanupFunction(() => {
    proxy.stop();
  });

  let chan = makeChan(`https://localhost:${proxy.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 405);

  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    async server => {
      await server.execute(
        `global.server_name = "${server.constructor.name}";`
      );
      await server.registerPathHandler("/test", (req, resp) => {
        resp.writeHead(200);
        resp.end(global.server_name);
      });
      let chan = makeChan(`${server.origin()}/test`);
      let { req, buff } = await new Promise(resolve => {
        chan.asyncOpen(
          new ChannelListener(
            (req, buff) => resolve({ req, buff }),
            null,
            CL_ALLOW_UNKNOWN_CL
          )
        );
      });
      equal(req.status, Cr.NS_OK);
      equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
      equal(buff, server.constructor.name);
      equal(
        req.QueryInterface(Ci.nsIHttpChannel).protocolVersion,
        server.constructor.name == "NodeHTTP2Server" ? "h2" : "http/1.1"
      );
    }
  );
});

add_task(async function test_http2_proxy() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start();
  registerCleanupFunction(() => {
    proxy.stop();
  });

  let chan = makeChan(`https://localhost:${proxy.port()}/test`);
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });
  equal(req.status, Cr.NS_OK);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 405);

  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    async server => {
      await server.execute(
        `global.server_name = "${server.constructor.name}";`
      );
      await server.registerPathHandler("/test", (req, resp) => {
        resp.writeHead(200);
        resp.end(global.server_name);
      });
      let chan = makeChan(`${server.origin()}/test`);
      let { req, buff } = await new Promise(resolve => {
        chan.asyncOpen(
          new ChannelListener(
            (req, buff) => resolve({ req, buff }),
            null,
            CL_ALLOW_UNKNOWN_CL
          )
        );
      });
      equal(req.status, Cr.NS_OK);
      equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
      equal(buff, server.constructor.name);
      equal(
        req.QueryInterface(Ci.nsIHttpChannel).protocolVersion,
        server.constructor.name == "NodeHTTP2Server" ? "h2" : "http/1.1"
      );
    }
  );
});
