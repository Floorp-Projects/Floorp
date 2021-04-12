/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Most of tests in this file are copied from test_trr.js, except tests listed
 * below. Since ODoH doesn't support push and customerized DNS resolver, some
 * related tests are skipped.
 *
 * test_trr_flags
 * test5
 * test5b
 * test25e // Aleady skipped in test_trr.js
 * test_clearCacheOnURIChange // TODO: Clear DNS cache when ODoH prefs changed.
 * test_dnsSuffix
 * test_async_resolve_with_trr_server_1
 * test_async_resolve_with_trr_server_2
 * test_async_resolve_with_trr_server_3
 * test_async_resolve_with_trr_server_5
 * test_async_resolve_with_trr_server_different_cache
 * test_async_resolve_with_trr_server_different_servers
 * test_async_resolve_with_trr_server_no_blacklist
 * test_async_resolve_with_trr_server_no_push
 * test_async_resolve_with_trr_server_no_push_part_2
 * test_async_resolve_with_trr_server_confirmation_ns
 * test_redirect_get
 * test_redirect_post
 * test_resolve_not_confirmed_wait_for_confirmation
 * test_resolve_confirmation_pending
 * test_resolve_confirm_failed
 * test_detected_uri
 * test_detected_uri_userSet
 * test_detected_uri_link_change
 * test_pref_changes
 * test_async_resolve_with_trr_server_bad_port
 * test_dohrollout_mode
 */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;
let listen;

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);
const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
const mainThread = threadManager.currentThread;

const defaultOriginAttributes = {};

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

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

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");

  // make all native resolve calls "secretly" resolve localhost instead
  prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow
  prefs.setIntPref("network.trr.mode", 3); // TRR first
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

  // This is for skiping the security check for the odoh host.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
}

setup();
registerCleanupFunction(() => {
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
  prefs.clearUserPref("network.trr.bootstrapAddr");
  prefs.clearUserPref("network.trr.request-timeout");
  prefs.clearUserPref("network.trr.clear-cache-on-pref-change");
  prefs.clearUserPref("network.trr.odoh.enabled");
  prefs.clearUserPref("network.trr.odoh.target_path");
  prefs.clearUserPref("network.trr.odoh.configs_uri");
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

class DNSListener {
  constructor(
    name,
    expectedAnswer,
    expectedSuccess = true,
    delay,
    type = Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    expectEarlyFail = false,
    flags = 0
  ) {
    this.name = name;
    this.expectedAnswer = expectedAnswer;
    this.expectedSuccess = expectedSuccess;
    this.delay = delay;
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });

    try {
      this.request = dns.asyncResolve(
        name,
        type,
        flags,
        null,
        this,
        mainThread,
        defaultOriginAttributes
      );
      Assert.ok(!expectEarlyFail);
    } catch (e) {
      Assert.ok(expectEarlyFail);
      this.resolve([e]);
    }
  }

  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.ok(
      inRequest == this.request,
      "Checking that this is the correct callback"
    );

    // If we don't expect success here, just resolve and the caller will
    // decide what to do with the results.
    if (!this.expectedSuccess) {
      this.resolve([inRequest, inRecord, inStatus]);
      return;
    }

    Assert.equal(inStatus, Cr.NS_OK, "Checking status");
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    let answer = inRecord.getNextAddrAsString();
    Assert.equal(
      answer,
      this.expectedAnswer,
      `Checking result for ${this.name}`
    );

    if (this.delay !== undefined) {
      Assert.greaterOrEqual(
        inRecord.trrFetchDurationNetworkOnly,
        this.delay,
        `the response should take at least ${this.delay}`
      );

      Assert.greaterOrEqual(
        inRecord.trrFetchDuration,
        this.delay,
        `the response should take at least ${this.delay}`
      );

      if (this.delay == 0) {
        // The response timing should be really 0
        Assert.equal(
          inRecord.trrFetchDurationNetworkOnly,
          0,
          `the response time should be 0`
        );

        Assert.equal(
          inRecord.trrFetchDuration,
          this.delay,
          `the response time should be 0`
        );
      }
    }

    this.resolve([inRequest, inRecord, inStatus]);
  }

  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIDNSListener) || aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  }

  // Implement then so we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

