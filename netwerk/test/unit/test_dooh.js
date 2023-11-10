/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from trr_common.js */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

let httpServer;
let ohttpServer;
let ohttpEncodedConfig = "not a valid config";

// Decapsulate the request, send it to the actual TRR, receive the response,
// encapsulate it, and send it back through `response`.
async function forwardToTRR(request, response) {
  let inputStream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  inputStream.init(request.bodyInputStream);
  let requestBody = inputStream.readBytes(inputStream.available());
  let ohttpResponse = ohttpServer.decapsulate(stringToBytes(requestBody));
  let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
    Ci.nsIBinaryHttp
  );
  let decodedRequest = bhttp.decodeRequest(ohttpResponse.request);
  let headers = {};
  for (
    let i = 0;
    i < decodedRequest.headerNames.length && decodedRequest.headerValues.length;
    i++
  ) {
    headers[decodedRequest.headerNames[i]] = decodedRequest.headerValues[i];
  }
  let uri = `${decodedRequest.scheme}://${decodedRequest.authority}${decodedRequest.path}`;
  let body = new Uint8Array(decodedRequest.content.length);
  for (let i = 0; i < decodedRequest.content.length; i++) {
    body[i] = decodedRequest.content[i];
  }
  try {
    // Timeout after 10 seconds.
    let fetchInProgress = true;
    let controller = new AbortController();
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(() => {
      if (fetchInProgress) {
        controller.abort();
      }
    }, 10000);
    let trrResponse = await fetch(uri, {
      method: decodedRequest.method,
      headers,
      body: decodedRequest.method == "POST" ? body : undefined,
      credentials: "omit",
      signal: controller.signal,
    });
    fetchInProgress = false;
    let data = new Uint8Array(await trrResponse.arrayBuffer());
    let trrResponseContent = [];
    for (let i = 0; i < data.length; i++) {
      trrResponseContent.push(data[i]);
    }
    let trrResponseHeaderNames = [];
    let trrResponseHeaderValues = [];
    for (let header of trrResponse.headers) {
      trrResponseHeaderNames.push(header[0]);
      trrResponseHeaderValues.push(header[1]);
    }
    let binaryResponse = new BinaryHttpResponse(
      trrResponse.status,
      trrResponseHeaderNames,
      trrResponseHeaderValues,
      trrResponseContent
    );
    let responseBytes = bhttp.encodeResponse(binaryResponse);
    let encResponse = ohttpResponse.encapsulate(responseBytes);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "message/ohttp-res", false);
    response.write(bytesToString(encResponse));
  } catch (e) {
    // Some tests involve the responder either timing out or closing the
    // connection unexpectedly.
  }
}

add_setup(async function setup() {
  h2Port = trr_test_setup();
  runningOHTTPTests = true;

  if (mozinfo.socketprocess_networking) {
    Services.dns; // Needed to trigger socket process.
    await TestUtils.waitForCondition(() => Services.io.socketProcessLaunched);
  }

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);

  let ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
    Ci.nsIObliviousHttp
  );
  ohttpServer = ohttp.server();

  httpServer = new HttpServer();
  httpServer.registerPathHandler("/relay", function (request, response) {
    response.processAsync();
    forwardToTRR(request, response).then(() => {
      response.finish();
    });
  });
  httpServer.registerPathHandler("/config", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/ohttp-keys", false);
    response.write(ohttpEncodedConfig);
  });
  httpServer.start(-1);

  Services.prefs.setBoolPref("network.trr.use_ohttp", true);
  // On windows the TTL fetch will race with clearing the cache
  // to refresh the cache entry.
  Services.prefs.setBoolPref("network.dns.get-ttl", false);

  registerCleanupFunction(async () => {
    trr_clear_prefs();
    Services.prefs.clearUserPref("network.trr.use_ohttp");
    Services.prefs.clearUserPref("network.trr.ohttp.config_uri");
    Services.prefs.clearUserPref("network.trr.ohttp.relay_uri");
    Services.prefs.clearUserPref("network.trr.ohttp.uri");
    Services.prefs.clearUserPref("network.dns.get-ttl");
    await new Promise((resolve, reject) => {
      httpServer.stop(resolve);
    });
  });
});

// Test that if DNS-over-OHTTP isn't configured, the implementation falls back
// to platform resolution.
add_task(async function test_ohttp_not_configured() {
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await new TRRDNSListener("example.com", "127.0.0.1");
});

add_task(async function set_ohttp_invalid_prefs() {
  let configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.prefs.setCharPref(
    "network.trr.ohttp.relay_uri",
    "http://nonexistent.test"
  );
  Services.prefs.setCharPref(
    "network.trr.ohttp.config_uri",
    "http://nonexistent.test"
  );

  Cc["@mozilla.org/network/oblivious-http-service;1"].getService(
    Ci.nsIObliviousHttpService
  );
  await configPromise;
});

// Test that if DNS-over-OHTTP has an invalid configuration, the implementation
// falls back to platform resolution.
add_task(async function test_ohttp_invalid_prefs_fallback() {
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await new TRRDNSListener("example.com", "127.0.0.1");
});

