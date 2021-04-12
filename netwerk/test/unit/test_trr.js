"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

const gDefaultPref = Services.prefs.getDefaultBranch("");
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

SetParentalControlEnabled(false);

let h2Port = trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

async function waitForConfirmation(expectedResponseIP, confirmationShouldFail) {
  // Check that the confirmation eventually completes.
  let count = 100;
  while (count > 0) {
    if (count == 50 || count == 10) {
      // At these two points we do a longer timeout to account for a slow
      // response on the server side. This is usually a problem on the Android
      // because of the increased delay between the emulator and host.
      await new Promise(resolve => do_timeout(100 * (100 / count), resolve));
    }
    let [, inRecord] = await new TRRDNSListener(
      `ip${count}.example.org`,
      undefined,
      false
    );
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    let responseIP = inRecord.getNextAddrAsString();
    Assert.ok(true, responseIP);
    if (responseIP == expectedResponseIP) {
      break;
    }
    count--;
  }

  if (confirmationShouldFail) {
    Assert.equal(count, 0, "Confirmation did not finish after 100 iterations");
    return;
  }

  Assert.greater(count, 0, "Finished confirmation before 100 iterations");
}

const TRR_Domain = "foo.example.com";
function setModeAndURI(mode, path) {
  Services.prefs.setIntPref("network.trr.mode", mode);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://${TRR_Domain}:${h2Port}/${path}`
  );
}

function makeChan(url, mode, bypassCache) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  chan.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
  chan.setTRRMode(mode);
  return chan;
}

add_task(async function test_server_up() {
  // This test checks that moz-http2.js running in node is working.
  // This should always be the first test in this file (except for setup)
  // otherwise we may encounter random failures when the http2 server is down.

  await NodeServer.execute("bad_id", `"hello"`)
    .then(() => ok(false, "expecting to throw"))
    .catch(e => equal(e.message, "Error: could not find id"));
});

add_task(async function test_trr_flags() {
  Services.prefs.setBoolPref("network.trr.fallback-on-zero-response", true);

  let httpserv = new HttpServer();
  httpserv.registerPathHandler("/", function handler(metadata, response) {
    let content = "ok";
    response.setHeader("Content-Length", `${content.length}`);
    response.bodyOutputStream.write(content, content.length);
  });
  httpserv.start(-1);

  const URL = `http://example.com:${httpserv.identity.primaryPort}/`;

  for (let mode of [0, 1, 2, 3, 4, 5]) {
    setModeAndURI(mode, "doh?responseIP=127.0.0.1");
    for (let flag of [
      Ci.nsIRequest.TRR_DEFAULT_MODE,
      Ci.nsIRequest.TRR_DISABLED_MODE,
      Ci.nsIRequest.TRR_FIRST_MODE,
      Ci.nsIRequest.TRR_ONLY_MODE,
    ]) {
      dns.clearCache(true);
      let chan = makeChan(URL, flag);
      let expectTRR =
        ([2, 3].includes(mode) && flag != Ci.nsIRequest.TRR_DISABLED_MODE) ||
        (mode == 0 &&
          [Ci.nsIRequest.TRR_FIRST_MODE, Ci.nsIRequest.TRR_ONLY_MODE].includes(
            flag
          ));

      await new Promise(resolve =>
        chan.asyncOpen(new ChannelListener(resolve))
      );

      equal(chan.getTRRMode(), flag);
      equal(
        expectTRR,
        chan.QueryInterface(Ci.nsIHttpChannelInternal).isResolvedByTRR
      );
    }
  }

  await new Promise(resolve => httpserv.stop(resolve));
  Services.prefs.clearUserPref("network.trr.fallback-on-zero-response");
});

add_task(async function test_A_record() {
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
});

add_task(async function test_push() {
  info("Verify DOH push");
  dns.clearCache(true);
  info("Asking server to push us a record");
  setModeAndURI(3, "doh?responseIP=5.5.5.5&push=true");

  await new TRRDNSListener("first.example.com", "5.5.5.5");

  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  // Don't clear the cache, otherwise we lose the pushed record.
  setModeAndURI(3, "404");

  await new TRRDNSListener("push.example.org", "2018::2018");
});

add_task(async function test_AAAA_records() {
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
});

add_task(async function test_RFC1918() {
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
});

