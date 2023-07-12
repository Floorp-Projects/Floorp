/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_trr.js */
/* import-globals-from head_http3.js */

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const TRR_Domain = "foo.example.com";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

const gOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

async function SetParentalControlEnabled(aEnabled) {
  let parentalControlsService = {
    parentalControlsEnabled: aEnabled,
    QueryInterface: ChromeUtils.generateQI(["nsIParentalControlsService"]),
  };
  let cid = MockRegistrar.register(
    "@mozilla.org/parental-controls-service;1",
    parentalControlsService
  );
  Services.dns.reloadParentalControlEnabled();
  MockRegistrar.unregister(cid);
}

let runningOHTTPTests = false;
let h2Port;

function setModeAndURIForODoH(mode, path) {
  Services.prefs.setIntPref("network.trr.mode", mode);
  if (path.substr(0, 4) == "doh?") {
    path = path.replace("doh?", "odoh?");
  }

  Services.prefs.setCharPref("network.trr.odoh.target_path", `${path}`);
}

function setModeAndURIForOHTTP(mode, path, domain) {
  Services.prefs.setIntPref("network.trr.mode", mode);
  if (domain) {
    Services.prefs.setCharPref(
      "network.trr.ohttp.uri",
      `https://${domain}:${h2Port}/${path}`
    );
  } else {
    Services.prefs.setCharPref(
      "network.trr.ohttp.uri",
      `https://${TRR_Domain}:${h2Port}/${path}`
    );
  }
}

function setModeAndURI(mode, path, domain) {
  if (runningOHTTPTests) {
    setModeAndURIForOHTTP(mode, path, domain);
  } else {
    Services.prefs.setIntPref("network.trr.mode", mode);
    if (domain) {
      Services.prefs.setCharPref(
        "network.trr.uri",
        `https://${domain}:${h2Port}/${path}`
      );
    } else {
      Services.prefs.setCharPref(
        "network.trr.uri",
        `https://${TRR_Domain}:${h2Port}/${path}`
      );
    }
  }
}

async function test_A_record() {
  info("Verifying a basic A record");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2"); // TRR-first
  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  info("Verifying a basic A record - without bootstrapping");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=3.3.3.3"); // TRR-only

  // Clear bootstrap address and add DoH endpoint hostname to local domains
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");
  Services.prefs.setCharPref("network.dns.localDomains", TRR_Domain);

  await new TRRDNSListener("bar.example.com", "3.3.3.3");

  Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");
  Services.prefs.clearUserPref("network.dns.localDomains");

  info("Verify that the cached record is used when DoH endpoint is down");
  // Don't clear the cache. That is what we're checking.
  setModeAndURI(3, "404");

  await new TRRDNSListener("bar.example.com", "3.3.3.3");
  info("verify working credentials in DOH request");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=4.4.4.4&auth=true");
  Services.prefs.setCharPref("network.trr.credentials", "user:password");

  await new TRRDNSListener("bar.example.com", "4.4.4.4");

  info("Verify failing credentials in DOH request");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=4.4.4.4&auth=true");
  Services.prefs.setCharPref("network.trr.credentials", "evil:person");

  let { inStatus } = await new TRRDNSListener(
    "wrong.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  Services.prefs.clearUserPref("network.trr.credentials");
}

async function test_AAAA_records() {
  info("Verifying AAAA record");

  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2020:2020::2020&delayIPv4=100");

  await new TRRDNSListener("aaaa.example.com", "2020:2020::2020");

  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2020:2020::2020&delayIPv6=100");

  await new TRRDNSListener("aaaa.example.com", "2020:2020::2020");

  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2020:2020::2020");

  await new TRRDNSListener("aaaa.example.com", "2020:2020::2020");
}

async function test_RFC1918() {
  info("Verifying that RFC1918 address from the server is rejected by default");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.168.0.1");

  let { inStatus } = await new TRRDNSListener(
    "rfc1918.example.com",
    undefined,
    false
  );

  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  setModeAndURI(3, "doh?responseIP=::ffff:192.168.0.1");
  ({ inStatus } = await new TRRDNSListener(
    "rfc1918-ipv6.example.com",
    undefined,
    false
  ));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  info("Verify RFC1918 address from the server is fine when told so");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.168.0.1");
  Services.prefs.setBoolPref("network.trr.allow-rfc1918", true);
  await new TRRDNSListener("rfc1918.example.com", "192.168.0.1");
  setModeAndURI(3, "doh?responseIP=::ffff:192.168.0.1");

  await new TRRDNSListener("rfc1918-ipv6.example.com", "::ffff:192.168.0.1");

  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
}

async function test_GET_ECS() {
  info("Verifying resolution via GET with ECS disabled");
  Services.dns.clearCache(true);
  // The template part should be discarded
  setModeAndURI(3, "doh{?dns}");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);

  await new TRRDNSListener("ecs.example.com", "5.5.5.5");

  info("Verifying resolution via GET with ECS enabled");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh");
  Services.prefs.setBoolPref("network.trr.disable-ECS", false);

  await new TRRDNSListener("get.example.com", "5.5.5.5");

  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
}

