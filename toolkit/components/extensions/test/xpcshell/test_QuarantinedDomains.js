"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

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
  for (let domain of DOMAINS) {
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

  expectAccess(plain, plainCS, {
    "addons-dev.allizom.org": false,
    "mixed.badssl.com": false,
    "careers.mozilla.com": true,
    "developer.mozilla.org": true,
    "test.example.com": false,
  });

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
