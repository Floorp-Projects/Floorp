/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;
let listen;

function inChildProcess() {
  return (
    Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
      .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  );
}

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
const mainThread = threadManager.currentThread;

const defaultOriginAttributes = {};

function setup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("network.security.esni.enabled", false);
  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // make all native resolve calls "secretly" resolve localhost instead
  prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow
  prefs.setIntPref("network.trr.mode", 2); // TRR first
  prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  prefs.setCharPref("network.trr.confirmationNS", "skip");

  // So we can change the pref without clearing the cache to check a pushed
  // record with a TRR path that fails.
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
}

if (!inChildProcess()) {
  setup();
  registerCleanupFunction(() => {
    prefs.clearUserPref("network.security.esni.enabled");
    prefs.clearUserPref("network.http.spdy.enabled");
    prefs.clearUserPref("network.http.spdy.enabled.http2");
    prefs.clearUserPref("network.dns.localDomains");
    prefs.clearUserPref("network.dns.native-is-localhost");
    prefs.clearUserPref("network.trr.mode");
    prefs.clearUserPref("network.trr.uri");
    prefs.clearUserPref("network.trr.credentials");
    prefs.clearUserPref("network.trr.wait-for-portal");
    prefs.clearUserPref("network.trr.allow-rfc1918");
    prefs.clearUserPref("network.trr.useGET");
    prefs.clearUserPref("network.trr.confirmationNS");
    prefs.clearUserPref("network.trr.bootstrapAddress");
    prefs.clearUserPref("network.trr.blacklist-duration");
    prefs.clearUserPref("network.trr.request-timeout");
    prefs.clearUserPref("network.trr.clear-cache-on-pref-change");
  });
}

class DNSListener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }
  onLookupComplete(inRequest, inRecord, inStatus) {
    this.resolve([inRequest, inRecord, inStatus]);
  }
  // So we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

DNSListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIDNSListener",
]);

