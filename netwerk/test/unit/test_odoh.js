/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Most of tests in this file are reused from test_trr.js, except tests listed
 * below. Since ODoH doesn't support push and customerized DNS resolver, some
 * related tests are skipped.
 *
 * test_trr_flags
 * test_push
 * test_clearCacheOnURIChange // TODO: Clear DNS cache when ODoH prefs changed.
 * test_dnsSuffix
 * test_async_resolve_with_trr_server
 * test_content_encoding_gzip
 * test_redirect
 * test_confirmation
 * test_detected_uri
 * test_pref_changes
 * test_dohrollout_mode
 * test_purge_trr_cache_on_mode_change
 * test_old_bootstrap_pref
 */

"use strict";

/* import-globals-from trr_common.js */

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function setup() {
  h2Port = trr_test_setup();
  runningODoHTests = true;

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  // This is for skiping the security check for the odoh host.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
}

setup();
registerCleanupFunction(() => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.trr.odoh.enabled");
  Services.prefs.clearUserPref("network.trr.odoh.target_path");
  Services.prefs.clearUserPref("network.trr.odoh.configs_uri");
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

add_task(async function testODoHConfig() {
  // use the h2 server as DOH provider
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/odohconfig"
  );

  let [, inRecord] = await new TRRDNSListener("odoh_host.example.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });
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
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/odohconfig?" + query
  );

  // Setting the pref "network.trr.odoh.target_host" will trigger the reload of
  // the ODoHConfigs.
  if (ODoHHost != undefined) {
    Services.prefs.setCharPref("network.trr.odoh.target_host", ODoHHost);
  } else {
    Services.prefs.setCharPref(
      "network.trr.odoh.target_host",
      `https://odoh_host_${testIndex++}.com`
    );
  }

  await topicObserved("odoh-service-activated");
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
  Services.prefs.setIntPref("network.trr.odoh.min_ttl", 1);
  await ODoHConfigTest("invalid=kemId&ttl=1");

  // This is triggered by the expiration of the TTL.
  await topicObserved("odoh-service-activated");
  Assert.ok(!dns.ODoHActivated);
  Services.prefs.clearUserPref("network.trr.odoh.min_ttl");
});

add_task(async function testODoHConfig7() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2); // TRR-first
  Services.prefs.setBoolPref("network.trr.odoh.enabled", true);
  // At this point, we've queried the ODoHConfig, but there is no usable config
  // (kemId is not supported). So, we should see the DNS result is from the
  // native resolver.
  await new TRRDNSListener("bar.example.com", "127.0.0.1");
});

async function ODoHConfigTestHTTP(configUri, expectedResult) {
  // Setting the pref "network.trr.odoh.configs_uri" will trigger the reload of
  // the ODoHConfigs.
  Services.prefs.setCharPref("network.trr.odoh.configs_uri", configUri);

  await topicObserved("odoh-service-activated");
  Assert.equal(dns.ODoHActivated, expectedResult);
}

add_task(async function testODoHConfig8() {
  dns.clearCache(true);
  Services.prefs.setCharPref("network.trr.uri", "");

  await ODoHConfigTestHTTP(
    `https://foo.example.com:${h2Port}/odohconfig?downloadFrom=http`,
    true
  );
});

add_task(async function testODoHConfig9() {
  // Reset the prefs back to the correct values, so we will get valid
  // ODoHConfigs when "network.trr.odoh.configs_uri" is cleared below.
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${h2Port}/odohconfig`
  );
  Services.prefs.setCharPref(
    "network.trr.odoh.target_host",
    `https://odoh_host.example.com:${h2Port}`
  );

  // Use a very short TTL.
  Services.prefs.setIntPref("network.trr.odoh.min_ttl", 1);
  // This will trigger to download ODoHConfigs from HTTPS RR.
  Services.prefs.clearUserPref("network.trr.odoh.configs_uri");

  await topicObserved("odoh-service-activated");
  Assert.ok(dns.ODoHActivated);

  await ODoHConfigTestHTTP(
    `https://foo.example.com:${h2Port}/odohconfig?downloadFrom=http`,
    true
  );

  // This is triggered by the expiration of the TTL.
  await topicObserved("odoh-service-activated");
  Assert.ok(dns.ODoHActivated);
  Services.prefs.clearUserPref("network.trr.odoh.min_ttl");
});

add_task(async function testODoHConfig10() {
  // We can still get ODoHConfigs from HTTPS RR.
  await ODoHConfigTestHTTP("http://invalid_odoh_config.com", true);
});

add_task(async function ensureODoHConfig() {
  Services.prefs.setBoolPref("network.trr.odoh.enabled", true);
  // Make sure we can connect to ODOH target successfully.
  Services.prefs.setCharPref(
    "network.dns.localDomains",
    "odoh_host.example.com"
  );

  // Make sure we have an usable ODoHConfig to use.
  await ODoHConfigTestHTTP(
    `https://foo.example.com:${h2Port}/odohconfig?downloadFrom=http`,
    true
  );
});

add_task(test_A_record);

add_task(test_AAAA_records);

add_task(test_RFC1918);

add_task(test_GET_ECS);

add_task(test_timeout_mode3);

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

add_task(test_connection_closed);

add_task(test_fetch_time);

add_task(test_fqdn);

add_task(test_ipv6_trr_fallback);

add_task(test_no_retry_without_doh);
