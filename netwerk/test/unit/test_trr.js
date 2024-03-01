"use strict";

/* import-globals-from trr_common.js */

const gDefaultPref = Services.prefs.getDefaultBranch("");

SetParentalControlEnabled(false);

function setup() {
  Services.prefs.setBoolPref("network.dns.get-ttl", false);
  h2Port = trr_test_setup();
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.dns.get-ttl");
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
    let { inRecord } = await new TRRDNSListener(
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

function setModeAndURI(mode, path) {
  Services.prefs.setIntPref("network.trr.mode", mode);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://${TRR_Domain}:${h2Port}/${path}`
  );
}

function makeChan(url, mode) {
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
  Services.prefs.setIntPref("network.trr.request_timeout_ms", 10000);
  Services.prefs.setIntPref(
    "network.trr.request_timeout_mode_trronly_ms",
    10000
  );

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
      Services.dns.clearCache(true);
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
  Services.prefs.clearUserPref("network.trr.request_timeout_ms");
  Services.prefs.clearUserPref("network.trr.request_timeout_mode_trronly_ms");
});

add_task(test_A_record);

add_task(async function test_push() {
  info("Verify DOH push");
  Services.dns.clearCache(true);
  info("Asking server to push us a record");
  setModeAndURI(3, "doh?responseIP=5.5.5.5&push=true");

  await new TRRDNSListener("first.example.com", "5.5.5.5");

  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  // Don't clear the cache, otherwise we lose the pushed record.
  setModeAndURI(3, "404");

  await new TRRDNSListener("push.example.org", "2018::2018");
});

add_task(test_AAAA_records);

add_task(test_RFC1918);

add_task(test_GET_ECS);

add_task(test_timeout_mode3);

add_task(test_trr_retry);

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

add_task(test_connection_closed);

add_task(async function test_clearCacheOnURIChange() {
  info("Check that the TRR cache should be cleared by a pref change.");
  Services.dns.clearCache(true);
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
    Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  let { inStatus } = await new TRRDNSListener(
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=2.2.2.2");

  ({ inStatus } = await new TRRDNSListener(
    "only_once.example.com",
    undefined,
    false,
    undefined,
    `https://target.example.com:666/404`
  ));
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );

  // // MOZ_LOG=sync,timestamp,nsHostResolver:5 We should not keep resolving only_once.example.com
  // // TODO: find a way of automating this
  // await new Promise(resolve => {});
});

add_task(test_fetch_time);

