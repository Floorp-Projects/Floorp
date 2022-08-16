/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
var { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

let h2Port;
let trrServer;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function setup() {
  Services.prefs.setIntPref("network.max_socket_process_failed_count", 2);
  trr_test_setup();

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  Assert.ok(mozinfo.socketprocess_networking);
}

setup();
registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.max_socket_process_failed_count");
  trr_clear_prefs();
  if (trrServer) {
    await trrServer.stop();
  }
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
      certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
        false
      );
    }
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      true
    );
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_task(async function setupTRRServer() {
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  // Only the last record is valid to use.
  await trrServer.registerDoHAnswers("test.example.com", "HTTPS", {
    answers: [
      {
        name: "test.example.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.example.com",
          values: [
            { key: "alpn", value: ["h2"] },
            { key: "port", value: h2Port },
          ],
        },
      },
    ],
  });

  await trrServer.registerDoHAnswers("test.example.com", "A", {
    answers: [
      {
        name: "test.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });
});

async function doTestSimpleRequest(fromSocketProcess) {
  let { inRecord } = await new TRRDNSListener("test.example.com", "127.0.0.1");
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  Assert.equal(inRecord.resolvedInSocketProcess(), fromSocketProcess);

  let chan = makeChan(`https://test.example.com/server-timing`);
  let [req] = await channelOpenPromise(chan);
  // Test if this request is done by h2.
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  Assert.equal(internal.isLoadedBySocketProcess, fromSocketProcess);
}

// Test if the data is loaded from socket process.
add_task(async function testSimpleRequest() {
  await doTestSimpleRequest(true);
});

function killSocketProcess(pid) {
  const ProcessTools = Cc["@mozilla.org/processtools-service;1"].getService(
    Ci.nsIProcessToolsService
  );
  ProcessTools.kill(pid);
}

// Test if socket process is restarted.
add_task(async function testSimpleRequestAfterCrash() {
  let socketProcessId = Services.io.socketProcessId;
  info(`socket process pid is ${socketProcessId}`);
  Assert.ok(socketProcessId != 0);

  killSocketProcess(socketProcessId);

  info("wait socket process restart...");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));
  await TestUtils.waitForCondition(() => Services.io.socketProcessLaunched);

  await doTestSimpleRequest(true);
});

// Test if data is loaded from parent process.
add_task(async function testTooManyCrashes() {
  let socketProcessId = Services.io.socketProcessId;
  info(`socket process pid is ${socketProcessId}`);
  Assert.ok(socketProcessId != 0);

  let socketProcessCrashed = false;
  Services.obs.addObserver(function observe(subject, topic, data) {
    Services.obs.removeObserver(observe, topic);
    socketProcessCrashed = true;
  }, "network:socket-process-crashed");

  killSocketProcess(socketProcessId);
  await TestUtils.waitForCondition(() => socketProcessCrashed);
  await doTestSimpleRequest(false);
});
