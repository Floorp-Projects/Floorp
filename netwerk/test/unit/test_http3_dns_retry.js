/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

let h2Port;
let h3Port;
let trrServer;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

add_setup(async function setup() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");

  trr_test_setup();

  if (mozinfo.socketprocess_networking) {
    Cc["@mozilla.org/network/protocol;1?name=http"].getService(
      Ci.nsIHttpProtocolHandler
    );
    Services.dns; // Needed to trigger socket process.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 1000));
  }

  Services.prefs.setIntPref("network.trr.mode", 2); // TRR first
  Services.prefs.setBoolPref("network.http.http3.enable", true);
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 6);
  Services.prefs.setBoolPref(
    "network.http.http3.block_loopback_ipv6_addr",
    true
  );
  Services.prefs.setBoolPref(
    "network.http.http3.retry_different_ip_family",
    true
  );
  Services.prefs.setBoolPref("network.dns.get-ttl", false);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  registerCleanupFunction(async () => {
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
    trr_clear_prefs();
    Services.prefs.clearUserPref(
      "network.http.http3.retry_different_ip_family"
    );
    Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
    Services.prefs.clearUserPref("network.http.http3.block_loopback_ipv6_addr");
    Services.prefs.clearUserPref("network.dns.get-ttl");
    if (trrServer) {
      await trrServer.stop();
    }
  });
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
  // eslint-disable-next-line no-async-promise-executor
  return new Promise(async resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();

    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

async function registerDoHAnswers(host, ipv4Answers, ipv6Answers, httpsRecord) {
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers(host, "HTTPS", {
    answers: httpsRecord,
  });

  await trrServer.registerDoHAnswers(host, "AAAA", {
    answers: ipv6Answers,
  });

  await trrServer.registerDoHAnswers(host, "A", {
    answers: ipv4Answers,
  });

  Services.dns.clearCache(true);
}

// Test if we retry IPv4 address for Http/3 properly.
add_task(async function test_retry_with_ipv4() {
  let host = "test.http3_retry.com";
  let ipv4answers = [
    {
      name: host,
      ttl: 55,
      type: "A",
      flush: false,
      data: "127.0.0.1",
    },
  ];
  // The UDP socket will return connection refused error because we set
  // "network.http.http3.block_loopback_ipv6_addr" to true.
  let ipv6answers = [
    {
      name: host,
      ttl: 55,
      type: "AAAA",
      flush: false,
      data: "::1",
    },
  ];
  let httpsRecord = [
    {
      name: host,
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: host,
        values: [
          { key: "alpn", value: "h3" },
          { key: "port", value: h3Port },
        ],
      },
    },
  ];

  await registerDoHAnswers(host, ipv4answers, ipv6answers, httpsRecord);

  let chan = makeChan(`https://${host}`);
  let [req] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "h3");

  await trrServer.stop();
});

add_task(async function test_retry_with_ipv4_disabled() {
  let host = "test.http3_retry_ipv4_blocked.com";
  let ipv4answers = [
    {
      name: host,
      ttl: 55,
      type: "A",
      flush: false,
      data: "127.0.0.1",
    },
  ];
  // The UDP socket will return connection refused error because we set
  // "network.http.http3.block_loopback_ipv6_addr" to true.
  let ipv6answers = [
    {
      name: host,
      ttl: 55,
      type: "AAAA",
      flush: false,
      data: "::1",
    },
  ];
  let httpsRecord = [
    {
      name: host,
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: host,
        values: [
          { key: "alpn", value: "h3" },
          { key: "port", value: h3Port },
        ],
      },
    },
  ];

  await registerDoHAnswers(host, ipv4answers, ipv6answers, httpsRecord);

  let chan = makeChan(`https://${host}`);
  chan.QueryInterface(Ci.nsIHttpChannelInternal);
  chan.setIPv4Disabled();

  await channelOpenPromise(chan, CL_EXPECT_FAILURE);
  await trrServer.stop();
});

// See bug 1837252. There is no way to observe the outcome of this test, because
// the crash in bug 1837252 is only triggered by speculative connection.
// The outcome of this test is no crash.
add_task(async function test_retry_with_ipv4_failed() {
  let host = "test.http3_retry_failed.com";
  // Return a wrong answer intentionally.
  let ipv4answers = [
    {
      name: host,
      ttl: 55,
      type: "AAAA",
      flush: false,
      data: "127.0.0.1",
    },
  ];
  // The UDP socket will return connection refused error because we set
  // "network.http.http3.block_loopback_ipv6_addr" to true.
  let ipv6answers = [
    {
      name: host,
      ttl: 55,
      type: "AAAA",
      flush: false,
      data: "::1",
    },
  ];
  let httpsRecord = [
    {
      name: host,
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: host,
        values: [
          { key: "alpn", value: "h3" },
          { key: "port", value: h3Port },
        ],
      },
    },
  ];

  await registerDoHAnswers(host, ipv4answers, ipv6answers, httpsRecord);

  // This speculative connection is used to trigger the mechanism to retry
  // Http/3 connection with a IPv4 address.
  // We want to make the connection entry's IP preference known,
  // so DnsAndConnectSocket::mRetryWithDifferentIPFamily will be set to true
  // before the second speculative connection.
  let uri = Services.io.newURI(`https://test.http3_retry_failed.com`);
  Services.io.speculativeConnect(
    uri,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    false
  );

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 3000));

  // When this speculative connection is created, the connection entry is
  // already set to prefer IPv4. Since we provided an invalid A response,
  // DnsAndConnectSocket::OnLookupComplete is called with an error.
  // Since DnsAndConnectSocket::mRetryWithDifferentIPFamily is true, we do
  // retry DNS lookup. During retry, we should not create UDP connection.
  Services.io.speculativeConnect(
    uri,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    false
  );

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 3000));
  await trrServer.stop();
});

add_task(async function test_retry_with_0rtt() {
  let host = "test.http3_retry_0rtt.com";
  let ipv4answers = [
    {
      name: host,
      ttl: 55,
      type: "A",
      flush: false,
      data: "127.0.0.1",
    },
  ];
  // The UDP socket will return connection refused error because we set
  // "network.http.http3.block_loopback_ipv6_addr" to true.
  let ipv6answers = [
    {
      name: host,
      ttl: 55,
      type: "AAAA",
      flush: false,
      data: "::1",
    },
  ];
  let httpsRecord = [
    {
      name: host,
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: host,
        values: [
          { key: "alpn", value: "h3" },
          { key: "port", value: h3Port },
        ],
      },
    },
  ];

  await registerDoHAnswers(host, ipv4answers, ipv6answers, httpsRecord);

  let chan = makeChan(`https://${host}`);
  chan.QueryInterface(Ci.nsIHttpChannelInternal);
  chan.setIPv6Disabled();

  let [req] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "h3");

  // Make sure the h3 connection created by the previous test is cleared.
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  chan = makeChan(`https://${host}`);
  chan.QueryInterface(Ci.nsIHttpChannelInternal);

  [req] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "h3");

  await trrServer.stop();
});