add_task(async function test_content_encoding_gzip() {
  info("Checking gzip content encoding");
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?redirect=4.4.4.4{&dns}");
  Services.prefs.setBoolPref("network.trr.useGET", true);
  Services.prefs.setBoolPref("network.trr.disable-ECS", true);

  await new TRRDNSListener("ecs.example.com", "4.4.4.4");

  // POST
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
  setModeAndURI(3, "doh?responseIP=1::ffff&slowConfirm=true");
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  await new TRRDNSListener("skipConfirmationForMode3.example.com", "1::ffff");

  // Reset between each test to force re-confirm
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");

  Services.dns.clearCache(true);
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
  Services.dns.clearCache(true);
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

add_task(test_fqdn);

add_task(async function test_detected_uri() {
  info("Test setDetectedTrrURI");
  Services.dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.clearUserPref("network.trr.uri");
  let defaultURI = gDefaultPref.getCharPref("network.trr.default_provider_uri");
  gDefaultPref.setCharPref(
    "network.trr.default_provider_uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.4.5.6`
  );
  await new TRRDNSListener("domainA.example.org.", "3.4.5.6");
  Services.dns.setDetectedTrrURI(
    `https://foo.example.com:${h2Port}/doh?responseIP=1.2.3.4`
  );
  await new TRRDNSListener("domainB.example.org.", "1.2.3.4");
  gDefaultPref.setCharPref("network.trr.default_provider_uri", defaultURI);

  // With a user-set doh uri this time.
  Services.dns.clearCache(true);
  setModeAndURI(2, "doh?responseIP=4.5.6.7");
  await new TRRDNSListener("domainA.example.org.", "4.5.6.7");

  // This should be a no-op, since we have a user-set URI
  Services.dns.setDetectedTrrURI(
    `https://foo.example.com:${h2Port}/doh?responseIP=1.2.3.4`
  );
  await new TRRDNSListener("domainB.example.org.", "4.5.6.7");

  // Test network link status change
  Services.dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.clearUserPref("network.trr.uri");
  gDefaultPref.setCharPref(
    "network.trr.default_provider_uri",
    `https://foo.example.com:${h2Port}/doh?responseIP=3.4.5.6`
  );
  await new TRRDNSListener("domainA.example.org.", "3.4.5.6");
  Services.dns.setDetectedTrrURI(
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

  gDefaultPref.setCharPref("network.trr.default_provider_uri", defaultURI);
});

add_task(async function test_pref_changes() {
  info("Testing pref change handling");
  Services.prefs.clearUserPref("network.trr.uri");
  let defaultURI = gDefaultPref.getCharPref("network.trr.default_provider_uri");

  async function doThenCheckURI(closure, expectedURI, expectChange = true) {
    let uriChanged;
    if (expectChange) {
      uriChanged = topicObserved("network:trr-uri-changed");
    }
    closure();
    if (expectChange) {
      await uriChanged;
    }
    equal(Services.dns.currentTrrURI, expectedURI);
  }

  // setting the default value of the pref should be reflected in the URI
  await doThenCheckURI(() => {
    gDefaultPref.setCharPref(
      "network.trr.default_provider_uri",
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
    Services.dns.setDetectedTrrURI(
      `https://foo.example.com:${h2Port}/doh?detected`
    );
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
      Services.dns.setDetectedTrrURI(
        `https://foo.example.com:${h2Port}/doh?detected`
      );
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
  gDefaultPref.setCharPref("network.trr.default_provider_uri", defaultURI);
});

add_task(async function test_dohrollout_mode() {
  info("Testing doh-rollout.mode");
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("doh-rollout.mode");

  equal(Services.dns.currentTrrMode, 0);

  async function doThenCheckMode(trrMode, rolloutMode, expectedMode, message) {
    let modeChanged;
    if (Services.dns.currentTrrMode != expectedMode) {
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
    equal(Services.dns.currentTrrMode, expectedMode, message);
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
  equal(Services.dns.currentTrrMode, 2);
  Services.prefs.clearUserPref("doh-rollout.mode");
  equal(Services.dns.currentTrrMode, 0);
});

add_task(test_ipv6_trr_fallback);

add_task(test_ipv4_trr_fallback);

add_task(test_no_retry_without_doh);

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
  Services.dns.clearCache(true);
  // Note this is a remote address. Setting this pref should have no effect,
  // as this is the old name for the bootstrap pref.
  // If this were to be used, the test would crash when accessing a non-local
  // IP address.
  Services.prefs.setCharPref("network.trr.bootstrapAddress", "1.1.1.1");
  setModeAndURI(Ci.nsIDNSService.MODE_TRRONLY, `doh?responseIP=4.4.4.4`);
  await new TRRDNSListener("testytest.com", "4.4.4.4");
});

add_task(async function test_padding() {
  setModeAndURI(Ci.nsIDNSService.MODE_TRRONLY, `doh`);
  async function CheckPadding(
    pad_length,
    request,
    none,
    ecs,
    padding,
    ecsPadding
  ) {
    Services.prefs.setIntPref("network.trr.padding.length", pad_length);
    Services.dns.clearCache(true);
    Services.prefs.setBoolPref("network.trr.padding", false);
    Services.prefs.setBoolPref("network.trr.disable-ECS", false);
    await new TRRDNSListener(request, none);

    Services.dns.clearCache(true);
    Services.prefs.setBoolPref("network.trr.padding", false);
    Services.prefs.setBoolPref("network.trr.disable-ECS", true);
    await new TRRDNSListener(request, ecs);

    Services.dns.clearCache(true);
    Services.prefs.setBoolPref("network.trr.padding", true);
    Services.prefs.setBoolPref("network.trr.disable-ECS", false);
    await new TRRDNSListener(request, padding);

    Services.dns.clearCache(true);
    Services.prefs.setBoolPref("network.trr.padding", true);
    Services.prefs.setBoolPref("network.trr.disable-ECS", true);
    await new TRRDNSListener(request, ecsPadding);
  }

  // short domain name
  await CheckPadding(
    16,
    "a.pd",
    "2.2.0.22",
    "2.2.0.41",
    "1.1.0.48",
    "1.1.0.48"
  );
  await CheckPadding(256, "a.pd", "2.2.0.22", "2.2.0.41", "1.1.1.0", "1.1.1.0");

  // medium domain name
  await CheckPadding(
    16,
    "has-padding.pd",
    "2.2.0.32",
    "2.2.0.51",
    "1.1.0.48",
    "1.1.0.64"
  );
  await CheckPadding(
    128,
    "has-padding.pd",
    "2.2.0.32",
    "2.2.0.51",
    "1.1.0.128",
    "1.1.0.128"
  );
  await CheckPadding(
    80,
    "has-padding.pd",
    "2.2.0.32",
    "2.2.0.51",
    "1.1.0.80",
    "1.1.0.80"
  );

  // long domain name
  await CheckPadding(
    16,
    "abcdefghijklmnopqrstuvwxyz0123456789.abcdefghijklmnopqrstuvwxyz0123456789.abcdefghijklmnopqrstuvwxyz0123456789.pd",
    "2.2.0.131",
    "2.2.0.150",
    "1.1.0.160",
    "1.1.0.160"
  );
  await CheckPadding(
    128,
    "abcdefghijklmnopqrstuvwxyz0123456789.abcdefghijklmnopqrstuvwxyz0123456789.abcdefghijklmnopqrstuvwxyz0123456789.pd",
    "2.2.0.131",
    "2.2.0.150",
    "1.1.1.0",
    "1.1.1.0"
  );
  await CheckPadding(
    80,
    "abcdefghijklmnopqrstuvwxyz0123456789.abcdefghijklmnopqrstuvwxyz0123456789.abcdefghijklmnopqrstuvwxyz0123456789.pd",
    "2.2.0.131",
    "2.2.0.150",
    "1.1.0.160",
    "1.1.0.160"
  );
});

add_task(test_connection_reuse_and_cycling);

// Can't test for socket process since telemetry is captured in different process.
add_task(
  { skip_if: () => mozinfo.socketprocess_networking },
  async function test_trr_pb_telemetry() {
    setModeAndURI(Ci.nsIDNSService.MODE_TRRONLY, `doh`);
    Services.dns.clearCache(true);
    Services.fog.initializeFOG();
    Services.fog.testResetFOG();
    await new TRRDNSListener("testytest.com", { expectedAnswer: "5.5.5.5" });

    Assert.equal(
      await Glean.networking.trrRequestCount.regular.testGetValue(),
      2
    ); // One for IPv4 and one for IPv6.
    Assert.equal(
      await Glean.networking.trrRequestCount.private.testGetValue(),
      null
    );

    await new TRRDNSListener("testytest.com", {
      expectedAnswer: "5.5.5.5",
      originAttributes: { privateBrowsingId: 1 },
    });

    Assert.equal(
      await Glean.networking.trrRequestCount.regular.testGetValue(),
      2
    );
    Assert.equal(
      await Glean.networking.trrRequestCount.private.testGetValue(),
      2
    );
  }
);
