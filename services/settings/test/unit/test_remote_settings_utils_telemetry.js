/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");

const server = new HttpServer();
server.start(-1);
registerCleanupFunction(() => server.stop(() => {}));
const SERVER_BASE_URL = `http://localhost:${server.identity.primaryPort}`;

const proxyServer = new HttpServer();
proxyServer.start(-1);
const PROXY_PORT = proxyServer.identity.primaryPort;
proxyServer.stop();

server.registerPathHandler("/destination", (request, response) => {
  response.setStatusLine(null, 412);
});

async function assertTelemetryEvents(expectedEvents) {
  await TelemetryTestUtils.assertEvents(expectedEvents, {
    category: "service_request",
    method: "bypass",
  });
}

add_task(async function setup() {
  await TelemetryController.testSetup();
});

add_task(async function test_telemetry() {
  const DESTINATION_URL = `${SERVER_BASE_URL}/destination`;

  {
    let res = await Utils.fetch(DESTINATION_URL);
    Assert.equal(res.status, 412, "fetch without proxy succeeded");
  }
  await assertTelemetryEvents([]);

  Services.prefs.setIntPref("network.proxy.type", 1);
  Services.prefs.setStringPref("network.proxy.http", "127.0.0.1");
  Services.prefs.setIntPref("network.proxy.http_port", PROXY_PORT);
  Services.prefs.setBoolPref("network.proxy.allow_hijacking_localhost", true);

  {
    let res = await Utils.fetch(DESTINATION_URL);
    Assert.equal(res.status, 412, "fetch with broken proxy succeeded");
  }
  // Note: failover handled by HttpChannel, hence no failover here.
  await assertTelemetryEvents([]);

  // Disable HttpChannel failover in favor of Utils.fetch's implementation.
  Services.prefs.setBoolPref("network.proxy.failover_direct", false);
  {
    let res = await Utils.fetch(DESTINATION_URL);
    Assert.equal(res.status, 412, "fetch succeeded with bypassProxy feature");
  }
  await assertTelemetryEvents([
    {
      category: "service_request",
      method: "bypass",
      object: "proxy_info",
      value: "remote-settings",
      extra: {
        source: "prefs",
        type: "manual",
      },
    },
  ]);

  Services.prefs.setBoolPref("network.proxy.allow_bypass", false);
  await Assert.rejects(
    Utils.fetch(DESTINATION_URL),
    /NetworkError/,
    "Request without failover fails"
  );
  await assertTelemetryEvents([]);
});