async function test_timeout_mode3() {
  info("Verifying that a short timeout causes failure with a slow server");
  Services.dns.clearCache(true);
  // First, mode 3.
  setModeAndURI(3, "doh?noResponse=true");
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);

  let { inStatus } = await new TRRDNSListener(
    "timeout.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Now for mode 2
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?noResponse=true");

  await new TRRDNSListener("timeout.example.com", "127.0.0.1"); // Should fallback

  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
}

async function test_trr_retry() {
  Services.dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);

  info("Test fallback to native");
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", false);
  setModeAndURI(2, "doh?noResponse=true");
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);

  await new TRRDNSListener("timeout.example.com", {
    expectedAnswer: "127.0.0.1",
  });

  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");

  info("Test Retry Success");
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", true);

  let chan = makeChan(
    `https://foo.example.com:${h2Port}/reset-doh-request-count`,
    Ci.nsIRequest.TRR_DISABLED_MODE
  );
  await new Promise(resolve =>
    chan.asyncOpen(new ChannelListener(resolve, null))
  );

  setModeAndURI(2, "doh?responseIP=2.2.2.2&retryOnDecodeFailure=true");
  await new TRRDNSListener("retry_ok.example.com", "2.2.2.2");

  info("Test Retry Failed");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&corruptedAnswer=true");
  await new TRRDNSListener("retry_ng.example.com", "127.0.0.1");
}

async function test_strict_native_fallback() {
  Services.dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.retry_on_recoverable_errors", true);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);

  info("First a timeout case");
  setModeAndURI(2, "doh?noResponse=true");
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);
  Services.prefs.setIntPref(
    "network.trr.strict_fallback_request_timeout_ms",
    10
  );

  Services.prefs.setBoolPref(
    "network.trr.strict_native_fallback_allow_timeouts",
    false
  );

  let { inStatus } = await new TRRDNSListener(
    "timeout.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  Services.dns.clearCache(true);
  await new TRRDNSListener("timeout.example.com", undefined, false);

  Services.dns.clearCache(true);
  Services.prefs.setBoolPref(
    "network.trr.strict_native_fallback_allow_timeouts",
    true
  );
  await new TRRDNSListener("timeout.example.com", {
    expectedAnswer: "127.0.0.1",
  });

  Services.prefs.setBoolPref(
    "network.trr.strict_native_fallback_allow_timeouts",
    false
  );

  info("Now a connection error");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  Services.prefs.clearUserPref(
    "network.trr.strict_fallback_request_timeout_ms"
  );
  ({ inStatus } = await new TRRDNSListener("closeme.com", undefined, false));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  info("Now a decode error");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&corruptedAnswer=true");
  ({ inStatus } = await new TRRDNSListener(
    "bar.example.com",
    undefined,
    false
  ));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  if (!mozinfo.socketprocess_networking) {
    // Confirmation state isn't passed cross-process.
    info("Now with confirmation failed - should fallback");
    Services.dns.clearCache(true);
    setModeAndURI(2, "doh?responseIP=2.2.2.2&corruptedAnswer=true");
    Services.prefs.setCharPref("network.trr.confirmationNS", "example.com");
    await TestUtils.waitForCondition(
      // 3 => CONFIRM_FAILED, 4 => CONFIRM_TRYING_FAILED
      () =>
        Services.dns.currentTrrConfirmationState == 3 ||
        Services.dns.currentTrrConfirmationState == 4,
      `Timed out waiting for confirmation failure. Currently ${Services.dns.currentTrrConfirmationState}`,
      1,
      5000
    );
    await new TRRDNSListener("bar.example.com", "127.0.0.1"); // Should fallback
  }

  info("Now a successful case.");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  if (!mozinfo.socketprocess_networking) {
    // Only need to reset confirmation state if we messed with it before.
    Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
    await TestUtils.waitForCondition(
      // 5 => CONFIRM_DISABLED
      () => Services.dns.currentTrrConfirmationState == 5,
      `Timed out waiting for confirmation disabled. Currently ${Services.dns.currentTrrConfirmationState}`,
      1,
      5000
    );
  }
  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  info("Now without strict fallback mode, timeout case");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?noResponse=true");
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);
  Services.prefs.setIntPref(
    "network.trr.strict_fallback_request_timeout_ms",
    10
  );
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);

  await new TRRDNSListener("timeout.example.com", "127.0.0.1"); // Should fallback

  info("Now a connection error");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  Services.prefs.clearUserPref(
    "network.trr.strict_fallback_request_timeout_ms"
  );
  await new TRRDNSListener("closeme.com", "127.0.0.1"); // Should fallback

  info("Now a decode error");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&corruptedAnswer=true");
  await new TRRDNSListener("bar.example.com", "127.0.0.1"); // Should fallback

  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  Services.prefs.clearUserPref(
    "network.trr.strict_fallback_request_timeout_ms"
  );
}

