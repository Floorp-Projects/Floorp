/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let h2Port;
let trrServer;

function inChildProcess() {
  return (
    Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
      .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  );
}

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

function setup() {
  trr_test_setup();
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
}

if (!inChildProcess()) {
  setup();
  registerCleanupFunction(async () => {
    trr_clear_prefs();
    await trrServer.stop();
  });
}

add_task(async function testHTTPSSVC() {
  // use the h2 server as DOH provider
  if (!inChildProcess()) {
    Services.prefs.setCharPref(
      "network.trr.uri",
      "https://foo.example.com:" + h2Port + "/httpssvc"
    );
  }

  let [, inRecord] = await new TRRDNSListener("test.httpssvc.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "h3pool");
  Assert.equal(answer[0].values.length, 7);
  Assert.deepEqual(
    answer[0].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    ["h2", "h3"],
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
  Assert.equal(
    answer[0].values[6].QueryInterface(Ci.nsISVCParamODoHConfig).ODoHConfig,
    "456...",
    "got correct answer"
  );
  Assert.equal(answer[1].priority, 2);
  Assert.equal(answer[1].name, "test.httpssvc.com");
  Assert.equal(answer[1].values.length, 5);
  Assert.deepEqual(
    answer[1].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    ["h2"],
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
  Assert.equal(
    answer[1].values[4].QueryInterface(Ci.nsISVCParamODoHConfig).ODoHConfig,
    "def...",
    "got correct answer"
  );
  Assert.equal(answer[2].priority, 3);
  Assert.equal(answer[2].name, "hello");
  Assert.equal(answer[2].values.length, 0);
});

add_task(async function test_aliasform() {
  trrServer = new TRRServer();
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
  await trrServer.registerDoHAnswers("test.com", "A", {
    answers: [
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
    ],
  });
  await trrServer.registerDoHAnswers("something.com", "A", {
    answers: [
      {
        name: "something.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });

  await new TRRDNSListener("test.com", { expectedAnswer: "1.2.3.4" });

  // Test a chain of HTTPSSVC AliasForm and CNAMEs
  await trrServer.registerDoHAnswers("x.com", "A", {
    answers: [
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
    ],
  });
  await trrServer.registerDoHAnswers("y.com", "A", {
    answers: [
      {
        name: "y.com",
        type: "CNAME",
        ttl: 55,
        class: "IN",
        flush: false,
        data: "z.com",
      },
    ],
  });
  await trrServer.registerDoHAnswers("z.com", "A", {
    answers: [
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
    ],
  });
  await trrServer.registerDoHAnswers("target.com", "A", {
    answers: [
      {
        name: "target.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "4.3.2.1",
      },
    ],
  });

  await new TRRDNSListener("x.com", { expectedAnswer: "4.3.2.1" });

  // We get a ServiceForm instead of a A answer, CNAME or AliasForm
  await trrServer.registerDoHAnswers("no-ip-host.com", "A", {
    answers: [
      {
        name: "no-ip-host.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "h3pool",
          values: [
            { key: "alpn", value: ["h2", "h3"] },
            { key: "no-default-alpn" },
            { key: "port", value: 8888 },
            { key: "ipv4hint", value: "1.2.3.4" },
            { key: "echconfig", value: "123..." },
            { key: "ipv6hint", value: "::1" },
          ],
        },
      },
    ],
  });

  let [, , inStatus] = await new TRRDNSListener("no-ip-host.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Test CNAME/AliasForm loop
  await trrServer.registerDoHAnswers("loop.com", "A", {
    answers: [
      {
        name: "loop.com",
        type: "CNAME",
        ttl: 55,
        class: "IN",
        flush: false,
        data: "loop2.com",
      },
    ],
  });
  await trrServer.registerDoHAnswers("loop2.com", "A", {
    answers: [
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
    ],
  });

  [, , inStatus] = await new TRRDNSListener("loop.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Alias form for .
  await trrServer.registerDoHAnswers("empty.com", "A", {
    answers: [
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
    ],
  });

  [, , inStatus] = await new TRRDNSListener("empty.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // We should ignore ServiceForm if an AliasForm record is also present
  await trrServer.registerDoHAnswers("multi.com", "HTTPS", {
    answers: [
      {
        name: "multi.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "h3pool",
          values: [
            { key: "alpn", value: ["h2", "h3"] },
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
    ],
  });

  let [, , inStatus2] = await new TRRDNSListener("multi.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus2),
    `${inStatus2} should be an error code`
  );

  // the svcparam keys are in reverse order
  await trrServer.registerDoHAnswers("order.com", "HTTPS", {
    answers: [
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
            { key: "alpn", value: ["h2", "h3"] },
          ],
        },
      },
    ],
  });

  [, , inStatus2] = await new TRRDNSListener("order.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus2),
    `${inStatus2} should be an error code`
  );

  // duplicate svcparam keys
  await trrServer.registerDoHAnswers("duplicate.com", "HTTPS", {
    answers: [
      {
        name: "duplicate.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "h3pool",
          values: [
            { key: "alpn", value: ["h2", "h3"] },
            { key: "alpn", value: ["h2", "h3", "h4"] },
          ],
        },
      },
    ],
  });

  [, , inStatus2] = await new TRRDNSListener("duplicate.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus2),
    `${inStatus2} should be an error code`
  );

  // mandatory svcparam
  await trrServer.registerDoHAnswers("mandatory.com", "HTTPS", {
    answers: [
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
            { key: "alpn", value: ["h2", "h3"] },
            { key: "key100" },
          ],
        },
      },
    ],
  });

  [, , inStatus2] = await new TRRDNSListener("mandatory.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });
  Assert.ok(!Components.isSuccessCode(inStatus2), `${inStatus2} should fail`);

  // mandatory svcparam
  await trrServer.registerDoHAnswers("mandatory2.com", "HTTPS", {
    answers: [
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
            { key: "alpn", value: ["h2", "h3"] },
            { key: "no-default-alpn" },
            { key: "port", value: 8888 },
            { key: "ipv4hint", value: "1.2.3.4" },
            { key: "echconfig", value: "123..." },
            { key: "ipv6hint", value: "::1" },
          ],
        },
      },
    ],
  });

  [, , inStatus2] = await new TRRDNSListener("mandatory2.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });

  Assert.ok(Components.isSuccessCode(inStatus2), `${inStatus2} should succeed`);

  // alias-mode with . targetName
  await trrServer.registerDoHAnswers("no-alias.com", "HTTPS", {
    answers: [
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
    ],
  });

  [, , inStatus2] = await new TRRDNSListener("no-alias.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });

  Assert.ok(!Components.isSuccessCode(inStatus2), `${inStatus2} should fail`);

  // service-mode with . targetName
  await trrServer.registerDoHAnswers("service.com", "HTTPS", {
    answers: [
      {
        name: "service.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: ".",
          values: [{ key: "alpn", value: ["h2", "h3"] }],
        },
      },
    ],
  });

  let inRecord;
  [, inRecord, inStatus2] = await new TRRDNSListener("service.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });
  Assert.ok(Components.isSuccessCode(inStatus2), `${inStatus2} should work`);
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "service.com");
});

add_task(async function testNegativeResponse() {
  let [, , inStatus] = await new TRRDNSListener("negative_test.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  await trrServer.registerDoHAnswers("negative_test.com", "HTTPS", {
    answers: [
      {
        name: "negative_test.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "negative_test.com",
          values: [{ key: "alpn", value: ["h2", "h3"] }],
        },
      },
    ],
  });

  // Should still be failed because a negative response is from DNS cache.
  [, , inStatus] = await new TRRDNSListener("negative_test.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  if (inChildProcess()) {
    do_send_remote_message("clearCache");
    await do_await_remote_message("clearCache-done");
  } else {
    dns.clearCache(true);
  }

  let inRecord;
  [, inRecord, inStatus] = await new TRRDNSListener("negative_test.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });
  Assert.ok(Components.isSuccessCode(inStatus), `${inStatus} should work`);
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "negative_test.com");
  await trrServer.stop();
});