add_task(async function test_GET_ECS() {
  info("Verifying resolution via GET with ECS disabled");
  dns.clearCache(true);
  // The template part should be discarded
  setModeAndURI(3, "doh{?dns}");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);

  await new TRRDNSListener("ecs.example.com", "5.5.5.5");

  info("Verifying resolution via GET with ECS enabled");
  dns.clearCache(true);
  setModeAndURI(3, "doh");
  Services.prefs.setBoolPref("network.trr.disable-ECS", false);

  await new TRRDNSListener("get.example.com", "5.5.5.5");

  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
});

add_task(async function test_timeout_mode3() {
  info("Verifying that a short timeout causes failure with a slow server");
  dns.clearCache(true);
  // First, mode 3.
  setModeAndURI(3, "dns-750ms");
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
  setModeAndURI(2, "dns-750ms");

  await new TRRDNSListener("timeout.example.com", "127.0.0.1"); // Should fallback

  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
});

add_task(async function test_no_answers_fallback() {
  info("Verfiying that we correctly fallback to Do53 when no answers from DoH");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=none"); // TRR-first

  await new TRRDNSListener("confirm.example.com", "127.0.0.1");
});

add_task(async function test_404_fallback() {
  info("Verfiying that we correctly fallback to Do53 when DoH sends 404");
  dns.clearCache(true);
  setModeAndURI(2, "404"); // TRR-first

  await new TRRDNSListener("test404.example.com", "127.0.0.1");
});

add_task(async function test_mode_1_and_4() {
  info("Verifying modes 1 and 4 are treated as TRR-off");
  for (let mode of [1, 4]) {
    dns.clearCache(true);
    setModeAndURI(mode, "doh?responseIP=2.2.2.2");
    Assert.equal(dns.currentTrrMode, 5, "Effective TRR mode should be 5");
  }
});

add_task(async function test_CNAME() {
  info("Checking that we follow a CNAME correctly");
  dns.clearCache(true);
  // The dns-cname path alternates between sending us a CNAME pointing to
  // another domain, and an A record. If we follow the cname correctly, doing
  // a lookup with this path as the DoH URI should resolve to that A record.
  setModeAndURI(3, "dns-cname");

  await new TRRDNSListener("cname.example.com", "99.88.77.66");

  info("Verifying that we bail out when we're thrown into a CNAME loop");
  dns.clearCache(true);
  // First mode 3.
  setModeAndURI(3, "doh?responseIP=none&cnameloop=true");

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
  setModeAndURI(2, "doh?responseIP=none&cnameloop=true");

  await new TRRDNSListener("test20.example.com", "127.0.0.1"); // Should fallback

  info("Check that we correctly handle CNAME bundled with an A record");
  dns.clearCache(true);
  // "dns-cname-a" path causes server to send a CNAME as well as an A record
  setModeAndURI(3, "dns-cname-a");

  await new TRRDNSListener("cname-a.example.com", "9.8.7.6");
});

add_task(async function test_name_mismatch() {
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
});

add_task(async function test_mode_2() {
  info("Checking that TRR result is used in mode 2");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=192.192.192.192");
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");

  await new TRRDNSListener("bar.example.com", "192.192.192.192");
});

add_task(async function test_excluded_domains() {
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
});

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

add_task(async function test_captiveportal_canonicalURL() {
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
});

add_task(async function test_parentalcontrols() {
  info("Check that DoH isn't used when parental controls are enabled");
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
});

// TRR-first check that DNS result is used if domain is part of the builtin-excluded-domains pref
add_task(async function test_builtin_excluded_domains() {
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
});

add_task(async function test_excluded_domains_mode3() {
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
});

add_task(async function test25e() {
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
});

add_task(async function test_parentalcontrols_mode3() {
  info("Check DoH isn't used when parental controls are enabled in mode 3");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=192.192.192.192");
  await SetParentalControlEnabled(true);
  await new TRRDNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
});

add_task(async function test_builtin_excluded_domains_mode3() {
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
});

add_task(async function count_cookies() {
  info("Check that none of the requests have set any cookies.");
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  Assert.equal(cm.countCookiesFromHost("example.com"), 0);
  Assert.equal(cm.countCookiesFromHost("foo.example.com."), 0);
});

add_task(async function test_connection_closed() {
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
});