async function test_no_answers_fallback() {
  info("Verfiying that we correctly fallback to Do53 when no answers from DoH");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=none"); // TRR-first

  await new TRRDNSListener("confirm.example.com", "127.0.0.1");

  info("Now in strict mode - no fallback");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  Services.dns.clearCache(true);
  await new TRRDNSListener("confirm.example.com", "127.0.0.1");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
}

async function test_404_fallback() {
  info("Verfiying that we correctly fallback to Do53 when DoH sends 404");
  Services.dns.clearCache(true);
  setModeAndURI(2, "404"); // TRR-first

  await new TRRDNSListener("test404.example.com", "127.0.0.1");

  info("Now in strict mode - no fallback");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  Services.dns.clearCache(true);
  let { inStatus } = await new TRRDNSListener("test404.example.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
}

async function test_mode_1_and_4() {
  info("Verifying modes 1 and 4 are treated as TRR-off");
  for (let mode of [1, 4]) {
    Services.dns.clearCache(true);
    setModeAndURI(mode, "doh?responseIP=2.2.2.2");
    Assert.equal(
      Services.dns.currentTrrMode,
      5,
      "Effective TRR mode should be 5"
    );
  }
}

async function test_CNAME() {
  info("Checking that we follow a CNAME correctly");
  Services.dns.clearCache(true);
  // The dns-cname path alternates between sending us a CNAME pointing to
  // another domain, and an A record. If we follow the cname correctly, doing
  // a lookup with this path as the DoH URI should resolve to that A record.
  setModeAndURI(3, "dns-cname");

  await new TRRDNSListener("cname.example.com", "99.88.77.66");

  info("Verifying that we bail out when we're thrown into a CNAME loop");
  Services.dns.clearCache(true);
  // First mode 3.
  setModeAndURI(3, "doh?responseIP=none&cnameloop=true");

  let { inStatus } = await new TRRDNSListener(
    "test18.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Now mode 2.
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=none&cnameloop=true");

  await new TRRDNSListener("test20.example.com", "127.0.0.1"); // Should fallback

  info("Check that we correctly handle CNAME bundled with an A record");
  Services.dns.clearCache(true);
  // "dns-cname-a" path causes server to send a CNAME as well as an A record
  setModeAndURI(3, "dns-cname-a");

  await new TRRDNSListener("cname-a.example.com", "9.8.7.6");
}

async function test_name_mismatch() {
  info("Verify that records that don't match the requested name are rejected");
  Services.dns.clearCache(true);
  // Setting hostname param tells server to always send record for bar.example.com
  // regardless of what was requested.
  setModeAndURI(3, "doh?hostname=mismatch.example.com");

  let { inStatus } = await new TRRDNSListener(
    "bar.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
}

async function test_mode_2() {
  info("Checking that TRR result is used in mode 2");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");

  await new TRRDNSListener("bar.example.com", "192.192.192.192");

  info("Now in strict mode");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  Services.dns.clearCache(true);
  await new TRRDNSListener("bar.example.com", "192.192.192.192");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
}

async function test_excluded_domains() {
  info("Checking that Do53 is used for names in excluded-domains list");
  for (let strictMode of [true, false]) {
    info("Strict mode: " + strictMode);
    Services.prefs.setBoolPref(
      "network.trr.strict_native_fallback",
      strictMode
    );
    Services.dns.clearCache(true);
    setModeAndURI(2, "doh?responseIP=192.192.192.192");
    Services.prefs.setCharPref(
      "network.trr.excluded-domains",
      "bar.example.com"
    );

    await new TRRDNSListener("bar.example.com", "127.0.0.1"); // Do53 result

    Services.dns.clearCache(true);
    Services.prefs.setCharPref("network.trr.excluded-domains", "example.com");

    await new TRRDNSListener("bar.example.com", "127.0.0.1");

    Services.dns.clearCache(true);
    Services.prefs.setCharPref(
      "network.trr.excluded-domains",
      "foo.test.com, bar.example.com"
    );
    await new TRRDNSListener("bar.example.com", "127.0.0.1");

    Services.dns.clearCache(true);
    Services.prefs.setCharPref(
      "network.trr.excluded-domains",
      "bar.example.com, foo.test.com"
    );

    await new TRRDNSListener("bar.example.com", "127.0.0.1");

    Services.prefs.clearUserPref("network.trr.excluded-domains");
  }
}

function topicObserved(topic) {
  return new Promise(resolve => {
    let observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      observe(aSubject, aTopic, aData) {
        if (aTopic == topic) {
          Services.obs.removeObserver(observer, topic);
          resolve(aData);
        }
      },
    };
    Services.obs.addObserver(observer, topic);
  });
}

async function test_captiveportal_canonicalURL() {
  info("Check that captivedetect.canonicalURL is resolved via native DNS");
  for (let strictMode of [true, false]) {
    info("Strict mode: " + strictMode);
    Services.prefs.setBoolPref(
      "network.trr.strict_native_fallback",
      strictMode
    );
    Services.dns.clearCache(true);
    setModeAndURI(2, "doh?responseIP=2.2.2.2");

    const cpServer = new HttpServer();
    cpServer.registerPathHandler(
      "/cp",
      function handleRawData(request, response) {
        response.setHeader("Content-Type", "text/plain", false);
        response.setHeader("Cache-Control", "no-cache", false);
        response.bodyOutputStream.write("data", 4);
      }
    );
    cpServer.start(-1);
    cpServer.identity.setPrimary(
      "http",
      "detectportal.firefox.com",
      cpServer.identity.primaryPort
    );
    let cpPromise = topicObserved("captive-portal-login");

    Services.prefs.setCharPref(
      "captivedetect.canonicalURL",
      `http://detectportal.firefox.com:${cpServer.identity.primaryPort}/cp`
    );
    Services.prefs.setBoolPref("network.captive-portal-service.testMode", true);
    Services.prefs.setBoolPref("network.captive-portal-service.enabled", true);

    // The captive portal has to have used native DNS, otherwise creating
    // a socket to a non-local IP would trigger a crash.
    await cpPromise;
    // Simply resolving the captive portal domain should still use TRR
    await new TRRDNSListener("detectportal.firefox.com", "2.2.2.2");

    Services.prefs.clearUserPref("network.captive-portal-service.enabled");
    Services.prefs.clearUserPref("network.captive-portal-service.testMode");
    Services.prefs.clearUserPref("captivedetect.canonicalURL");

    await new Promise(resolve => cpServer.stop(resolve));
  }
}

async function test_parentalcontrols() {
  info("Check that DoH isn't used when parental controls are enabled");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);

  info("Now in strict mode");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);
}

