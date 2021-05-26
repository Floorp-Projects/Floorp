/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 *  Test for SecuritySettingsCleaner.
 */

"use strict";

let gSSService = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

function addSecurityInfo({ host, topLevelBaseDomain, originAttributes = {} }) {
  let uri = Services.io.newURI(`https://${host}`);

  let secInfo = Cc[
    "@mozilla.org/security/transportsecurityinfo;1"
  ].createInstance(Ci.nsITransportSecurityInfo);

  gSSService.processHeader(
    Ci.nsISiteSecurityService.HEADER_HSTS,
    uri,
    "max-age=1000;",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST,
    getOAWithPartitionKey(topLevelBaseDomain, originAttributes)
  );
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
  expected = true,
  originAttributes = {},
}) {
  let uri = Services.io.newURI(`https://${host}`);
  let isSecure = gSSService.isSecureURI(
    Ci.nsISiteSecurityService.HEADER_HSTS,
    uri,
    0,
    getOAWithPartitionKey(topLevelBaseDomain, originAttributes)
  );

  let message = `HSTS ${expected ? "is set" : "is not set"} for ${host}`;
  if (topLevelBaseDomain) {
    message += ` partitioned under ${topLevelBaseDomain}`;
  }
  Assert.equal(isSecure, expected, message);
}

add_task(async function test_baseDomain() {
  gSSService.clearAll();
  addTestSecurityInfo();

  // Clear security settings of example.net including partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS,
      aResolve
    );
  });

  testSecurityInfo({ host: "example.net", expected: false });
  // SecuritySettingsCleaner also removes subdomain settings.
  testSecurityInfo({ host: "test.example.net", expected: false });
  testSecurityInfo({ host: "example.org" });

  testSecurityInfo({
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expected: false,
  });
  testSecurityInfo({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });

  // Cleanup
  gSSService.clearAll();
});

add_task(async function test_host() {
  gSSService.clearAll();
  addTestSecurityInfo();

  // Clear security settings of example.net without partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS,
      aResolve
    );
  });

  testSecurityInfo({ host: "example.net", expected: false });
  testSecurityInfo({ host: "test.example.net", expected: false });
  testSecurityInfo({ host: "example.org" });

  testSecurityInfo({ host: "example.com", topLevelBaseDomain: "example.net" });
  testSecurityInfo({
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });

  // Cleanup
  gSSService.clearAll();
});