add_task(async function test_clearCacheOnURIChange() {
  info("Check that the TRR cache should be cleared by a pref change.");
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", true);
  setModeAndURI(2, "doh?responseIP=7.7.7.7");

  await new TRRDNSListener("bar.example.com", "7.7.7.7");

  // The TRR cache should be cleared by this pref change.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://localhost:${h2Port}/doh?responseIP=8.8.8.8`
  );

  await new TRRDNSListener("bar.example.com", "8.8.8.8");
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);
});

add_task(async function test_dnsSuffix() {
  info("Checking that domains matching dns suffix list use Do53");
  async function checkDnsSuffixInMode(mode) {
    dns.clearCache(true);
    setModeAndURI(mode, "doh?responseIP=1.2.3.4&push=true");
    await new TRRDNSListener("example.org", "1.2.3.4");
    await new TRRDNSListener("push.example.org", "2018::2018");
    await new TRRDNSListener("test.com", "1.2.3.4");

    let networkLinkService = {
      dnsSuffixList: ["example.org"],
      QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),
    };
    Services.obs.notifyObservers(
      networkLinkService,
      "network:dns-suffix-list-updated"
    );
    await new TRRDNSListener("test.com", "1.2.3.4");
    if (Services.prefs.getBoolPref("network.trr.split_horizon_mitigations")) {
      await new TRRDNSListener("example.org", "127.0.0.1");
      // Also test that we don't use the pushed entry.
      await new TRRDNSListener("push.example.org", "127.0.0.1");
    } else {
      await new TRRDNSListener("example.org", "1.2.3.4");
      await new TRRDNSListener("push.example.org", "2018::2018");
    }

    // Attempt to clean up, just in case
    networkLinkService.dnsSuffixList = [];
    Services.obs.notifyObservers(
      networkLinkService,
      "network:dns-suffix-list-updated"
    );
  }

  Services.prefs.setBoolPref("network.trr.split_horizon_mitigations", true);
  await checkDnsSuffixInMode(2);
  Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");
  await checkDnsSuffixInMode(3);
  Services.prefs.setBoolPref("network.trr.split_horizon_mitigations", false);
  // Test again with mitigations off
  await checkDnsSuffixInMode(2);
  await checkDnsSuffixInMode(3);
  Services.prefs.clearUserPref("network.trr.split_horizon_mitigations");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");
});