function observerPromise(topic) {
  return new Promise(resolve => {
    let observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      observe(aSubject, aTopic, aData) {
        dump(`observe: ${aSubject}, ${aTopic}, ${aData} \n`);
        if (aTopic == topic) {
          Services.obs.removeObserver(observer, topic);
          resolve(aData);
        }
      },
    };
    Services.obs.addObserver(observer, topic);
  });
}

add_task(async function testODoHConfig() {
  // use the h2 server as DOH provider
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/odohconfig"
  );

  let [, inRecord] = await new DNSListener(
    "odoh_host.example.com",
    undefined,
    false,
    undefined,
    Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC
  );
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "odoh_host.example.com");
  Assert.equal(answer[0].values.length, 1);
  let odohconfig = answer[0].values[0].QueryInterface(Ci.nsISVCParamODoHConfig)
    .ODoHConfig;
  Assert.equal(odohconfig.length, 46);
});

let testIndex = 0;

async function ODoHConfigTest(query, ODoHHost, expectedResult = false) {
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/odohconfig?" + query
  );

  // Setting the pref "network.trr.odoh.target_host" will trigger the reload of
  // the ODoHConfigs.
  if (ODoHHost != undefined) {
    prefs.setCharPref("network.trr.odoh.target_host", ODoHHost);
  } else {
    prefs.setCharPref(
      "network.trr.odoh.target_host",
      `https://odoh_host_${testIndex++}.com`
    );
  }

  await observerPromise("odoh-service-activated");
  Assert.equal(dns.ODoHActivated, expectedResult);
}

add_task(async function testODoHConfig1() {
  await ODoHConfigTest("invalid=empty");
});

add_task(async function testODoHConfig2() {
  await ODoHConfigTest("invalid=version");
});

add_task(async function testODoHConfig3() {
  await ODoHConfigTest("invalid=configLength");
});

add_task(async function testODoHConfig4() {
  await ODoHConfigTest("invalid=totalLength");
});

add_task(async function testODoHConfig5() {
  await ODoHConfigTest("invalid=kemId");
});

add_task(async function testODoHConfig6() {
  // Use a very short TTL.
  prefs.setIntPref("network.trr.odoh.min_ttl", 1);
  await ODoHConfigTest("invalid=kemId&ttl=1");

  // This is triggered by the expiration of the TTL.
  await observerPromise("odoh-service-activated");
  Assert.ok(!dns.ODoHActivated);
  prefs.clearUserPref("network.trr.odoh.min_ttl");
});

add_task(async function testODoHConfig7() {
  dns.clearCache(true);
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setBoolPref("network.trr.odoh.enabled", true);
  // At this point, we've queried the ODoHConfig, but there is no usable config
  // (kemId is not supported). So, we should see the DNS result is from the
  // native resolver.
  await new DNSListener("bar.example.com", "127.0.0.1");
});

async function ODoHConfigTestHTTP(configUri, expectedResult) {
  // Setting the pref "network.trr.odoh.configs_uri" will trigger the reload of
  // the ODoHConfigs.
  prefs.setCharPref("network.trr.odoh.configs_uri", configUri);

  await observerPromise("odoh-service-activated");
  Assert.equal(dns.ODoHActivated, expectedResult);
}

add_task(async function testODoHConfig8() {
  dns.clearCache(true);
  prefs.setCharPref("network.trr.uri", "");

  await ODoHConfigTestHTTP(
    `https://foo.example.com:${h2Port}/odohconfig?downloadFrom=http`,
    true
  );
});

add_task(async function testODoHConfig9() {
  // Reset the prefs back to the correct values, so we will get valid
  // ODoHConfigs when "network.trr.odoh.configs_uri" is cleared below.
  prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/odohconfig`
  );
  prefs.setCharPref(
    "network.trr.odoh.target_host",
    `https://odoh_host.example.com:${h2Port}`
  );

  // Use a very short TTL.
  prefs.setIntPref("network.trr.odoh.min_ttl", 1);
  // This will trigger to download ODoHConfigs from HTTPS RR.
  prefs.clearUserPref("network.trr.odoh.configs_uri");

  await observerPromise("odoh-service-activated");
  Assert.ok(dns.ODoHActivated);

  await ODoHConfigTestHTTP(
    `https://foo.example.com:${h2Port}/odohconfig?downloadFrom=http`,
    true
  );

  // This is triggered by the expiration of the TTL.
  await observerPromise("odoh-service-activated");
  Assert.ok(dns.ODoHActivated);
  prefs.clearUserPref("network.trr.odoh.min_ttl");
});