async function test_builtin_excluded_domains() {
  info("Verifying Do53 is used for domains in builtin-excluded-domians list");
  for (let strictMode of [true, false]) {
    info("Strict mode: " + strictMode);
    Services.prefs.setBoolPref(
      "network.trr.strict_native_fallback",
      strictMode
    );
    Services.dns.clearCache(true);
    setModeAndURI(2, "doh?responseIP=2.2.2.2");

    Services.prefs.setCharPref("network.trr.excluded-domains", "");
    Services.prefs.setCharPref(
      "network.trr.builtin-excluded-domains",
      "bar.example.com"
    );
    await new TRRDNSListener("bar.example.com", "127.0.0.1");

    Services.dns.clearCache(true);
    Services.prefs.setCharPref(
      "network.trr.builtin-excluded-domains",
      "example.com"
    );
    await new TRRDNSListener("bar.example.com", "127.0.0.1");

    Services.dns.clearCache(true);
    Services.prefs.setCharPref(
      "network.trr.builtin-excluded-domains",
      "foo.test.com, bar.example.com"
    );
    await new TRRDNSListener("bar.example.com", "127.0.0.1");
    await new TRRDNSListener("foo.test.com", "127.0.0.1");
  }
}

async function test_excluded_domains_mode3() {
  info("Checking  Do53 is used for names in excluded-domains list in mode 3");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");

  await new TRRDNSListener("excluded", "192.192.192.192", true);

  Services.dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded");

  await new TRRDNSListener("excluded", "127.0.0.1");

  // Test .local
  Services.dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded,local");

  await new TRRDNSListener("test.local", "127.0.0.1");

  // Test .other
  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "excluded,local,other"
  );

  await new TRRDNSListener("domain.other", "127.0.0.1");
}

