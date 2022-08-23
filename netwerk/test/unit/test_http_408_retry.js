/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function loadURL(uri, flags) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;

  return new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buff) => resolve({ req, buff }), null, flags)
    );
  });
}

add_task(async function test() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  async function check408retry(server) {
    info(`Testing ${server.constructor.name}`);
    await server.execute(`global.server_name = "${server.constructor.name}";`);
    if (
      server.constructor.name == "NodeHTTPServer" ||
      server.constructor.name == "NodeHTTPSServer"
    ) {
      await server.registerPathHandler("/test", (req, resp) => {
        let oldSock = global.socket;
        global.socket = resp.socket;
        if (global.socket == oldSock) {
          // This function is handled within the httpserver where setTimeout is
          // available.
          // eslint-disable-next-line mozilla/no-arbitrary-setTimeout, no-undef
          setTimeout(
            arg => {
              arg.writeHead(408);
              arg.end("stuff");
            },
            1100,
            resp
          );
          return;
        }
        resp.writeHead(200);
        resp.end(global.server_name);
      });
    } else {
      await server.registerPathHandler("/test", (req, resp) => {
        global.socket = resp.socket;
        if (!global.sent408) {
          global.sent408 = true;
          resp.writeHead(408);
          resp.end("stuff");
          return;
        }
        resp.writeHead(200);
        resp.end(global.server_name);
      });
    }

    async function load() {
      let { req, buff } = await loadURL(
        `${server.origin()}/test`,
        CL_ALLOW_UNKNOWN_CL
      );
      equal(req.status, Cr.NS_OK);
      equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
      equal(buff, server.constructor.name);
      equal(
        req.QueryInterface(Ci.nsIHttpChannel).protocolVersion,
        server.constructor.name == "NodeHTTP2Server" ? "h2" : "http/1.1"
      );
    }

    info("first load");
    await load();
    info("second load");
    await load();
  }

  await with_node_servers(
    [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
    check408retry
  );
});