add_task(async function testODoHConfig10() {
  // We can still get ODoHConfigs from HTTPS RR.
  await ODoHConfigTestHTTP("http://invalid_odoh_config.com", true);
});

// verify basic A record
add_task(async function test1() {
  dns.clearCache(true);
  prefs.setIntPref("network.trr.mode", 2); // TRR-first
  prefs.setBoolPref("network.trr.odoh.enabled", true);

  // Make sure we have an usable ODoHConfig to use.
  await ODoHConfigTestHTTP(
    `https://foo.example.com:${h2Port}/odohconfig?downloadFrom=http`,
    true
  );

  // Make sure we can connect to ODOH target successfully.
  prefs.setCharPref("network.dns.localDomains", "odoh_host.example.com");

  const expectedIP = "8.8.8.8";
  prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=${expectedIP}`
  );

  await new DNSListener("bar.example.com", expectedIP);
});

// verify basic A record - without bootstrapping
add_task(async function test1b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=3.3.3.3`
  );
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new DNSListener("bar.example.com", "3.3.3.3");

  Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");
});

// verify that the name was put in cache - it works with bad DNS URI
add_task(async function test2() {
  // Don't clear the cache. That is what we're checking.
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.odoh.target_path", `404`);

  await new DNSListener("bar.example.com", "3.3.3.3");
});

// verify working credentials in DOH request
add_task(async function test3() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=4.4.4.4&auth=true`
  );
  Services.prefs.setCharPref("network.trr.credentials", "user:password");

  await new DNSListener("bar.example.com", "4.4.4.4");
});

// verify failing credentials in ODOH request
add_task(async function test4() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=4.4.4.4&auth=true`
  );
  Services.prefs.setCharPref("network.trr.credentials", "evil:person");

  let [, , inStatus] = await new DNSListener(
    "wrong.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// verify AAAA entry
add_task(async function test6() {
  Services.prefs.setBoolPref("network.trr.wait-for-A-and-AAAA", true);

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2020&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2030&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2020&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2030&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2020`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false); // ignored when wait-for-A-and-AAAA is true
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2030`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  Services.prefs.setBoolPref("network.trr.wait-for-A-and-AAAA", false);

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2020&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2030&delayIPv4=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2020&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2030&delayIPv6=100`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2020`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2020");

  dns.clearCache(true);
  Services.prefs.setBoolPref("network.trr.early-AAAA", false);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2020:2020::2030`
  );
  await new DNSListener("aaaa.example.com", "2020:2020::2030");

  Services.prefs.clearUserPref("network.trr.early-AAAA");
  Services.prefs.clearUserPref("network.trr.wait-for-A-and-AAAA");
});

// verify RFC1918 address from the server is rejected
add_task(async function test7() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=192.168.0.1`
  );
  let [, , inStatus] = await new DNSListener(
    "rfc1918.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=::ffff:192.168.0.1`
  );
  [, , inStatus] = await new DNSListener(
    "rfc1918-ipv6.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// verify RFC1918 address from the server is fine when told so
add_task(async function test8() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=192.168.0.1`
  );
  Services.prefs.setBoolPref("network.trr.allow-rfc1918", true);
  await new DNSListener("rfc1918.example.com", "192.168.0.1");
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=::ffff:192.168.0.1`
  );
  await new DNSListener("rfc1918-ipv6.example.com", "::ffff:192.168.0.1");
});

// use GET and disable ECS (makes a larger request)
// verify URI template cutoff
add_task(async function test8b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.odoh.target_path", `odoh`);
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);
  await new DNSListener("ecs.example.com", "5.5.5.5");
});

// use GET
add_task(async function test9() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.odoh.target_path", `odoh`);
  Services.prefs.clearUserPref("network.trr.allow-rfc1918");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", false);
  await new DNSListener("get.example.com", "5.5.5.5");
});

