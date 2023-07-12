/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

// Trigger a proper telemetry init.
do_get_profile(true);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

// setup and configure a proxy server that will just deny connections.
const proxy = AddonTestUtils.createHttpServer();
proxy.registerPrefixHandler("/", (request, response) => {
  response.setStatusLine(request.httpVersion, 504, "hello proxy user");
  response.write("ok!");
});

// Register a proxy to be used by TCPSocket connections later.
let proxy_info;

function getBadProxyPort() {
  let server = new HttpServer();
  server.start(-1);
  const badPort = server.identity.primaryPort;
  server.stop();
  return badPort;
}

function registerProxy() {
  let pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService(
    Ci.nsIProtocolProxyService
  );

  const proxyFilter = {
    applyFilter(uri, defaultProxyInfo, callback) {
      if (
        proxy_info &&
        uri.host == PingServer.host &&
        uri.port == PingServer.port
      ) {
        let proxyInfo = pps.newProxyInfo(
          proxy_info.type,
          proxy_info.host,
          proxy_info.port,
          "",
          "",
          0,
          4096,
          null
        );
        proxyInfo.sourceId = proxy_info.sourceId;
        callback.onProxyFilterResult(proxyInfo);
      } else {
        callback.onProxyFilterResult(defaultProxyInfo);
      }
    },
  };

  pps.registerFilter(proxyFilter, 0);
  registerCleanupFunction(() => {
    pps.unregisterFilter(proxyFilter);
  });
}

add_task(async function setup() {
  fakeIntlReady();

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.HealthPingEnabled,
    false
  );
  TelemetryStopwatch.setTestModeEnabled(true);

  registerProxy();

  PingServer.start();

  // accept proxy connections for PingServer
  proxy.identity.add("http", PingServer.host, PingServer.port);

  await TelemetrySend.setup(true);
  TelemetrySend.setTestModeEnabled(true);
  TelemetrySend.setServer(`http://localhost:${PingServer.port}`);
});

function checkEvent() {
  // ServiceRequest should have recorded an event for this.
  let expected = [
    "service_request",
    "bypass",
    "proxy_info",
    "telemetry.send",
    {
      source: proxy_info.sourceId,
      type: "api",
    },
  ];
  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_ALL_CHANNELS,
    false
  );

  let received = snapshot.parent[0];
  received.shift();
  Assert.deepEqual(
    expected,
    received,
    `retry telemetry data matched ${JSON.stringify(received)}`
  );
  Telemetry.clearEvents();
}

async function submitPingWithDate(date, expected) {
  fakeNow(new Date(date));
  let pingId = await TelemetryController.submitExternalPing(
    "test-send-date-header",
    {}
  );
  let req = await PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);
  Assert.equal(
    req.getHeader("Date"),
    expected,
    "Telemetry should send the correct Date header with requests."
  );
  Assert.equal(ping.id, pingId, "Should have received the correct ping id.");
}

// While there is no specific indiction, this test causes the
// telemetrySend doPing onload handler to be invoked.
add_task(async function test_failed_server() {
  proxy_info = {
    type: "http",
    host: proxy.identity.primaryHost,
    port: proxy.identity.primaryPort,
    sourceId: "failed_server_test",
  };

  await TelemetrySend.reset();
  await submitPingWithDate(
    Date.UTC(2011, 1, 1, 11, 0, 0),
    "Tue, 01 Feb 2011 11:00:00 GMT"
  );
  checkEvent();
});

// While there is no specific indiction, this test causes the
// telemetrySend doPing error handler to be invoked.
add_task(async function test_no_server() {
  // Make sure the underlying proxy failover is disabled to easily force
  // telemetry to retry the request.
  Services.prefs.setBoolPref("network.proxy.failover_direct", false);

  proxy_info = {
    type: "http",
    host: "localhost",
    port: getBadProxyPort(),
    sourceId: "no_server_test",
  };

  await TelemetrySend.reset();
  await submitPingWithDate(
    Date.UTC(2012, 1, 1, 11, 0, 0),
    "Wed, 01 Feb 2012 11:00:00 GMT"
  );
  checkEvent();
});

// Mock out the send timer activity.
function waitForTimer() {
  return new Promise(resolve => {
    fakePingSendTimer(
      (callback, timeout) => {
        resolve([callback, timeout]);
      },
      () => {}
    );
  });
}

add_task(async function test_no_bypass() {
  // Make sure the underlying proxy failover is disabled to easily force
  // telemetry to retry the request.
  Services.prefs.setBoolPref("network.proxy.failover_direct", false);
  // Disable the retry and submit again.
  Services.prefs.setBoolPref("network.proxy.allow_bypass", false);

  proxy_info = {
    type: "http",
    host: "localhost",
    port: getBadProxyPort(),
    sourceId: "no_server_test",
  };

  await TelemetrySend.reset();

  fakeNow(new Date(Date.UTC(2013, 1, 1, 11, 0, 0)));

  let timerPromise = waitForTimer();
  let pingId = await TelemetryController.submitExternalPing(
    "test-send-date-header",
    {}
  );
  let [pingSendTimerCallback] = await timerPromise;
  Assert.ok(!!pingSendTimerCallback, "Should have a timer callback");

  Assert.equal(
    TelemetrySend.pendingPingCount,
    1,
    "Should have correct pending ping count"
  );

  // Reset the proxy, trigger the next tick - we should receive the ping.
  proxy_info = null;
  pingSendTimerCallback();
  let req = await PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);

  // PingServer finished before telemetry, so make sure it's done.
  await TelemetrySend.testWaitOnOutgoingPings();

  Assert.equal(
    req.getHeader("Date"),
    "Fri, 01 Feb 2013 11:00:00 GMT",
    "Telemetry should send the correct Date header with requests."
  );
  Assert.equal(ping.id, pingId, "Should have received the correct ping id.");

  // reset to save any pending pings
  Assert.equal(
    TelemetrySend.pendingPingCount,
    0,
    "Should not have any pending pings"
  );

  await TelemetrySend.reset();
  PingServer.stop();
});