add_task(async function test_async_resolve_with_trr_server() {
  info("Checking asyncResolveWithTrrServer");
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 0); // TRR-disabled

  await new TRRDNSListener(
    "bar_with_trr1.example.com",
    "2.2.2.2",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`
  );

  // Test request without trr server, it should return a native dns response.
  await new TRRDNSListener("bar_with_trr1.example.com", "127.0.0.1");

  // Mode 2
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  await new TRRDNSListener(
    "bar_with_trr2.example.com",
    "3.3.3.3",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );

  // Test request without trr server, it should return a response from trr server defined in the pref.
  await new TRRDNSListener("bar_with_trr2.example.com", "2.2.2.2");

  // Mode 3
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2.2.2.2");

  await new TRRDNSListener(
    "bar_with_trr3.example.com",
    "3.3.3.3",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );

  // Test request without trr server, it should return a response from trr server defined in the pref.
  await new TRRDNSListener("bar_with_trr3.example.com", "2.2.2.2");

  // Mode 5
  dns.clearCache(true);
  setModeAndURI(5, "doh?responseIP=2.2.2.2");

  // When dns is resolved in socket process, we can't set |expectEarlyFail| to true.
  let inSocketProcess = mozinfo.socketprocess_networking;
  await new TRRDNSListener(
    "bar_with_trr3.example.com",
    undefined,
    false,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`,
    !inSocketProcess
  );

  // Call normal AsyncOpen, it will return result from the native resolver.
  await new TRRDNSListener("bar_with_trr3.example.com", "127.0.0.1");

  // Check that cache is ignored when server is different
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=2.2.2.2");

  await new TRRDNSListener("bar_with_trr4.example.com", "2.2.2.2", true);

  // The record will be fetch again.
  await new TRRDNSListener(
    "bar_with_trr4.example.com",
    "3.3.3.3",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );

  // The record will be fetch again.
  await new TRRDNSListener(
    "bar_with_trr5.example.com",
    "4.4.4.4",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=4.4.4.4`
  );

  // Check no fallback and no blocklisting upon failure
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  let [, , inStatus] = await new TRRDNSListener(
    "bar_with_trr6.example.com",
    undefined,
    false,
    undefined,
    `https://foo.example.com:${h2Port}/404`
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  await new TRRDNSListener("bar_with_trr6.example.com", "2.2.2.2", true);

  // Check that DoH push doesn't work
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  await new TRRDNSListener(
    "bar_with_trr7.example.com",
    "3.3.3.3",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3&push=true`
  );

  // AsyncResoleWithTrrServer rejects server pushes and the entry for push.example.org
  // shouldn't be neither in the default cache not in AsyncResoleWithTrrServer cache.
  setModeAndURI(2, "404");

  await new TRRDNSListener(
    "push.example.org",
    "3.3.3.3",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3&push=true`
  );

  await new TRRDNSListener("push.example.org", "127.0.0.1");

  // Check confirmation is ignored
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=1::ffff");
  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  // AsyncResoleWithTrrServer will succeed
  await new TRRDNSListener(
    "bar_with_trr8.example.com",
    "3.3.3.3",
    true,
    undefined,
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );

  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  // Bad port
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  [, , inStatus] = await new TRRDNSListener(
    "only_once.example.com",
    undefined,
    false,
    undefined,
    `https://target.example.com:666/404`
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // // MOZ_LOG=sync,timestamp,nsHostResolver:5 We should not keep resolving only_once.example.com
  // // TODO: find a way of automating this
  // await new Promise(resolve => {});
});

add_task(async function test_fetch_time() {
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
});

add_task(async function test_content_encoding_gzip() {
  info("Checking gzip content encoding");
  dns.clearCache(true);
  Services.prefs.setBoolPref(
    "network.trr.send_empty_accept-encoding_headers",
    false
  );
  setModeAndURI(3, "doh?responseIP=2.2.2.2");

  await new TRRDNSListener("bar.example.com", "2.2.2.2");
  Services.prefs.clearUserPref(
    "network.trr.send_empty_accept-encoding_headers"
  );
});

add_task(async function test_redirect() {
  info("Check handling of redirect");

  // GET
  dns.clearCache(true);
  setModeAndURI(3, "doh?redirect=4.4.4.4{&dns}");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);

  await new TRRDNSListener("ecs.example.com", "4.4.4.4");

  // POST
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.useGET", false);
  setModeAndURI(3, "doh?redirect=4.4.4.4");

  await new TRRDNSListener("bar.example.com", "4.4.4.4");

  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
});

// confirmationNS set without confirmed NS yet
// checks that we properly fall back to DNS is confirmation is not ready yet,
// and wait-for-confirmation pref is true
add_task(async function test_confirmation() {
  info("Checking that we fall back correctly when confirmation is pending");
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.wait-for-confirmation", true);
  setModeAndURI(2, "doh?responseIP=7.7.7.7&slowConfirm=true");
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  await new TRRDNSListener("example.org", "127.0.0.1");
  await new Promise(resolve => do_timeout(1000, resolve));
  await waitForConfirmation("7.7.7.7");

  // Reset between each test to force re-confirm
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  info("Check that confirmation is skipped in mode 3");
  // This is just a smoke test to make sure lookups succeed immediately
  // in mode 3 without waiting for confirmation.
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=1::ffff&slowConfirm=true");
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  await new TRRDNSListener("skipConfirmationForMode3.example.com", "1::ffff");

  // Reset between each test to force re-confirm
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.wait-for-confirmation", false);
  setModeAndURI(2, "doh?responseIP=7.7.7.7&slowConfirm=true");
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  // DoH available immediately
  await new TRRDNSListener("example.org", "7.7.7.7");

  // Reset between each test to force re-confirm
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  // Fallback when confirmation fails
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.wait-for-confirmation", true);
  setModeAndURI(2, "404");
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  await waitForConfirmation("7.7.7.7", true);

  await new TRRDNSListener("example.org", "127.0.0.1");

  // Reset
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.prefs.clearUserPref("network.trr.wait-for-confirmation");
});

add_task(async function test_fqdn() {
  info("Test that we handle FQDN encoding and decoding properly");
  dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=9.8.7.6");

  await new TRRDNSListener("fqdn.example.org.", "9.8.7.6");

  // GET
  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.useGET", true);
  await new TRRDNSListener("fqdn_get.example.org.", "9.8.7.6");

  Services.prefs.clearUserPref("network.trr.useGET");
});