// confirmationNS set without confirmed NS yet
add_task(async function test10() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.clearUserPref("network.trr.useGET");
  Services.prefs.clearUserPref("network.trr.disable-ECS");
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=1::ffff`
  );
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  await new DNSListener("skipConfirmationForMode3.example.com", "1::ffff");
});

// use a slow server and short timeout!
add_task(async function test11() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?noResponse=true`
  );
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);
  let [, , inStatus] = await new DNSListener(
    "test11.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// gets an no answers back from ODoH. Falls back to regular DNS
add_task(async function test12() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=none`
  );
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  await new DNSListener("confirm.example.com", "127.0.0.1");
});

// TRR-first gets a 404 back from ODOH
add_task(async function test13() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?httpError=true`
  );
  await new DNSListener("test13.example.com", "127.0.0.1");
});

// Test that MODE_RESERVED4 (previously MODE_SHADOW) is treated as TRR off.
add_task(async function test14() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?httpError=true`
  );
  await new DNSListener("test14.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// Test that MODE_RESERVED4 (previously MODE_SHADOW) is treated as TRR off.
add_task(async function test15() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?noResponse=true`
  );
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);
  await new DNSListener("test15.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first and timed out TRR
add_task(async function test16() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?noResponse=true`
  );
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10);
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 10);
  await new DNSListener("test16.example.com", "127.0.0.1");
});

// TRR-only and chase CNAME
add_task(async function test17() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?cname=content`
  );
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
  await new DNSListener("cname.example.com", "99.88.77.66");

  // Reset |cname_confirm| counter in moz-http2.js.
  let chan = makeChan(
    `https://foo.example.com:${h2Port}/reset_cname_confirm`,
    Ci.nsIRequest.TRR_DISABLED_MODE
  );
  await new Promise(resolve => chan.asyncOpen(new ChannelListener(resolve)));
});

// TRR-only and a CNAME loop
add_task(async function test18() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=none&cnameloop=true`
  );
  let [, , inStatus] = await new DNSListener(
    "test18.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// Test that MODE_RESERVED1 (previously MODE_PARALLEL) is treated as TRR off.
add_task(async function test19() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 1); // MODE_RESERVED1. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=none&cnameloop=true`
  );
  await new DNSListener("test19.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 1); // MODE_RESERVED1. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first and a CNAME loop
add_task(async function test20() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=none&cnameloop=true`
  );
  await new DNSListener("test20.example.com", "127.0.0.1");
});

// Test that MODE_RESERVED4 (previously MODE_SHADOW) is treated as TRR off.
add_task(async function test21() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=none&cnameloop=true`
  );
  await new DNSListener("test21.example.com", "127.0.0.1");

  Services.prefs.setIntPref("network.trr.mode", 4); // MODE_RESERVED4. Interpreted as TRR off.
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2`
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// verify that basic A record name mismatch gets rejected.
// Gets a response for bar.example.com instead of what it requested
add_task(async function test22() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only to avoid native fallback
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?hostname=bar.example.com`
  );
  let [, , inStatus] = await new DNSListener(
    "mismatch.example.com",
    undefined,
    false
  );
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

// TRR-only, with a CNAME response with a bundled A record for that CNAME!
add_task(async function test23() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?cname=ARecord`
  );
  await new DNSListener("cname-a.example.com", "9.8.7.6");
});

// TRR-first check that TRR result is used
add_task(async function test24() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=192.192.192.192`
  );
  await new DNSListener("bar.example.com", "192.192.192.192");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref
