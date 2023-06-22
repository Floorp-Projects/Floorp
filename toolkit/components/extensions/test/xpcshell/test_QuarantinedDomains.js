"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

AddonTestUtils.overrideCertDB();
AddonTestUtils.usePrivilegedSignatures = id => id.startsWith("privileged");

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

const server = createHttpServer({ hosts: ["example.org", "example.net"] });
server.registerPathHandler("/", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.write("<!DOCTYPE html><html></html>");
});

const ADDONS_RESTRICTED_DOMAINS_PREF =
  "extensions.webextensions.addons-restricted-domains@mozilla.com.disabled";

const DOMAINS = [
  "addons-dev.allizom.org",
  "mixed.badssl.com",
  "careers.mozilla.com",
  "developer.mozilla.org",
  "test.example.com",
];

const CAN_ACCESS_ALL = DOMAINS.reduce((map, domain) => {
  return { ...map, [domain]: true };
}, {});

function makePolicy(options) {
  return new WebExtensionPolicy({
    baseURL: "file:///foo/",
    localizeCallback: str => str,
    allowedOrigins: new MatchPatternSet(["<all_urls>"], { ignorePath: true }),
    mozExtensionHostname: Services.uuid.generateUUID().toString().slice(1, -1),
    ...options,
  });
}

function makeCS(policy) {
  return new WebExtensionContentScript(policy, {
    matches: new MatchPatternSet(["<all_urls>"]),
  });
}

function makeExtension({ id }) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id } },
      permissions: ["<all_urls>"],
      content_scripts: [
        {
          js: ["script.js"],
          matches: ["<all_urls>"],
        },
      ],
    },
    useAddonManager: "permanent",
    files: {
      "script.js": `
        browser.test.sendMessage("tld", location.host.split(".").at(-1));
        browser.test.sendMessage("cs");
      `,
    },
  });
}

function expectQuarantined(expectedDomains) {
  for (let domain of DOMAINS) {
    let uri = Services.io.newURI(`https://${domain}/`);
    let quarantined = expectedDomains.includes(domain);

    equal(
      quarantined,
      WebExtensionPolicy.isQuarantinedURI(uri),
      `Expect ${uri.spec} to ${quarantined ? "" : "not"} be quarantined.`
    );
  }
}

function expectAccess(policy, cs, expected) {
  for (let domain of Object.keys(expected)) {
    let uri = Services.io.newURI(`https://${domain}/`);
    let access = expected[domain];
    let match = access;

    equal(
      access,
      !policy.quarantinedFromURI(uri),
      `${policy.id} is ${access ? "not" : ""} quarantined from ${uri.spec}.`
    );
    equal(
      access,
      policy.canAccessURI(uri),
      `Expect ${policy.id} ${access ? "can" : "can't"} access ${uri.spec}.`
    );

    equal(
      match,
      cs.matchesURI(uri),
      `Expect ${cs.extension.id} to ${match ? "" : "not"} match ${uri.spec}.`
    );
  }
}

function expectHost(desc, host, quarantined) {
  let uri = Services.io.newURI(`https://${host}/`);
  equal(
    quarantined,
    WebExtensionPolicy.isQuarantinedURI(uri),
    `Expect ${desc} "${host}" to ${quarantined ? "" : "not"} be quarantined.`
  );
}

function makePolicies() {
  const plain = makePolicy({ id: "plain@test" });
  const system = makePolicy({ id: "system@test", isPrivileged: true });
  const exempt = makePolicy({ id: "exempt@test", ignoreQuarantine: true });

  return { plain, system, exempt };
}

function makeContentScripts(policies) {
  return policies.map(makeCS);
}