add_task(async function test_detected_uri() {
  info("Test setDetectedTrrURI");
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.clearUserPref("network.trr.uri");
  let defaultURI = gDefaultPref.getCharPref("network.trr.uri");
  gDefaultPref.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.4.5.6`
  );
  await new TRRDNSListener("domainA.example.org.", "3.4.5.6");
  dns.setDetectedTrrURI(
    `https://foo.example.com:${h2Port}/doh?responseIP=1.2.3.4`
  );
  await new TRRDNSListener("domainB.example.org.", "1.2.3.4");
  gDefaultPref.setCharPref("network.trr.uri", defaultURI);

  // With a user-set doh uri this time.
  dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=4.5.6.7");
  await new TRRDNSListener("domainA.example.org.", "4.5.6.7");

  // This should be a no-op, since we have a user-set URI
  dns.setDetectedTrrURI(
    `https://foo.example.com:${h2Port}/doh?responseIP=1.2.3.4`
  );
  await new TRRDNSListener("domainB.example.org.", "4.5.6.7");

  // Test network link status change
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.clearUserPref("network.trr.uri");
  gDefaultPref.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.4.5.6`
  );
  await new TRRDNSListener("domainA.example.org.", "3.4.5.6");
  dns.setDetectedTrrURI(
    `https://foo.example.com:${h2Port}/doh?responseIP=1.2.3.4`
  );
  await new TRRDNSListener("domainB.example.org.", "1.2.3.4");

  let networkLinkService = {
    platformDNSIndications: 0,
    QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),
  };

  Services.obs.notifyObservers(
    networkLinkService,
    "network:link-status-changed",
    "changed"
  );

  await new TRRDNSListener("domainC.example.org.", "3.4.5.6");

  gDefaultPref.setCharPref("network.trr.uri", defaultURI);
});

add_task(async function test_pref_changes() {
  info("Testing pref change handling");
  Services.prefs.clearUserPref("network.trr.uri");
  let defaultURI = gDefaultPref.getCharPref("network.trr.uri");

  async function doThenCheckURI(closure, expectedURI, expectChange = false) {
    let uriChanged;
    if (expectChange) {
      uriChanged = topicObserved("network:trr-uri-changed");
    }
    closure();
    if (expectChange) {
      await uriChanged;
    }
    equal(dns.currentTrrURI, expectedURI);
  }

  // setting the default value of the pref should be reflected in the URI
  await doThenCheckURI(() => {
    gDefaultPref.setCharPref(
      "network.trr.uri",
      `https://foo.example.com:${h2Port}/doh?default`
    );
  }, `https://foo.example.com:${h2Port}/doh?default`);

  // the user set value should be reflected in the URI
  await doThenCheckURI(() => {
    Services.prefs.setCharPref(
      "network.trr.uri",
      `https://foo.example.com:${h2Port}/doh?user`
    );
  }, `https://foo.example.com:${h2Port}/doh?user`);

  // A user set pref is selected, so it should be chosen instead of the rollout
  await doThenCheckURI(
    () => {
      Services.prefs.setCharPref(
        "doh-rollout.uri",
        `https://foo.example.com:${h2Port}/doh?rollout`
      );
    },
    `https://foo.example.com:${h2Port}/doh?user`,
    false
  );

  // There is no user set pref, so we go to the rollout pref
  await doThenCheckURI(() => {
    Services.prefs.clearUserPref("network.trr.uri");
  }, `https://foo.example.com:${h2Port}/doh?rollout`);

  // When the URI is set by the rollout addon, detection is allowed
  await doThenCheckURI(() => {
    dns.setDetectedTrrURI(`https://foo.example.com:${h2Port}/doh?detected`);
  }, `https://foo.example.com:${h2Port}/doh?detected`);

  // Should switch back to the default provided by the rollout addon
  await doThenCheckURI(() => {
    let networkLinkService = {
      platformDNSIndications: 0,
      QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),
    };
    Services.obs.notifyObservers(
      networkLinkService,
      "network:link-status-changed",
      "changed"
    );
  }, `https://foo.example.com:${h2Port}/doh?rollout`);

  // Again the user set pref should be chosen
  await doThenCheckURI(() => {
    Services.prefs.setCharPref(
      "network.trr.uri",
      `https://foo.example.com:${h2Port}/doh?user`
    );
  }, `https://foo.example.com:${h2Port}/doh?user`);

  // Detection should not work with a user set pref
  await doThenCheckURI(
    () => {
      dns.setDetectedTrrURI(`https://foo.example.com:${h2Port}/doh?detected`);
    },
    `https://foo.example.com:${h2Port}/doh?user`,
    false
  );

  // Should stay the same on network changes
  await doThenCheckURI(
    () => {
      let networkLinkService = {
        platformDNSIndications: 0,
        QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),
      };
      Services.obs.notifyObservers(
        networkLinkService,
        "network:link-status-changed",
        "changed"
      );
    },
    `https://foo.example.com:${h2Port}/doh?user`,
    false
  );

  // Restore the pref
  gDefaultPref.setCharPref("network.trr.uri", defaultURI);
});