async function test25e() {
  info("Check captivedetect.canonicalURL is resolved via native DNS in mode 3");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");

  const cpServer = new HttpServer();
  cpServer.registerPathHandler(
    "/cp",
    function handleRawData(request, response) {
      response.setHeader("Content-Type", "text/plain", false);
      response.setHeader("Cache-Control", "no-cache", false);
      response.bodyOutputStream.write("data", 4);
    }
  );
  cpServer.start(-1);
  cpServer.identity.setPrimary(
    "http",
    "detectportal.firefox.com",
    cpServer.identity.primaryPort
  );
  let cpPromise = topicObserved("captive-portal-login");

  Services.prefs.setCharPref(
    "captivedetect.canonicalURL",
    `http://detectportal.firefox.com:${cpServer.identity.primaryPort}/cp`
  );
  Services.prefs.setBoolPref("network.captive-portal-service.testMode", true);
  Services.prefs.setBoolPref("network.captive-portal-service.enabled", true);

  // The captive portal has to have used native DNS, otherwise creating
  // a socket to a non-local IP would trigger a crash.
  await cpPromise;
  // // Simply resolving the captive portal domain should still use TRR
  await new TRRDNSListener("detectportal.firefox.com", "192.192.192.192");

  Services.prefs.clearUserPref("network.captive-portal-service.enabled");
  Services.prefs.clearUserPref("network.captive-portal-service.testMode");
  Services.prefs.clearUserPref("captivedetect.canonicalURL");

  await new Promise(resolve => cpServer.stop(resolve));
}

async function test_parentalcontrols_mode3() {
  info("Check DoH isn't used when parental controls are enabled in mode 3");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
}

async function test_builtin_excluded_domains_mode3() {
  info("Check Do53 used for domains in builtin-excluded-domians list, mode 3");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded"
  );

  await new TRRDNSListener("excluded", "127.0.0.1");

  // Test .local
  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded,local"
  );

  await new TRRDNSListener("test.local", "127.0.0.1");

  // Test .other
  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded,local,other"
  );

  await new TRRDNSListener("domain.other", "127.0.0.1");
}

async function count_cookies() {
  info("Check that none of the requests have set any cookies.");
  Assert.equal(Services.cookies.countCookiesFromHost("example.com"), 0);
  Assert.equal(Services.cookies.countCookiesFromHost("foo.example.com."), 0);
}

