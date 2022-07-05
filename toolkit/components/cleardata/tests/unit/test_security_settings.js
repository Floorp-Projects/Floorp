/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 *  Test for SecuritySettingsCleaner.
 *  This tests both, the SiteSecurityService and the ClientAuthRememberService.
 */

"use strict";

let gSSService = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

let cars = Cc["@mozilla.org/security/clientAuthRememberService;1"].getService(
  Ci.nsIClientAuthRememberService
);

let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

// These are not actual server and client certs. The ClientAuthRememberService
// does not care which certs we store decisions for, as long as they're valid.
let [serverCert, clientCert] = certDB.getCerts();

function addSecurityInfo({ host, topLevelBaseDomain, originAttributes = {} }) {
  let attrs = getOAWithPartitionKey({ topLevelBaseDomain }, originAttributes);

  let uri = Services.io.newURI(`https://${host}`);
  let secInfo = Cc[
    "@mozilla.org/security/transportsecurityinfo;1"
  ].createInstance(Ci.nsITransportSecurityInfo);

  gSSService.processHeader(uri, "max-age=1000;", secInfo, attrs);

  cars.rememberDecisionScriptable(host, attrs, serverCert, clientCert);
}

function addTestSecurityInfo() {
  // First party
  addSecurityInfo({ host: "example.net" });
  addSecurityInfo({ host: "test.example.net" });
  addSecurityInfo({ host: "example.org" });

  // Third-party partitioned
  addSecurityInfo({ host: "example.com", topLevelBaseDomain: "example.net" });
  addSecurityInfo({ host: "example.net", topLevelBaseDomain: "example.org" });
  addSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });

  // Ensure we have the correct state initially.
  testSecurityInfo({ host: "example.net" });
  testSecurityInfo({ host: "test.example.net" });
  testSecurityInfo({ host: "example.org" });
  testSecurityInfo({ host: "example.com", topLevelBaseDomain: "example.net" });
  testSecurityInfo({ host: "example.net", topLevelBaseDomain: "example.org" });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });
}

function testSecurityInfo({
  host,
  topLevelBaseDomain,
  expectedHSTS = true,
  expectedCARS = true,
  originAttributes = {},
}) {
  let attrs = getOAWithPartitionKey({ topLevelBaseDomain }, originAttributes);

  let messageSuffix = `for ${host}`;
  if (topLevelBaseDomain) {
    messageSuffix += ` partitioned under ${topLevelBaseDomain}`;
  }

  let uri = Services.io.newURI(`https://${host}`);
  let isSecure = gSSService.isSecureURI(uri, attrs);
  Assert.equal(
    isSecure,
    expectedHSTS,
    `HSTS ${expectedHSTS ? "is set" : "is not set"} ${messageSuffix}`
  );

  let hasRemembered = cars.hasRememberedDecisionScriptable(
    host,
    attrs,
    serverCert,
    {}
  );
  // CARS deleteDecisionsByHost does not include subdomains. That means for some
  // test cases we expect a different remembered state.
  Assert.equal(
    hasRemembered,
    expectedCARS,
    `CAR ${expectedCARS ? "is set" : "is not set"} ${messageSuffix}`
  );
}

add_task(async function test_baseDomain() {
  gSSService.clearAll();

  // ---- hsts cleaner ----
  addTestSecurityInfo();

  // Clear hsts data of example.net including partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_HSTS,
      aResolve
    );
  });

  testSecurityInfo({
    host: "example.net",
    expectedHSTS: false,
    expectedCARS: true,
  });
  // HSTSCleaner also removes subdomain settings.
  testSecurityInfo({
    host: "test.example.net",
    expectedHSTS: false,
    expectedCARS: true,
  });
  testSecurityInfo({ host: "example.org" });

  testSecurityInfo({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expectedHSTS: false,
    expectedCARS: true,
  });
  testSecurityInfo({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: false,
    expectedCARS: true,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: false,
    expectedCARS: true,
  });

  // ---- client auth remember cleaner -----
  addTestSecurityInfo();

  // Clear security settings of example.net including partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_CLIENT_AUTH_REMEMBER_SERVICE,
      aResolve
    );
  });

  testSecurityInfo({
    host: "example.net",
    expectedHSTS: true,
    expectedCARS: false,
  });
  // ClientAuthRememberCleaner also removes subdomain settings.
  testSecurityInfo({
    host: "test.example.net",
    expectedHSTS: true,
    expectedCARS: false,
  });
  testSecurityInfo({ host: "example.org" });

  testSecurityInfo({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expectedHSTS: true,
    expectedCARS: false,
  });
  testSecurityInfo({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: true,
    expectedCARS: false,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: true,
    expectedCARS: false,
  });

  // Cleanup
  gSSService.clearAll();
});

add_task(async function test_host() {
  gSSService.clearAll();

  // ---- HSTS cleaer ----
  addTestSecurityInfo();

  // Clear security settings of example.net without partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_HSTS,
      aResolve
    );
  });

  testSecurityInfo({
    host: "example.net",
    expectedHSTS: false,
    expectedCARS: true,
  });
  testSecurityInfo({
    host: "test.example.net",
    expectedHSTS: false,
    expectedCARS: true,
  });
  testSecurityInfo({ host: "example.org" });

  testSecurityInfo({ host: "example.com", topLevelBaseDomain: "example.net" });
  testSecurityInfo({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: false,
    expectedCARS: true,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: false,
    expectedCARS: true,
  });

  // Cleanup
  gSSService.clearAll();

  // --- clientAuthRemember cleaner ---

  addTestSecurityInfo();

  // Clear security settings of example.net without partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_CLIENT_AUTH_REMEMBER_SERVICE,
      aResolve
    );
  });

  testSecurityInfo({
    host: "example.net",
    expectedHSTS: true,
    expectedCARS: false,
  });
  testSecurityInfo({
    host: "test.example.net",
    expectedHSTS: true,
    expectedCARS: true,
  });
  testSecurityInfo({ host: "example.org" });

  testSecurityInfo({ host: "example.com", topLevelBaseDomain: "example.net" });
  testSecurityInfo({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: true,
    expectedCARS: false,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expectedHSTS: true,
    expectedCARS: true,
  });

  // Cleanup
  gSSService.clearAll();
});