add_task(async function test_dohrollout_mode() {
  info("Testing doh-rollout.mode");
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("doh-rollout.mode");

  equal(dns.currentTrrMode, 0);

  async function doThenCheckMode(trrMode, rolloutMode, expectedMode, message) {
    let modeChanged;
    if (dns.currentTrrMode != expectedMode) {
      modeChanged = topicObserved("network:trr-mode-changed");
    }

    if (trrMode != undefined) {
      Services.prefs.setIntPref("network.trr.mode", trrMode);
    }

    if (rolloutMode != undefined) {
      Services.prefs.setIntPref("doh-rollout.mode", rolloutMode);
    }

    if (modeChanged) {
      await modeChanged;
    }
    equal(dns.currentTrrMode, expectedMode, message);
  }

  await doThenCheckMode(2, undefined, 2);
  await doThenCheckMode(3, undefined, 3);
  await doThenCheckMode(5, undefined, 5);
  await doThenCheckMode(2, undefined, 2);
  await doThenCheckMode(0, undefined, 0);
  await doThenCheckMode(1, undefined, 5);
  await doThenCheckMode(6, undefined, 5);

  await doThenCheckMode(2, 0, 2);
  await doThenCheckMode(2, 1, 2);
  await doThenCheckMode(2, 2, 2);
  await doThenCheckMode(2, 3, 2);
  await doThenCheckMode(2, 5, 2);
  await doThenCheckMode(3, 2, 3);
  await doThenCheckMode(5, 2, 5);

  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("doh-rollout.mode");

  await doThenCheckMode(undefined, 2, 2);
  await doThenCheckMode(undefined, 3, 3);

  // All modes that are not 0,2,3 are treated as 5
  await doThenCheckMode(undefined, 5, 5);
  await doThenCheckMode(undefined, 4, 5);
  await doThenCheckMode(undefined, 6, 5);

  await doThenCheckMode(undefined, 2, 2);
  await doThenCheckMode(3, undefined, 3);

  Services.prefs.clearUserPref("network.trr.mode");
  equal(dns.currentTrrMode, 2);
  Services.prefs.clearUserPref("doh-rollout.mode");
  equal(dns.currentTrrMode, 0);
});

add_task(async function test_ipv6_trr_fallback() {
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
});

add_task(async function test_no_retry_without_doh() {
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
});

// This test checks that normally when the TRR mode goes from ON -> OFF
// we purge the DNS cache (including TRR), so the entries aren't used on
// networks where they shouldn't. For example - turning on a VPN.
add_task(async function test_purge_trr_cache_on_mode_change() {
  info("Checking that we purge cache when TRR is turned off");
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", true);

  Services.prefs.setIntPref("network.trr.mode", 0);
  Services.prefs.setIntPref("doh-rollout.mode", 2);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.3.3.3`
  );

  await new TRRDNSListener("cached.example.com", "3.3.3.3");
  Services.prefs.clearUserPref("doh-rollout.mode");

  await new TRRDNSListener("cached.example.com", "127.0.0.1");

  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);
  Services.prefs.clearUserPref("doh-rollout.mode");
});

add_task(async function test_old_bootstrap_pref() {
  dns.clearCache(true);
  // Note this is a remote address. Setting this pref should have no effect,
  // as this is the old name for the bootstrap pref.
  // If this were to be used, the test would crash when accessing a non-local
  // IP address.
  Services.prefs.setCharPref("network.trr.bootstrapAddress", "1.1.1.1");
  setModeAndURI(Ci.nsIDNSService.MODE_TRRONLY, `doh?responseIP=4.4.4.4`);
  await new TRRDNSListener("testytest.com", "4.4.4.4");
});