add_task(async function testHTTPSSVC() {
  // use the h2 server as DOH provider
  if (!inChildProcess()) {
    prefs.setCharPref(
      "network.trr.uri",
      "https://foo.example.com:" + h2Port + "/httpssvc"
    );
  }

  let listenerEsni = new DNSListener();
  let request = dns.asyncResolve(
    "test.httpssvc.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listenerEsni,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await listenerEsni;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "h3pool");
  Assert.equal(answer[0].values.length, 6);
  Assert.equal(
    answer[0].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    "h2,h3",
    "got correct answer"
  );
  Assert.ok(
    answer[0].values[1].QueryInterface(Ci.nsISVCParamNoDefaultAlpn),
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[2].QueryInterface(Ci.nsISVCParamPort).port,
    8888,
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[3].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[0]
      .address,
    "1.2.3.4",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[4].QueryInterface(Ci.nsISVCParamEchConfig).echconfig,
    "123...",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[5].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[0]
      .address,
    "::1",
    "got correct answer"
  );
  Assert.equal(answer[1].priority, 2);
  Assert.equal(answer[1].name, "test.httpssvc.com");
  Assert.equal(answer[1].values.length, 4);
  Assert.equal(
    answer[1].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    "h2",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[1].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[0]
      .address,
    "1.2.3.4",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[1].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[1]
      .address,
    "5.6.7.8",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[2].QueryInterface(Ci.nsISVCParamEchConfig).echconfig,
    "abc...",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[3].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[0]
      .address,
    "::1",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[3].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[1]
      .address,
    "fe80::794f:6d2c:3d5e:7836",
    "got correct answer"
  );
  Assert.equal(answer[2].priority, 3);
  Assert.equal(answer[2].name, "hello");
  Assert.equal(answer[2].values.length, 0);
});

add_task(async function test_aliasform() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => trrServer.stop());
  await trrServer.start();
  dump(`port = ${trrServer.port}\n`);

  if (inChildProcess()) {
    do_send_remote_message("mode3-port", trrServer.port);
    await do_await_remote_message("mode3-port-done");
  } else {
    Services.prefs.setIntPref("network.trr.mode", 3);
    Services.prefs.setCharPref(
      "network.trr.uri",
      `https://foo.example.com:${trrServer.port}/dns-query`
    );
  }

  // Test that HTTPS priority = 0 (AliasForm) behaves like a CNAME
  await trrServer.registerDoHAnswers("test.com", "A", [
    {
      name: "test.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: "something.com",
        values: [],
      },
    },
  ]);
  await trrServer.registerDoHAnswers("something.com", "A", [
    {
      name: "something.com",
      ttl: 55,
      type: "A",
      flush: false,
      data: "1.2.3.4",
    },
  ]);

  await new TRRDNSListener("test.com", { expectedAnswer: "1.2.3.4" });

  // Test a chain of HTTPSSVC AliasForm and CNAMEs
  await trrServer.registerDoHAnswers("x.com", "A", [
    {
      name: "x.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: "y.com",
        values: [],
      },
    },
  ]);
  await trrServer.registerDoHAnswers("y.com", "A", [
    {
      name: "y.com",
      type: "CNAME",
      ttl: 55,
      class: "IN",
      flush: false,
      data: "z.com",
    },
  ]);
  await trrServer.registerDoHAnswers("z.com", "A", [
    {
      name: "z.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: "target.com",
        values: [],
      },
    },
  ]);
  await trrServer.registerDoHAnswers("target.com", "A", [
    {
      name: "target.com",
      ttl: 55,
      type: "A",
      flush: false,
      data: "4.3.2.1",
    },
  ]);

  await new TRRDNSListener("x.com", { expectedAnswer: "4.3.2.1" });

  // We get a ServiceForm instead of a A answer, CNAME or AliasForm
  await trrServer.registerDoHAnswers("no-ip-host.com", "A", [
    {
      name: "no-ip-host.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "h3pool",
        values: [
          { key: "alpn", value: "h2,h3" },
          { key: "no-default-alpn" },
          { key: "port", value: 8888 },
          { key: "ipv4hint", value: "1.2.3.4" },
          { key: "echconfig", value: "123..." },
          { key: "ipv6hint", value: "::1" },
        ],
      },
    },
  ]);

  let [, , inStatus] = await new TRRDNSListener("no-ip-host.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Test CNAME/AliasForm loop
  await trrServer.registerDoHAnswers("loop.com", "A", [
    {
      name: "loop.com",
      type: "CNAME",
      ttl: 55,
      class: "IN",
      flush: false,
      data: "loop2.com",
    },
  ]);
  await trrServer.registerDoHAnswers("loop2.com", "A", [
    {
      name: "loop2.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: "loop.com",
        values: [],
      },
    },
  ]);

  [, , inStatus] = await new TRRDNSListener("loop.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Alias form for .
  await trrServer.registerDoHAnswers("empty.com", "A", [
    {
      name: "empty.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: "", // This is not allowed
        values: [],
      },
    },
  ]);

  [, , inStatus] = await new TRRDNSListener("empty.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // We should ignore ServiceForm if an AliasForm record is also present
  await trrServer.registerDoHAnswers("multi.com", "HTTPS", [
    {
      name: "multi.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "h3pool",
        values: [
          { key: "alpn", value: "h2,h3" },
          { key: "no-default-alpn" },
          { key: "port", value: 8888 },
          { key: "ipv4hint", value: "1.2.3.4" },
          { key: "echconfig", value: "123..." },
          { key: "ipv6hint", value: "::1" },
        ],
      },
    },
    {
      name: "multi.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: "example.com",
        values: [],
      },
    },
  ]);

  let listener = new DNSListener();
  let request = dns.asyncResolve(
    "multi.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(
    !Components.isSuccessCode(inStatus2),
    `${inStatus2} should be an error code`
  );

  // the svcparam keys are in reverse order
  await trrServer.registerDoHAnswers("order.com", "HTTPS", [
    {
      name: "order.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "h3pool",
        values: [
          { key: "ipv6hint", value: "::1" },
          { key: "echconfig", value: "123..." },
          { key: "ipv4hint", value: "1.2.3.4" },
          { key: "port", value: 8888 },
          { key: "no-default-alpn" },
          { key: "alpn", value: "h2,h3" },
        ],
      },
    },
  ]);

  listener = new DNSListener();
  request = dns.asyncResolve(
    "order.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(
    !Components.isSuccessCode(inStatus2),
    `${inStatus2} should be an error code`
  );

  // duplicate svcparam keys
  await trrServer.registerDoHAnswers("duplicate.com", "HTTPS", [
    {
      name: "duplicate.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "h3pool",
        values: [
          { key: "alpn", value: "h2,h3" },
          { key: "alpn", value: "h2,h3,h4" },
        ],
      },
    },
  ]);

  listener = new DNSListener();
  request = dns.asyncResolve(
    "duplicate.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(
    !Components.isSuccessCode(inStatus2),
    `${inStatus2} should be an error code`
  );

  // mandatory svcparam
  await trrServer.registerDoHAnswers("mandatory.com", "HTTPS", [
    {
      name: "mandatory.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "h3pool",
        values: [
          { key: "mandatory", value: ["key100"] },
          { key: "alpn", value: "h2,h3" },
          { key: "key100" },
        ],
      },
    },
  ]);

  listener = new DNSListener();
  request = dns.asyncResolve(
    "mandatory.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(!Components.isSuccessCode(inStatus2), `${inStatus2} should fail`);

  // mandatory svcparam
  await trrServer.registerDoHAnswers("mandatory2.com", "HTTPS", [
    {
      name: "mandatory2.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "h3pool",
        values: [
          {
            key: "mandatory",
            value: [
              "alpn",
              "no-default-alpn",
              "port",
              "ipv4hint",
              "echconfig",
              "ipv6hint",
            ],
          },
          { key: "alpn", value: "h2,h3" },
          { key: "no-default-alpn" },
          { key: "port", value: 8888 },
          { key: "ipv4hint", value: "1.2.3.4" },
          { key: "echconfig", value: "123..." },
          { key: "ipv6hint", value: "::1" },
        ],
      },
    },
  ]);

  listener = new DNSListener();
  request = dns.asyncResolve(
    "mandatory2.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(Components.isSuccessCode(inStatus2), `${inStatus2} should succeed`);

  // alias-mode with . targetName
  await trrServer.registerDoHAnswers("no-alias.com", "HTTPS", [
    {
      name: "no-alias.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 0,
        name: ".",
        values: [],
      },
    },
  ]);

  listener = new DNSListener();
  request = dns.asyncResolve(
    "no-alias.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(!Components.isSuccessCode(inStatus2), `${inStatus2} should fail`);

  // service-mode with . targetName
  await trrServer.registerDoHAnswers("service.com", "HTTPS", [
    {
      name: "service.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: ".",
        values: [{ key: "alpn", value: "h2,h3" }],
      },
    },
  ]);

  listener = new DNSListener();
  request = dns.asyncResolve(
    "service.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus2] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.ok(Components.isSuccessCode(inStatus2), `${inStatus2} should work`);
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "service.com");
});