add_task(async function test24b() {
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "bar.example.com");
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref
add_task(async function test24c() {
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "example.com");
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref that contains things before it.
add_task(async function test24d() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "foo.test.com, bar.example.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the excluded-domains pref that contains things after it.
add_task(async function test24e() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "bar.example.com, foo.test.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

function observerPromise(topic) {
  return new Promise(resolve => {
    let observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      observe(aSubject, aTopic, aData) {
        dump(`observe: ${aSubject}, ${aTopic}, ${aData} \n`);
        if (aTopic == topic) {
          Services.obs.removeObserver(observer, topic);
          resolve(aData);
        }
      },
    };
    Services.obs.addObserver(observer, topic);
  });
}

// TRR-first check that captivedetect.canonicalURL is resolved via native DNS
add_task(async function test24f() {
  dns.clearCache(true);

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
  let cpPromise = observerPromise("captive-portal-login");

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
  await new DNSListener("detectportal.firefox.com", "192.192.192.192");

  Services.prefs.clearUserPref("network.captive-portal-service.enabled");
  Services.prefs.clearUserPref("network.captive-portal-service.testMode");
  Services.prefs.clearUserPref("captivedetect.canonicalURL");

  await new Promise(resolve => cpServer.stop(resolve));
});

// TRR-first check that a domain is resolved via native DNS when parental control is enabled.
add_task(async function test24g() {
  dns.clearCache(true);
  await SetParentalControlEnabled(true);
  await new DNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
});

// TRR-first check that DNS result is used if domain is part of the builtin-excluded-domains pref
add_task(async function test24h() {
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "bar.example.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the builtin-excluded-domains pref
add_task(async function test24i() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "example.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the builtin-excluded-domains pref that contains things before it.
add_task(async function test24j() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "foo.test.com, bar.example.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-first check that DNS result is used if domain is part of the builtin-excluded-domains pref that contains things after it.
add_task(async function test24k() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "bar.example.com, foo.test.com"
  );
  await new DNSListener("bar.example.com", "127.0.0.1");
});

// TRR-only that resolving excluded with TRR-only mode will use the remote
// resolver if it's not in the excluded domains
add_task(async function test25() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=192.192.192.192`
  );

  await new DNSListener("excluded", "192.192.192.192", true);
});

// TRR-only check that excluded goes directly to native lookup when in the excluded-domains
add_task(async function test25b() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded");

  await new DNSListener("excluded", "127.0.0.1");
});

// TRR-only check that test.local is resolved via native DNS
add_task(async function test25c() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded,local");

  await new DNSListener("test.local", "127.0.0.1");
});

// TRR-only check that .other is resolved via native DNS when the pref is set
add_task(async function test25d() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "excluded,local,other"
  );

  await new DNSListener("domain.other", "127.0.0.1");
});

// TRR-only check that a domain is resolved via native DNS when parental control is enabled.
add_task(async function test25f() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  await SetParentalControlEnabled(true);
  await new DNSListener("www.example.com", "127.0.0.1");
  await SetParentalControlEnabled(false);
});

// TRR-only check that excluded goes directly to native lookup when in the builtin-excluded-domains
add_task(async function test25g() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded"
  );

  await new DNSListener("excluded", "127.0.0.1");
});

// TRR-only check that test.local is resolved via native DNS
add_task(async function test25h() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded,local"
  );

  await new DNSListener("test.local", "127.0.0.1");
});

// TRR-only check that .other is resolved via native DNS when the pref is set
add_task(async function test25i() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref(
    "network.trr.builtin-excluded-domains",
    "excluded,local,other"
  );

  await new DNSListener("domain.other", "127.0.0.1");
});

// Check that none of the requests have set any cookies.
add_task(async function count_cookies() {
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  Assert.equal(cm.countCookiesFromHost("example.com"), 0);
  Assert.equal(cm.countCookiesFromHost("foo.example.com."), 0);
});

add_task(async function test_connection_closed() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  // We don't need to wait for 30 seconds for the request to fail
  Services.prefs.setIntPref("network.trr.request_timeout_mode_trronly_ms", 500);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2`
  );
  // bootstrap
  Services.prefs.setCharPref("network.trr.bootstrapAddr", "127.0.0.1");

  await new DNSListener("bar.example.com", "2.2.2.2");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new DNSListener("bar2.example.com", "2.2.2.2");
});

add_task(async function test_connection_closed_no_bootstrap() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "excluded,local");
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=3.3.3.3`
  );

  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new DNSListener("bar.example.com", "3.3.3.3");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  await new DNSListener("bar2.example.com", "3.3.3.3");
});

add_task(async function test_connection_closed_no_bootstrap_no_excluded() {
  // This test makes sure that even in mode 3 without a bootstrap address
  // we are able to restart the TRR connection if it drops - the TRR service
  // channel will use regular DNS to resolve the TRR address.
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3); // TRR-only
  Services.prefs.setCharPref("network.trr.excluded-domains", "");
  Services.prefs.setCharPref("network.trr.builtin-excluded-domains", "");
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new DNSListener("bar.example.com", "3.3.3.3");

  // makes the TRR connection shut down.
  let [, , inStatus] = await new DNSListener("closeme.com", undefined, false);
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
  dns.clearCache(true);
  await new DNSListener("bar2.example.com", "3.3.3.3");
});

add_task(async function test_connection_closed_trr_first() {
  // This test exists to document what happens when we're in TRR only mode
  // and we don't set a bootstrap address. We use DNS to resolve the
  // initial URI, but if the connection fails, we don't fallback to DNS
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=9.9.9.9`
  );
  Services.prefs.setCharPref(
    "network.dns.localDomains",
    "closeme.com, odoh_host.example.com"
  );
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");

  await new DNSListener("bar.example.com", "9.9.9.9");

  // makes the TRR connection shut down. Should fallback to DNS
  await new DNSListener("closeme.com", "127.0.0.1");
  // TRR should be back up again
  await new DNSListener("bar2.example.com", "9.9.9.9");

  // Remove closeme.com
  Services.prefs.setCharPref(
    "network.dns.localDomains",
    "odoh_host.example.com"
  );
});