async function test_connection_closed() {
  info("Check we handle it correctly when the connection is closed");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2.2.2.2");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  // We don't need to wait for 30 seconds for the request to fail
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 500);
  // bootstrap
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  let { inStatus } = await new TRRDNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // No bootstrap this time
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  Services.dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded,local");
  Services.prefs.setCharPref("network.dns.localDomains", TRR_Domain);

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  ({ inStatus } = await new TRRDNSListener("closeme.com", undefined, false));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // No local domains either
  Services.dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  ({ inStatus } = await new TRRDNSListener("closeme.com", undefined, false));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // Now make sure that even in mode 3 without a bootstrap address
  // we are able to restart the TRR connection if it drops - the TRR service
  // channel will use regular DNS to resolve the TRR address.
  Services.dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  ({ inStatus } = await new TRRDNSListener("closeme.com", undefined, false));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  Services.dns.clearCache(true);
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // This test exists to document what happens when we're in TRR only mode
  // and we don't set a bootstrap address. We use DNS to resolve the
  // initial URI, but if the connection fails, we don't fallback to DNS
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=9.9.9.9");
  Services.prefs.setCharPref("network.dns.localDomains", "closeme.com");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new TRRDNSListener("bar.example.com", "9.9.9.9");

  // makes the TRR connection shut down. Should fallback to DNS
  await new TRRDNSListener("closeme.com", "127.0.0.1");
  // TRR should be back up again
  await new TRRDNSListener("bar2.example.com", "9.9.9.9");
}

async function test_fetch_time() {
  info("Verifying timing");
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&delayIPv4=20");

  await new TRRDNSListener("bar_time.example.com", "2.2.2.2", true, 20);

  // gets an error from DoH. It will fall back to regular DNS. The TRR timing should be 0.
  Services.dns.clearCache(true);
  setModeAndURI(2, "404&delayIPv4=20");

  await new TRRDNSListener("bar_time1.example.com", "127.0.0.1", true, 0);

  // check an excluded domain. It should fall back to regular DNS. The TRR timing should be 0.
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "bar_time2.example.com"
  );
  for (let strictMode of [true, false]) {
    info("Strict mode: " + strictMode);
    Services.prefs.setBoolPref(
      "network.trr.strict_native_fallback",
      strictMode
    );
    Services.dns.clearCache(true);
    setModeAndURI(2, "doh?responseIP=2.2.2.2&delayIPv4=20");
    await new TRRDNSListener("bar_time2.example.com", "127.0.0.1", true, 0);
  }

  Services.prefs.setCharPref("network.trr.excluded-domains", "");

  // verify RFC1918 address from the server is rejected and the TRR timing will be not set because the response will be from the native resolver.
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=192.168.0.1&delayIPv4=20");
  await new TRRDNSListener("rfc1918_time.example.com", "127.0.0.1", true, 0);
}

async function test_fqdn() {
  info("Test that we handle FQDN encoding and decoding properly");
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=9.8.7.6");

  await new TRRDNSListener("fqdn.example.org.", "9.8.7.6");

  // GET
  Services.dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.useGET", true);
  await new TRRDNSListener("fqdn_get.example.org.", "9.8.7.6");

  Services.prefs.clearUserPref("network.trr.useGET");
}

async function test_ipv6_trr_fallback() {
  info("Testing fallback with ipv6");
  Services.dns.clearCache(true);

  setModeAndURI(2, "doh?responseIP=4.4.4.4");
  const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
    Ci.nsINativeDNSResolverOverride
  );
  gOverride.addIPOverride("ipv6.host.com", "1:1::2");

  // Should not fallback to Do53 because A request for ipv6.host.com returns
  // 4.4.4.4
  let { inStatus } = await new TRRDNSListener("ipv6.host.com", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    expectedSuccess: false,
  });
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);

  // This time both requests fail, so we do fall back
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=none");
  await new TRRDNSListener("ipv6.host.com", "1:1::2");

  info("In strict mode, the lookup should fail when both reqs fail.");
  Services.dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  setModeAndURI(2, "doh?responseIP=none");
  await new TRRDNSListener("ipv6.host.com", "1:1::2");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);

  override.clearOverrides();
}

async function test_ipv4_trr_fallback() {
  info("Testing fallback with ipv4");
  Services.dns.clearCache(true);

  setModeAndURI(2, "doh?responseIP=1:2::3");
  const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
    Ci.nsINativeDNSResolverOverride
  );
  gOverride.addIPOverride("ipv4.host.com", "3.4.5.6");

  // Should not fallback to Do53 because A request for ipv4.host.com returns
  // 1:2::3
  let { inStatus } = await new TRRDNSListener("ipv4.host.com", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
    expectedSuccess: false,
  });
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);

  // This time both requests fail, so we do fall back
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=none");
  await new TRRDNSListener("ipv4.host.com", "3.4.5.6");

  // No fallback with strict mode.
  Services.dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  setModeAndURI(2, "doh?responseIP=none");
  await new TRRDNSListener("ipv4.host.com", "3.4.5.6");
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", false);

  override.clearOverrides();
}

