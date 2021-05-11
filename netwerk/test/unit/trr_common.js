/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_trr.js */
/* import-globals-from head_http3.js */

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const TRR_Domain = "foo.example.com";

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
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
  dns.reloadParentalControlEnabled();
  MockRegistrar.unregister(cid);
}

let runningODoHTests = false;
let h2Port;

function setModeAndURIForODoH(mode, path) {
  Services.prefs.setIntPref("network.trr.mode", mode);
  if (path.substr(0, 4) == "doh?") {
    path = path.replace("doh?", "odoh?");
  }

  Services.prefs.setCharPref("network.trr.odoh.target_path", `${path}`);
}

function setModeAndURI(mode, path) {
  if (runningODoHTests) {
    setModeAndURIForODoH(mode, path);
  } else {
    Services.prefs.setIntPref("network.trr.mode", mode);
    Services.prefs.setCharPref(
      "network.trr.uri",
      `https://${TRR_Domain}:${h2Port}/${path}`
    );
  }
}

async function test_A_record() {
  info("Verifying a basic A record");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2"); // TRR-first
  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  info("Verifying a basic A record - without bootstrapping");
  dns.clearCache(true);
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
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=4.4.4.4&auth=true");
  Services.prefs.setCharPref("network.trr.credentials", "user:password");

  await new TRRDNSListener("bar.example.com", "4.4.4.4");

  info("Verify failing credentials in DOH request");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=4.4.4.4&auth=true");
  Services.prefs.setCharPref("network.trr.credentials", "evil:person");

  let [, , inStatus] = await new TRRDNSListener(
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

  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2020:2020::2020&delayIPv4=100");

  await new TRRDNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2020:2020::2020&delayIPv6=100");

  await new TRRDNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2020:2020::2020");

  await new TRRDNSListener("aaaa.example.com", "2020:2020::2020");
}

async function test_RFC1918() {
  info("Verifying that RFC1918 address from the server is rejected by default");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.168.0.1");

  let [, , inStatus] = await new TRRDNSListener(
    "rfc1918.example.com",
    undefined,
    false
  );

  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  setModeAndURI(3, "doh?responseIP=::ffff:192.168.0.1");
  [, , inStatus] = await new TRRDNSListener(
    "rfc1918-ipv6.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  info("Verify RFC1918 address from the server is fine when told so");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.168.0.1");
  Services.prefs.setBoolPref("network.trr.allow-rfc1918", true);
  await new TRRDNSListener("rfc1918.example.com", "192.168.0.1");
  setModeAndURI(3, "doh?responseIP=::ffff:192.168.0.1");

  await new TRRDNSListener("rfc1918-ipv6.example.com", "::ffff:192.168.0.1");

  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
}

async function test_GET_ECS() {
  info("Verifying resolution via GET with ECS disabled");
  dns.clearCache(true);
  // The template part should be discarded
  if (runningODoHTests) {
    setModeAndURI(3, "odoh");
  } else {
    setModeAndURI(3, "doh{?dns}");
  }
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);

  await new TRRDNSListener("ecs.example.com", "5.5.5.5");

  info("Verifying resolution via GET with ECS enabled");
  dns.clearCache(true);
  if (runningODoHTests) {
    setModeAndURI(3, "odoh");
  } else {
    setModeAndURI(3, "doh");
  }
  Services.prefs.setBoolPref("network.trr.disable-ECS", false);

  await new TRRDNSListener("get.example.com", "5.5.5.5");

  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
}

async function test_timeout_mode3() {
  info("Verifying that a short timeout causes failure with a slow server");
  dns.clearCache(true);
  // First, mode 3.
  setModeAndURI(3, "doh?noResponse=true");
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);

  let [, , inStatus] = await new TRRDNSListener(
    "timeout.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Now for mode 2
  dns.clearCache(true);
  setModeAndURI(2, "doh?noResponse=true");

  await new TRRDNSListener("timeout.example.com", "127.0.0.1"); // Should fallback

  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
}

async function test_no_answers_fallback() {
  info("Verfiying that we correctly fallback to Do53 when no answers from DoH");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=none"); // TRR-first

  await new TRRDNSListener("confirm.example.com", "127.0.0.1");
}

async function test_404_fallback() {
  info("Verfiying that we correctly fallback to Do53 when DoH sends 404");
  dns.clearCache(true);
  setModeAndURI(2, "404"); // TRR-first

  await new TRRDNSListener("test404.example.com", "127.0.0.1");
}

async function test_mode_1_and_4() {
  info("Verifying modes 1 and 4 are treated as TRR-off");
  for (let mode of [1, 4]) {
    dns.clearCache(true);
    setModeAndURI(mode, "doh?responseIP=2.2.2.2");
    Assert.equal(dns.currentTrrMode, 5, "Effective TRR mode should be 5");
  }
}