// verify TRR timings
add_task(async function test_fetch_time() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2&delayIPv4=20`
  );

  await new DNSListener("bar_time.example.com", "2.2.2.2", true, 20);
});

// gets an error from DoH. It will fall back to regular DNS. The TRR timing should be 0.
add_task(async function test_no_fetch_time_on_trr_error() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?httpError=true&delayIPv4=20`
  );

  await new DNSListener("bar_time1.example.com", "127.0.0.1", true, 0);
});

// check an excluded domain. It should fall back to regular DNS. The TRR timing should be 0.
add_task(async function test_no_fetch_time_for_excluded_domains() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.excluded-domains",
    "bar_time2.example.com"
  );
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=2.2.2.2&delayIPv4=20`
  );

  await new DNSListener("bar_time2.example.com", "127.0.0.1", true, 0);

  Services.prefs.setCharPref("network.trr.excluded-domains", "");
});

// verify RFC1918 address from the server is rejected and the TRR timing will be not set because the response will be from the native resolver.
add_task(async function test_no_fetch_time_for_rfc1918_not_allowed() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=192.168.0.1&delayIPv4=20`
  );
  await new DNSListener("rfc1918_time.example.com", "127.0.0.1", true, 0);
});

// Tests that we handle FQDN encoding and decoding properly
add_task(async function test_fqdn() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=9.8.7.6`
  );
  await new DNSListener("fqdn.example.org.", "9.8.7.6");
});

add_task(async function test_fqdn_get() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setBoolPref("network.trr.useGET", true);

  await new DNSListener("fqdn_get.example.org.", "9.8.7.6");

  Services.prefs.setBoolPref("network.trr.useGET", false);
});

add_task(async function test_ipv6_trr_fallback() {
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

  Services.prefs.setIntPref("network.trr.mode", 2);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=4.4.4.4`
  );
  const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
    Ci.nsINativeDNSResolverOverride
  );
  gOverride.addIPOverride("ipv6.host.com", "1:1::2");

  await new DNSListener(
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
  // See bug 1648147 - if the TRR returns 0.0.0.0 we should not retry with DNS
  Services.prefs.setBoolPref("network.trr.fallback-on-zero-response", false);

  async function test(url, ip) {
    Services.prefs.setIntPref("network.trr.mode", 2);
    Services.prefs.setCharPref(
      "network.trr.odoh.target_path",
      `odoh?responseIP=${ip}`
    );

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
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", true);

  Services.prefs.setIntPref("network.trr.mode", 0);
  Services.prefs.setIntPref("doh-rollout.mode", 2);
  Services.prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=3.3.3.3`
  );

  await new DNSListener("cached.example.com", "3.3.3.3");
  Services.prefs.clearUserPref("doh-rollout.mode");

  await new DNSListener("cached.example.com", "127.0.0.1");

  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);
});