async function test_no_retry_without_doh() {
  info("Bug 1648147 - if the TRR returns 0.0.0.0 we should not retry with DNS");
  Services.prefs.setBoolPref("network.trr.fallback-on-zero-response", false);

  async function test(url, ip) {
    setModeAndURI(2, `doh?responseIP=${ip}`);

    // Requests to 0.0.0.0 are usually directed to localhost, so let's use a port
    // we know isn't being used - 666 (Doom)
    let chan = makeChan(url, Ci.nsIRequest.TRR_DEFAULT_MODE);
    let statusCounter = {
      statusCount: {},
      QueryInterface: ChromeUtils.generateQI([
        "nsIInterfaceRequestor",
        "nsIProgressEventSink",
      ]),
      getInterface(iid) {
        return this.QueryInterface(iid);
      },
      onProgress(request, progress, progressMax) {},
      onStatus(request, status, statusArg) {
        this.statusCount[status] = 1 + (this.statusCount[status] || 0);
      },
    };
    chan.notificationCallbacks = statusCounter;
    await new Promise(resolve =>
      chan.asyncOpen(new ChannelListener(resolve, null, CL_EXPECT_FAILURE))
    );
    equal(
      statusCounter.statusCount[0x4b000b],
      1,
      "Expecting only one instance of NS_NET_STATUS_RESOLVED_HOST"
    );
    equal(
      statusCounter.statusCount[0x4b0007],
      1,
      "Expecting only one instance of NS_NET_STATUS_CONNECTING_TO"
    );
  }

  for (let strictMode of [true, false]) {
    info("Strict mode: " + strictMode);
    Services.prefs.setBoolPref(
      "network.trr.strict_native_fallback",
      strictMode
    );
    await test(`http://unknown.ipv4.stuff:666/path`, "0.0.0.0");
    await test(`http://unknown.ipv6.stuff:666/path`, "::");
  }
}

