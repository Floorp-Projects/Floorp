/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

const SIZE = 4096;
const CONTENT = "x".repeat(SIZE);

add_task(async function test_slow_upload() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");

  let proxies = [
    NodeHTTPProxyServer,
    NodeHTTPSProxyServer,
    NodeHTTP2ProxyServer,
  ];
  for (let p of proxies) {
    let proxy = new p();
    await proxy.start();
    registerCleanupFunction(async () => {
      await proxy.stop();
    });

    await with_node_servers(
      [NodeHTTPServer, NodeHTTPSServer, NodeHTTP2Server],
      async server => {
        info(`Testing ${p.name} with ${server.constructor.name}`);
        await server.execute(
          `global.server_name = "${server.constructor.name}";`
        );
        await server.registerPathHandler("/test", (req, resp) => {
          let content = "";
          req.on("data", data => {
            global.data_count = (global.data_count || 0) + 1;
            content += data;
          });
          req.on("end", () => {
            resp.writeHead(200);
            resp.end(content);
          });
        });

        let sstream = Cc[
          "@mozilla.org/io/string-input-stream;1"
        ].createInstance(Ci.nsIStringInputStream);
        sstream.data = CONTENT;

        let mime = Cc[
          "@mozilla.org/network/mime-input-stream;1"
        ].createInstance(Ci.nsIMIMEInputStream);
        mime.addHeader("Content-Type", "multipart/form-data; boundary=zzzzz");
        mime.setData(sstream);

        let tq = Cc["@mozilla.org/network/throttlequeue;1"].createInstance(
          Ci.nsIInputChannelThrottleQueue
        );
        // Make sure the request takes more than one read.
        tq.init(100 + SIZE / 2, 100 + SIZE / 2);

        let chan = NetUtil.newChannel({
          uri: `${server.origin()}/test`,
          loadUsingSystemPrincipal: true,
        }).QueryInterface(Ci.nsIHttpChannel);

        let tic = chan.QueryInterface(Ci.nsIThrottledInputChannel);
        tic.throttleQueue = tq;

        chan
          .QueryInterface(Ci.nsIUploadChannel)
          .setUploadStream(mime, "", mime.available());
        chan.requestMethod = "POST";

        let { req, buff } = await new Promise(resolve => {
          chan.asyncOpen(
            new ChannelListener(
              (req1, buff1) => resolve({ req: req1, buff: buff1 }),
              null,
              CL_ALLOW_UNKNOWN_CL
            )
          );
        });
        equal(req.status, Cr.NS_OK);
        equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
        Assert.equal(buff, CONTENT, "Content must match");
        ok(!!req.QueryInterface(Ci.nsIProxiedChannel).proxyInfo);
        greater(
          await server.execute(`global.data_count`),
          1,
          "Content should have been streamed to the server in several chunks"
        );
      }
    );
    await proxy.stop();
  }
});