add_task(async function set_ohttp_prefs_500_error() {
  let configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.prefs.setCharPref(
    "network.trr.ohttp.relay_uri",
    `http://localhost:${httpServer.identity.primaryPort}/relay`
  );
  Services.prefs.setCharPref(
    "network.trr.ohttp.config_uri",
    `http://localhost:${httpServer.identity.primaryPort}/500error`
  );
  await configPromise;
});

// Test that if DNS-over-OHTTP has an invalid configuration, the implementation
// falls back to platform resolution.
add_task(async function test_ohttp_500_error_fallback() {
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await new TRRDNSListener("example.com", "127.0.0.1");
});

add_task(async function retryConfigOnConnectivityChange() {
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  // First we make sure the config is properly loaded
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  let ohttpService = Cc[
    "@mozilla.org/network/oblivious-http-service;1"
  ].getService(Ci.nsIObliviousHttpService);
  ohttpService.clearTRRConfig();
  ohttpEncodedConfig = bytesToString(ohttpServer.encodedConfig);
  let configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.prefs.setCharPref(
    "network.trr.ohttp.relay_uri",
    `http://localhost:${httpServer.identity.primaryPort}/relay`
  );
  Services.prefs.setCharPref(
    "network.trr.ohttp.config_uri",
    `http://localhost:${httpServer.identity.primaryPort}/config`
  );
  let [, status] = await configPromise;
  equal(status, "success");
  info("retryConfigOnConnectivityChange setup complete");

  ohttpService.clearTRRConfig();

  let port = httpServer.identity.primaryPort;
  // Stop the server so getting the config fails.
  await httpServer.stop();

  configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.obs.notifyObservers(
    null,
    "network:captive-portal-connectivity-changed"
  );
  [, status] = await configPromise;
  equal(status, "failed");

  // Should fallback to native DNS since the config is empty
  Services.dns.clearCache(true);
  await new TRRDNSListener("example.com", "127.0.0.1");

  // Start the server back again.
  httpServer.start(port);
  Assert.equal(
    port,
    httpServer.identity.primaryPort,
    "server should get the same port"
  );

  // Still the config hasn't been reloaded.
  await new TRRDNSListener("example2.com", "127.0.0.1");

  // Signal a connectivity change so we reload the config
  configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.obs.notifyObservers(
    null,
    "network:captive-portal-connectivity-changed"
  );
  [, status] = await configPromise;
  equal(status, "success");

  await new TRRDNSListener("example3.com", "2.2.2.2");

  // Now check that we also reload a missing config if a TRR confirmation fails.
  ohttpService.clearTRRConfig();
  configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.obs.notifyObservers(
    null,
    "network:trr-confirmation",
    "CONFIRM_FAILED"
  );
  [, status] = await configPromise;
  equal(status, "success");
  await new TRRDNSListener("example4.com", "2.2.2.2");

  // set the config to an invalid value and check that as long as the URL
  // doesn't change, we dont reload it again on connectivity notifications.
  ohttpEncodedConfig = "not a valid config";
  configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.obs.notifyObservers(
    null,
    "network:captive-portal-connectivity-changed"
  );

  await new TRRDNSListener("example5.com", "2.2.2.2");

  // The change should not cause any config reload because we already have a config.
  [, status] = await configPromise;
  equal(status, "no-changes");

  await new TRRDNSListener("example6.com", "2.2.2.2");
  // Clear the config_uri pref so it gets set to the proper value in the next test.
  configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  Services.prefs.setCharPref("network.trr.ohttp.config_uri", ``);
  await configPromise;
});

add_task(async function set_ohttp_prefs_valid() {
  let ohttpService = Cc[
    "@mozilla.org/network/oblivious-http-service;1"
  ].getService(Ci.nsIObliviousHttpService);
  ohttpService.clearTRRConfig();
  let configPromise = TestUtils.topicObserved("ohttp-service-config-loaded");
  ohttpEncodedConfig = bytesToString(ohttpServer.encodedConfig);
  Services.prefs.setCharPref(
    "network.trr.ohttp.config_uri",
    `http://localhost:${httpServer.identity.primaryPort}/config`
  );
  await configPromise;
});

add_task(test_A_record);

add_task(test_AAAA_records);

add_task(test_RFC1918);

add_task(test_GET_ECS);

add_task(test_timeout_mode3);

add_task(test_strict_native_fallback);

add_task(test_no_answers_fallback);

add_task(test_404_fallback);

add_task(test_mode_1_and_4);

add_task(test_CNAME);

add_task(test_name_mismatch);

add_task(test_mode_2);

add_task(test_excluded_domains);

add_task(test_captiveportal_canonicalURL);

add_task(test_parentalcontrols);

// TRR-first check that DNS result is used if domain is part of the builtin-excluded-domains pref
add_task(test_builtin_excluded_domains);

add_task(test_excluded_domains_mode3);

add_task(test25e);

add_task(test_parentalcontrols_mode3);

add_task(test_builtin_excluded_domains_mode3);

add_task(count_cookies);

// This test doesn't work with having a JS httpd server as a relay.
// add_task(test_connection_closed);

add_task(test_fetch_time);

add_task(test_fqdn);

add_task(test_ipv6_trr_fallback);

add_task(test_ipv4_trr_fallback);

add_task(test_no_retry_without_doh);