add_task(async function test_QuarantinedDomains() {
  const { plain, system, exempt } = makePolicies();
  const [plainCS, systemCS, exemptCS] = makeContentScripts([
    plain,
    system,
    exempt,
  ]);

  info("Initial pref state is an empty list.");
  expectQuarantined([]);

  expectAccess(plain, plainCS, CAN_ACCESS_ALL);
  expectAccess(system, systemCS, CAN_ACCESS_ALL);
  expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);

  info("Default test domain list.");
  Services.prefs.setStringPref(
    "extensions.quarantinedDomains.list",
    "addons-dev.allizom.org,mixed.badssl.com,test.example.com"
  );

  expectQuarantined([
    "addons-dev.allizom.org",
    "mixed.badssl.com",
    "test.example.com",
  ]);

  const EXPECT_DEFAULTS = {
    "addons-dev.allizom.org": false,
    "mixed.badssl.com": false,
    "careers.mozilla.com": true,
    "developer.mozilla.org": true,
    "test.example.com": false,
  };

  expectAccess(plain, plainCS, EXPECT_DEFAULTS);
  expectAccess(system, systemCS, CAN_ACCESS_ALL);
  expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);

  info("Test changing policy.ignoreQuarantine after creation.");

  ok(!plain.ignoreQuarantine, "plain policy does not ignore quarantine.");
  ok(system.ignoreQuarantine, "system policy does ignore quarantine.");
  ok(exempt.ignoreQuarantine, "exempt policy does ignore quarantine.");

  plain.ignoreQuarantine = true;
  system.ignoreQuarantine = false;
  exempt.ignoreQuarantine = false;

  ok(plain.ignoreQuarantine, "expect plain.ignoreQuarantine to be true.");
  ok(!system.ignoreQuarantine, "expect system.ignoreQuarantine to be false.");
  ok(!exempt.ignoreQuarantine, "expect exempt.ignoreQuarantine to be false.");

  expectAccess(plain, plainCS, CAN_ACCESS_ALL);
  expectAccess(system, systemCS, EXPECT_DEFAULTS);
  expectAccess(exempt, exemptCS, EXPECT_DEFAULTS);

  plain.ignoreQuarantine = false;
  system.ignoreQuarantine = true;
  exempt.ignoreQuarantine = true;

  expectAccess(plain, plainCS, EXPECT_DEFAULTS);
  expectAccess(system, systemCS, CAN_ACCESS_ALL);
  expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);

  info("Disable the Quarantined Domains feature.");
  Services.prefs.setBoolPref("extensions.quarantinedDomains.enabled", false);
  expectQuarantined([]);

  expectAccess(plain, plainCS, CAN_ACCESS_ALL);
  expectAccess(system, systemCS, CAN_ACCESS_ALL);
  expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);

  info(
    "Enable again, drop addons-dev.allizom.org and add developer.mozilla.org to the pref."
  );
  Services.prefs.setBoolPref("extensions.quarantinedDomains.enabled", true);

  Services.prefs.setStringPref(
    "extensions.quarantinedDomains.list",
    "mixed.badssl.com,developer.mozilla.org,test.example.com"
  );
  expectQuarantined([
    "mixed.badssl.com",
    "developer.mozilla.org",
    "test.example.com",
  ]);

  expectAccess(plain, plainCS, {
    "addons-dev.allizom.org": true,
    "mixed.badssl.com": false,
    "careers.mozilla.com": true,
    "developer.mozilla.org": false,
    "test.example.com": false,
  });

  expectAccess(system, systemCS, CAN_ACCESS_ALL);
  expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);

  expectHost("host with a port", "test.example.com:1025", true);

  expectHost("FQDN", "test.example.com.", false);
  expectHost("subdomain", "subdomain.test.example.com", false);
  expectHost("domain with prefix", "pretest.example.com", false);
  expectHost("domain with suffix", "test.example.comsuf", false);
});

function setIgnorePref(id, value = false) {
  Services.prefs.setBoolPref(`extensions.quarantineIgnoredByUser.${id}`, value);
}

