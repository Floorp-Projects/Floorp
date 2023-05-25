"use strict";

const DOMAINS = [
  "addons-dev.allizom.org",
  "mixed.badssl.com",
  "careers.mozilla.com",
  "developer.mozilla.org",
  "test.example.com",
];

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

add_task(async function test_QuarantinedDomains() {
  let plain = makePolicy({ id: "plain@test" });
  let system = makePolicy({ id: "system@test", isPrivileged: true });
  let exempt = makePolicy({ id: "exempt@test", ignoreQuarantine: true });

  let plainCS = makeCS(plain);
  let systemCS = makeCS(system);
  let exemptCS = makeCS(exempt);

  const canAccessAll = {
    "addons-dev.allizom.org": true,
    "mixed.badssl.com": true,
    "careers.mozilla.com": true,
    "developer.mozilla.org": true,
    "test.example.com": true,
  };

  info("Initial pref state is an empty list.");
  expectQuarantined([]);

  expectAccess(plain, plainCS, canAccessAll);
  expectAccess(system, systemCS, canAccessAll);
  expectAccess(exempt, exemptCS, canAccessAll);

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

  expectAccess(system, systemCS, canAccessAll);
  expectAccess(exempt, exemptCS, canAccessAll);

  info("Disable the Quarantined Domains feature.");
  Services.prefs.setBoolPref("extensions.quarantinedDomains.enabled", false);
  expectQuarantined([]);

  expectAccess(plain, plainCS, canAccessAll);
  expectAccess(system, systemCS, canAccessAll);
  expectAccess(exempt, exemptCS, canAccessAll);

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

  expectAccess(system, systemCS, canAccessAll);
  expectAccess(exempt, exemptCS, canAccessAll);

  expectHost("host with a port", "test.example.com:1025", true);

  expectHost("FQDN", "test.example.com.", false);
  expectHost("subdomain", "subdomain.test.example.com", false);
  expectHost("domain with prefix", "pretest.example.com", false);
  expectHost("domain with suffix", "test.example.comsuf", false);
});
