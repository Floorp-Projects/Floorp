/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

function makeChan(uri) {
  var principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "http://example.com"
    );
  let chan = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

async function setup() {
  if (!inChildProcess()) {
    Services.prefs.setBoolPref("browser.opaqueResponseBlocking", true);
  }
  let server = new NodeHTTPServer();
  await server.start();
  registerCleanupFunction(async () => {
    await server.stop();
  });
  await server.registerPathHandler("/dosniff", (req, resp) => {
    resp.writeHead(500, {
      "Content-Type": "application/json",
      "Set-Cookie": "mycookie",
    });
    resp.write("good");
    resp.end("done");
  });
  await server.registerPathHandler("/nosniff", (req, resp) => {
    resp.writeHead(500, {
      "Content-Type": "application/msword",
      "Set-Cookie": "mycookie",
    });
    resp.write("good");
    resp.end("done");
  });

  return server;
}
async function test_empty_header(server, doSniff) {
  let chan;
  if (doSniff) {
    chan = makeChan(`${server.origin()}/dosniff`);
  } else {
    chan = makeChan(`${server.origin()}/nosniff`);
  }
  let req = await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });
  equal(req.status, Cr.NS_ERROR_FAILURE);
  equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 500);

  req.visitResponseHeaders({
    visitHeader: function visit(_aName, _aValue) {
      ok(false);
    },
  });
}

add_task(async function () {
  let server = await setup();
  await test_empty_header(server, true);
  await test_empty_header(server, false);
});