async function test_connection_reuse_and_cycling() {
  Services.dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 500);
  Services.prefs.setIntPref(
    "network.trr.strict_fallback_request_timeout_ms",
    500
  );
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 500);

  setModeAndURI(2, `doh?responseIP=9.8.7.6`);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);
  Services.prefs.setCharPref("network.trr.confirmationNS", "example.com");
  await TestUtils.waitForCondition(
    // 2 => CONFIRM_OK
    () => Services.dns.currentTrrConfirmationState == 2,
    `Timed out waiting for confirmation success. Currently ${Services.dns.currentTrrConfirmationState}`,
    1,
    5000
  );

  // Setting conncycle=true in the URI. Server will start logging reqs.
  // We will do a specific sequence of lookups, then fetch the log from
  // the server and check that it matches what we'd expect.
  setModeAndURI(2, `doh?responseIP=9.8.7.6&conncycle=true`);
  await TestUtils.waitForCondition(
    // 2 => CONFIRM_OK
    () => Services.dns.currentTrrConfirmationState == 2,
    `Timed out waiting for confirmation success. Currently ${Services.dns.currentTrrConfirmationState}`,
    1,
    5000
  );
  // Confirmation upon uri-change will have created one req.

  // Two reqs for each bar1 and bar2 - A + AAAA.
  await new TRRDNSListener("bar1.example.org.", "9.8.7.6");
  await new TRRDNSListener("bar2.example.org.", "9.8.7.6");
  // Total so far: (1) + 2 + 2 = 5

  // Two reqs that fail, one Confirmation req, two retried reqs that succeed.
  await new TRRDNSListener("newconn.example.org.", "9.8.7.6");
  await TestUtils.waitForCondition(
    // 2 => CONFIRM_OK
    () => Services.dns.currentTrrConfirmationState == 2,
    `Timed out waiting for confirmation success. Currently ${Services.dns.currentTrrConfirmationState}`,
    1,
    5000
  );
  // Total so far: (5) + 2 + 1 + 2 = 10

  // Two reqs for each bar3 and bar4 .
  await new TRRDNSListener("bar3.example.org.", "9.8.7.6");
  await new TRRDNSListener("bar4.example.org.", "9.8.7.6");
  // Total so far: (10) + 2 + 2 = 14.

  // Two reqs that fail, one Confirmation req, two retried reqs that succeed.
  await new TRRDNSListener("newconn2.example.org.", "9.8.7.6");
  await TestUtils.waitForCondition(
    // 2 => CONFIRM_OK
    () => Services.dns.currentTrrConfirmationState == 2,
    `Timed out waiting for confirmation success. Currently ${Services.dns.currentTrrConfirmationState}`,
    1,
    5000
  );
  // Total so far: (14) + 2 + 1 + 2 = 19

  // Two reqs for each bar5 and bar6 .
  await new TRRDNSListener("bar5.example.org.", "9.8.7.6");
  await new TRRDNSListener("bar6.example.org.", "9.8.7.6");
  // Total so far: (19) + 2 + 2 = 23

  let chan = makeChan(
    `https://foo.example.com:${h2Port}/get-doh-req-port-log`,
    Ci.nsIRequest.TRR_DISABLED_MODE
  );
  let dohReqPortLog = await new Promise(resolve =>
    chan.asyncOpen(
      new ChannelListener((stuff, buffer) => {
        resolve(JSON.parse(buffer));
      })
    )
  );

  // Since the actual ports seen will vary at runtime, we use placeholders
  // instead in our expected output definition. For example, if two entries
  // both have "port1", it means they both should have the same port in the
  // server's log.
  // For reqs that fail and trigger a Confirmation + retry, the retried reqs
  // might not re-use the new connection created for Confirmation due to a
  // race, so we have an extra alternate expected port for them. This lets
  // us test that they use *a* new port even if it's not *the* new port.
  // Subsequent lookups are not affected, they will use the same conn as
  // the Confirmation req.
  let expectedLogTemplate = [
    ["example.com", "port1"],
    ["bar1.example.org", "port1"],
    ["bar1.example.org", "port1"],
    ["bar2.example.org", "port1"],
    ["bar2.example.org", "port1"],
    ["newconn.example.org", "port1"],
    ["newconn.example.org", "port1"],
    ["example.com", "port2"],
    ["newconn.example.org", "port2"],
    ["newconn.example.org", "port2"],
    ["bar3.example.org", "port2"],
    ["bar3.example.org", "port2"],
    ["bar4.example.org", "port2"],
    ["bar4.example.org", "port2"],
    ["newconn2.example.org", "port2"],
    ["newconn2.example.org", "port2"],
    ["example.com", "port3"],
    ["newconn2.example.org", "port3"],
    ["newconn2.example.org", "port3"],
    ["bar5.example.org", "port3"],
    ["bar5.example.org", "port3"],
    ["bar6.example.org", "port3"],
    ["bar6.example.org", "port3"],
  ];

  if (expectedLogTemplate.length != dohReqPortLog.length) {
    // This shouldn't happen, and if it does, we'll fail the assertion
    // below. But first dump the whole server-side log to help with
    // debugging should we see a failure. Most likely cause would be
    // that another consumer of TRR happened to make a request while
    // the test was running and polluted the log.
    info(dohReqPortLog);
  }

  equal(
    expectedLogTemplate.length,
    dohReqPortLog.length,
    "Correct number of req log entries"
  );

  let seenPorts = new Set();
  // This is essentially a symbol table - as we iterate through the log
  // we will assign the actual seen port numbers to the placeholders.
  let seenPortsByExpectedPort = new Map();

  for (let i = 0; i < expectedLogTemplate.length; i++) {
    let expectedName = expectedLogTemplate[i][0];
    let expectedPort = expectedLogTemplate[i][1];
    let seenName = dohReqPortLog[i][0];
    let seenPort = dohReqPortLog[i][1];
    info(`Checking log entry. Name: ${seenName}, Port: ${seenPort}`);
    equal(expectedName, seenName, "Name matches for entry " + i);
    if (!seenPortsByExpectedPort.has(expectedPort)) {
      ok(!seenPorts.has(seenPort), "Port should not have been previously used");
      seenPorts.add(seenPort);
      seenPortsByExpectedPort.set(expectedPort, seenPort);
    } else {
      equal(
        seenPort,
        seenPortsByExpectedPort.get(expectedPort),
        "Connection was reused as expected"
      );
    }
  }
}
