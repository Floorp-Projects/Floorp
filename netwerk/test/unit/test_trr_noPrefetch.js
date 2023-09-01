/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let trrServer = null;
add_setup(async function setup() {
  if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    return;
  }

  trr_test_setup();
  Services.prefs.setBoolPref("network.dns.disablePrefetch", true);
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("network.dns.disablePrefetch");
    trr_clear_prefs();
  });

  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  // We need to define both A and AAAA responses, otherwise
  // we might race and pick up the skip reason for the other request.
  await trrServer.registerDoHAnswers(`myfoo.test`, "A", {
    answers: [],
  });
  await trrServer.registerDoHAnswers(`myfoo.test`, "AAAA", {
    answers: [],
  });

  // myfoo2.test will return sever error as it's not defined

  // return nxdomain for this one
  await trrServer.registerDoHAnswers(`myfoo3.test`, "A", {
    flags: 0x03,
    answers: [],
  });
  await trrServer.registerDoHAnswers(`myfoo3.test`, "AAAA", {
    flags: 0x03,
    answers: [],
  });

  await trrServer.registerDoHAnswers(`alt1.example.com`, "A", {
    answers: [
      {
        name: "alt1.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });
});

add_task(async function test_failure() {
  let req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: `http://myfoo.test/`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });

  equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST);
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).effectiveTRRMode,
    Ci.nsIRequest.TRR_ONLY_MODE
  );
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).trrSkipReason,
    Ci.nsITRRSkipReason.TRR_NO_ANSWERS
  );

  req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: `http://myfoo2.test/`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });

  equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST);
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).effectiveTRRMode,
    Ci.nsIRequest.TRR_ONLY_MODE
  );
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).trrSkipReason,
    Ci.nsITRRSkipReason.TRR_RCODE_FAIL
  );

  req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: `http://myfoo3.test/`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE));
  });

  equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST);
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).effectiveTRRMode,
    Ci.nsIRequest.TRR_ONLY_MODE
  );
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).trrSkipReason,
    Ci.nsITRRSkipReason.TRR_NXDOMAIN
  );
});

add_task(async function test_success() {
  let httpServer = new NodeHTTP2Server();
  await httpServer.start();
  await httpServer.registerPathHandler("/", (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
  registerCleanupFunction(async () => {
    await httpServer.stop();
  });

  let req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: `https://alt1.example.com:${httpServer.port()}/`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });

  equal(req.status, Cr.NS_OK);
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).effectiveTRRMode,
    Ci.nsIRequest.TRR_ONLY_MODE
  );
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).trrSkipReason,
    Ci.nsITRRSkipReason.TRR_OK
  );

  // Another request to check connection reuse
  req = await new Promise(resolve => {
    let chan = NetUtil.newChannel({
      uri: `https://alt1.example.com:${httpServer.port()}/second`,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });

  equal(req.status, Cr.NS_OK);
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).effectiveTRRMode,
    Ci.nsIRequest.TRR_ONLY_MODE
  );
  equal(
    req.QueryInterface(Ci.nsIHttpChannelInternal).trrSkipReason,
    Ci.nsITRRSkipReason.TRR_OK
  );
});