async function test_CNAME() {
  info("Checking that we follow a CNAME correctly");
  dns.clearCache(true);
  // The dns-cname path alternates between sending us a CNAME pointing to
  // another domain, and an A record. If we follow the cname correctly, doing
  // a lookup with this path as the DoH URI should resolve to that A record.
  if (runningODoHTests) {
    setModeAndURI(3, "odoh?cname=content");
  } else {
    setModeAndURI(3, "dns-cname");
  }

  await new TRRDNSListener("cname.example.com", "99.88.77.66");

  info("Verifying that we bail out when we're thrown into a CNAME loop");
  dns.clearCache(true);
  // First mode 3.
  if (runningODoHTests) {
    let chan = makeChan(
      `https://foo.example.com:${h2Port}/reset_cname_confirm`,
      Ci.nsIRequest.TRR_DISABLED_MODE
    );
    await new Promise(resolve => chan.asyncOpen(new ChannelListener(resolve)));

    setModeAndURI(3, "odoh?responseIP=none&cnameloop=true");
  } else {
    setModeAndURI(3, "doh?responseIP=none&cnameloop=true");
  }

  let [, , inStatus] = await new TRRDNSListener(
    "test18.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // Now mode 2.
  dns.clearCache(true);
  if (runningODoHTests) {
    setModeAndURI(2, "ododoh?responseIP=none&cnameloop=trueoh");
  } else {
    setModeAndURI(2, "doh?responseIP=none&cnameloop=true");
  }

  await new TRRDNSListener("test20.example.com", "127.0.0.1"); // Should fallback

  info("Check that we correctly handle CNAME bundled with an A record");
  dns.clearCache(true);
  // "dns-cname-a" path causes server to send a CNAME as well as an A record
  if (runningODoHTests) {
    setModeAndURI(3, "odoh?cname=ARecord");
  } else {
    setModeAndURI(3, "dns-cname-a");
  }

  await new TRRDNSListener("cname-a.example.com", "9.8.7.6");
}

async function test_name_mismatch() {
  info("Verify that records that don't match the requested name are rejected");
  dns.clearCache(true);
  // Setting hostname param tells server to always send record for bar.example.com
  // regardless of what was requested.
  setModeAndURI(3, "doh?hostname=mismatch.example.com");

  let [, , inStatus] = await new TRRDNSListener(
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
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");

  await new TRRDNSListener("bar.example.com", "192.192.192.192");
}

async function test_excluded_domains() {
  info("Checking that Do53 is used for names in excluded-domains list");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "bar.example.com");

  await new TRRDNSListener("bar.example.com", "127.0.0.1"); // Do53 result

  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "example.com");

  await new TRRDNSListener("bar.example.com", "127.0.0.1");

  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "foo.test.com, bar.example.com"
  );
  await new TRRDNSListener("bar.example.com", "127.0.0.1");

  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "bar.example.com, foo.test.com"
  );

  await new TRRDNSListener("bar.example.com", "127.0.0.1");

  Services.prefs.clearUserPref("network.trr.excluded-domains");
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
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  const cpServer = new HttpServer();
  cpServer.registerPathHandler("/cp", function handleRawData(
    request,
    response
  ) {
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.bodyOutputStream.write("data", 4);
  });
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

async function test_parentalcontrols() {
  info("Check that DoH isn't used when parental controls are enabled");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
}

async function test_builtin_excluded_domains() {
  info("Verifying Do53 is used for domains in builtin-excluded-domians list");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "bar.example.com"
  );
  await new TRRDNSListener("bar.example.com", "127.0.0.1");

  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "example.com"
  );
  await new TRRDNSListener("bar.example.com", "127.0.0.1");

  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "foo.test.com, bar.example.com"
  );
  await new TRRDNSListener("bar.example.com", "127.0.0.1");
  await new TRRDNSListener("foo.test.com", "127.0.0.1");
}

async function test_excluded_domains_mode3() {
  info("Checking  Do53 is used for names in excluded-domains list in mode 3");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");

  await new TRRDNSListener("excluded", "192.192.192.192", true);

  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded");

  await new TRRDNSListener("excluded", "127.0.0.1");

  // Test .local
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded,local");

  await new TRRDNSListener("test.local", "127.0.0.1");

  // Test .other
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "excluded,local,other"
  );

  await new TRRDNSListener("domain.other", "127.0.0.1");
}

async function test25e() {
  info("Check captivedetect.canonicalURL is resolved via native DNS in mode 3");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");

  const cpServer = new HttpServer();
  cpServer.registerPathHandler("/cp", function handleRawData(
    request,
    response
  ) {
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.bodyOutputStream.write("data", 4);
  });
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
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
}

async function test_builtin_excluded_domains_mode3() {
  info("Check Do53 used for domains in builtin-excluded-domians list, mode 3");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded"
  );

  await new TRRDNSListener("excluded", "127.0.0.1");

  // Test .local
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded,local"
  );

  await new TRRDNSListener("test.local", "127.0.0.1");

  // Test .other
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded,local,other"
  );

  await new TRRDNSListener("domain.other", "127.0.0.1");
}

async function count_cookies() {
  info("Check that none of the requests have set any cookies.");
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  Assert.equal(cm.countCookiesFromHost("example.com"), 0);
  Assert.equal(cm.countCookiesFromHost("foo.example.com."), 0);
}