// Test that ignore prefs take effect in the content process.
add_task(
  { pref_set: [["extensions.quarantinedDomains.list", "example.net"]] },
  async function test_QuarantinedDomains_ignored() {
    const EXPECT_DEFAULTS = { "example.org": true, "example.net": false };
    const CAN_ACCESS_ALL = { "example.org": true, "example.net": true };

    let alpha = makeExtension({ id: "alpha@test" });
    let beta = makeExtension({ id: "beta@test" });
    let system = makeExtension({ id: "privileged@test" });

    let page = await ExtensionTestUtils.loadContentPage("about:blank");

    await alpha.startup();
    await beta.startup();
    await system.startup();

    equal(system.extension.isPrivileged, true, "is privileged");

    let alphaPolicy = alpha.extension.policy;
    let betaPolicy = beta.extension.policy;

    let alphaCounters = { org: 0, net: 0 };
    let betaCounters = { org: 0, net: 0 };

    alpha.onMessage("tld", tld => alphaCounters[tld]++);
    beta.onMessage("tld", tld => betaCounters[tld]++);
    system.onMessage("tld", () => {});

    async function testTLD(tld, expectAlpha, expectBeta) {
      let alphaCount = alphaCounters[tld];
      let betaCount = betaCounters[tld];

      await page.loadURL(`http://example.${tld}/`);
      if (expectAlpha) {
        await alpha.awaitMessage("cs");
        alphaCount++;
      }
      if (expectBeta) {
        await beta.awaitMessage("cs");
        betaCount++;
      }
      // Sanity check, plus always having something to await.
      await system.awaitMessage("cs");

      equal(alphaCount, alphaCounters[tld], `Expected ${tld} alpha CS counter`);
      equal(betaCount, betaCounters[tld], `Expected ${tld} beta CS counter`);
    }

    info("Test defaults, example.org is accessible, example.net is not.");

    await testTLD("org", true, true);
    await testTLD("net", false, false);

    expectAccess(alphaPolicy, alphaPolicy.contentScripts[0], EXPECT_DEFAULTS);
    expectAccess(betaPolicy, betaPolicy.contentScripts[0], EXPECT_DEFAULTS);

    info("Test setting the pref for alpha@test.");
    setIgnorePref("alpha@test", true);

    await testTLD("net", true, false);
    await testTLD("org", true, true);

    expectAccess(alphaPolicy, alphaPolicy.contentScripts[0], CAN_ACCESS_ALL);
    expectAccess(betaPolicy, betaPolicy.contentScripts[0], EXPECT_DEFAULTS);

    info("Test setting the pref for beta@test.");
    setIgnorePref("beta@test", true);

    await testTLD("org", true, true);
    await testTLD("net", true, true);
    expectAccess(betaPolicy, betaPolicy.contentScripts[0], CAN_ACCESS_ALL);

    info("Test unsetting the pref for alpha@test.");
    setIgnorePref("alpha@test", false);

    await testTLD("net", false, true);
    await testTLD("org", true, true);

    info("Test unsetting the pref for beta@test.");
    setIgnorePref("beta@test", false);

    await testTLD("org", true, true);
    await testTLD("net", false, false);

    expectAccess(alphaPolicy, alphaPolicy.contentScripts[0], EXPECT_DEFAULTS);
    expectAccess(betaPolicy, betaPolicy.contentScripts[0], EXPECT_DEFAULTS);

    Assert.deepEqual(
      alphaCounters,
      { org: 5, net: 2 },
      "Expected final Alpha content script counters."
    );

    Assert.deepEqual(
      betaCounters,
      { org: 5, net: 2 },
      "Expected final Beta content script counters."
    );

    await system.unload();
    await beta.unload();
    await alpha.unload();

    await page.close();
  }
);

// Make sure we honor the system add-on pref.
add_task(
  {
    pref_set: [
      [ADDONS_RESTRICTED_DOMAINS_PREF, true],
      [
        "extensions.quarantinedDomains.list",
        "addons-dev.allizom.org,mixed.badssl.com,test.example.com",
      ],
    ],
  },
  async function test_QuarantinedDomains_with_system_addon_disabled() {
    await AddonTestUtils.promiseRestartManager();

    const { plain, system, exempt } = makePolicies();
    const [plainCS, systemCS, exemptCS] = makeContentScripts([
      plain,
      system,
      exempt,
    ]);

    expectQuarantined([]);
    expectAccess(plain, plainCS, CAN_ACCESS_ALL);
    expectAccess(system, systemCS, CAN_ACCESS_ALL);
    expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);

    // When the user changes this pref to re-enable the system add-on...
    Services.prefs.setBoolPref(ADDONS_RESTRICTED_DOMAINS_PREF, false);
    // ...after a AOM restart...
    await AddonTestUtils.promiseRestartManager();
    // ...we expect no change.
    expectQuarantined([]);
    expectAccess(plain, plainCS, CAN_ACCESS_ALL);
    expectAccess(system, systemCS, CAN_ACCESS_ALL);
    expectAccess(exempt, exemptCS, CAN_ACCESS_ALL);
  }
);