async function test_connection_closed() {
  info("Check we handle it correctly when the connection is closed");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2.2.2.2");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  // We don't need to wait for 30 seconds for the request to fail
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 500);
  // bootstrap
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new TRRDNSListener(
    "closeme.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // No bootstrap this time
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded,local");
  Services.prefs.setCharPref("network.dns.localDomains", TRR_Domain);

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  [, , inStatus] = await new TRRDNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // No local domains either
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  [, , inStatus] = await new TRRDNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // Now make sure that even in mode 3 without a bootstrap address
  // we are able to restart the TRR connection if it drops - the TRR service
  // channel will use regular DNS to resolve the TRR address.
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  [, , inStatus] = await new TRRDNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  dns.clearCache(true);
  await new TRRDNSListener("bar2.example.com", "2.2.2.2");

  // This test exists to document what happens when we're in TRR only mode
  // and we don't set a bootstrap address. We use DNS to resolve the
  // initial URI, but if the connection fails, we don't fallback to DNS
  dns.clearCache(true);
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
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2&delayIPv4=20");

  await new TRRDNSListener("bar_time.example.com", "2.2.2.2", true, 20);

  // gets an error from DoH. It will fall back to regular DNS. The TRR timing should be 0.
  dns.clearCache(true);
  setModeAndURI(2, "404&delayIPv4=20");

  await new TRRDNSListener("bar_time1.example.com", "127.0.0.1", true, 0);

  // check an excluded domain. It should fall back to regular DNS. The TRR timing should be 0.
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "bar_time2.example.com"
  );
  setModeAndURI(2, "doh?responseIP=2.2.2.2&delayIPv4=20");

  await new TRRDNSListener("bar_time2.example.com", "127.0.0.1", true, 0);

  Services.prefs.setCharPref("network.trr.excluded-domains", "");

  // verify RFC1918 address from the server is rejected and the TRR timing will be not set because the response will be from the native resolver.
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=192.168.0.1&delayIPv4=20");
  await new TRRDNSListener("rfc1918_time.example.com", "127.0.0.1", true, 0);
}

async function test_fqdn() {
  info("Test that we handle FQDN encoding and decoding properly");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=9.8.7.6");

  await new TRRDNSListener("fqdn.example.org.", "9.8.7.6");

  // GET
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.useGET", true);
  await new TRRDNSListener("fqdn_get.example.org.", "9.8.7.6");

  Services.prefs.clearUserPref("network.trr.useGET");
}

async function test_ipv6_trr_fallback() {
  info("Testing fallback with ipv6");
  dns.clearCache(true);
  let httpserver = new HttpServer();
  httpserver.registerPathHandler("/content", (metadata, response) => {
    response.setHeader("Content-Type", "text/plain");
    response.setHeader("Cache-Control", "no-cache");

    const responseBody = "anybody";
    response.bodyOutputStream.write(responseBody, responseBody.length);
  });
  httpserver.start(-1);

  Services.prefs.setBoolPref("network.captive-portal-service.testMode", true);
  let url = `http://127.0.0.1:666/doom_port_should_not_be_open`;
  Services.prefs.setCharPref("network.connectivity-service.IPv6.url", url);
  let ncs = Cc[
    "@mozilla.org/network/network-connectivity-service;1"
  ].getService(Ci.nsINetworkConnectivityService);

  function promiseObserverNotification(topic, matchFunc) {
    return new Promise((resolve, reject) => {
      Services.obs.addObserver(function observe(subject, topic, data) {
        let matches =
          typeof matchFunc != "function" || matchFunc(subject, data);
        if (!matches) {
          return;
        }
        Services.obs.removeObserver(observe, topic);
        resolve({ subject, data });
      }, topic);
    });
  }

  let checks = promiseObserverNotification(
    "network:connectivity-service:ip-checks-complete"
  );

  ncs.recheckIPConnectivity();

  await checks;
  equal(
    ncs.IPv6,
    Ci.nsINetworkConnectivityService.NOT_AVAILABLE,
    "Check IPv6 support (expect NOT_AVAILABLE)"
  );

  setModeAndURI(2, "doh?responseIP=4.4.4.4");
  const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
    Ci.nsINativeDNSResolverOverride
  );
  gOverride.addIPOverride("ipv6.host.com", "1:1::2");

  await new TRRDNSListener(
    "ipv6.host.com",
    "1:1::2",
    true,
    0,
    "",
    false,
    Ci.nsIDNSService.RESOLVE_DISABLE_IPV4
  );

  Services.prefs.clearUserPref("network.captive-portal-service.testMode");
  Services.prefs.clearUserPref("network.connectivity-service.IPv6.url");

  override.clearOverrides();
  await httpserver.stop();
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
      statusCounter.statusCount[0x804b000b],
      1,
      "Expecting only one instance of NS_NET_STATUS_RESOLVED_HOST"
    );
    equal(
      statusCounter.statusCount[0x804b0007],
      1,
      "Expecting only one instance of NS_NET_STATUS_CONNECTING_TO"
    );
  }

  await test(`http://unknown.ipv4.stuff:666/path`, "0.0.0.0");
  await test(`http://unknown.ipv6.stuff:666/path`, "::");
}
