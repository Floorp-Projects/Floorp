/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

let cars = Cc["@mozilla.org/security/clientAuthRememberService;1"].getService(
  Ci.nsIClientAuthRememberService
);
let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

function getOAWithPartitionKey(
  { scheme = "https", topLevelBaseDomain, port = null } = {},
  originAttributes = {}
) {
  if (!topLevelBaseDomain || !scheme) {
    return originAttributes;
  }

  return {
    ...originAttributes,
    partitionKey: `(${scheme},${topLevelBaseDomain}${port ? `,${port}` : ""})`,
  };
}

// These are not actual server and client certs. The ClientAuthRememberService
// does not care which certs we store decisions for, as long as they're valid.
let [serverCert, clientCert] = certDB.getCerts();

function addSecurityInfo({ host, topLevelBaseDomain, originAttributes = {} }) {
  let attrs = getOAWithPartitionKey({ topLevelBaseDomain }, originAttributes);
  cars.rememberDecisionScriptable(host, attrs, serverCert, clientCert);
}

function testSecurityInfo({
  host,
  topLevelBaseDomain,
  originAttributes = {},
  expected = true,
}) {
  let attrs = getOAWithPartitionKey({ topLevelBaseDomain }, originAttributes);

  let messageSuffix = `for ${host}`;
  if (topLevelBaseDomain) {
    messageSuffix += ` partitioned under ${topLevelBaseDomain}`;
  }

  let hasRemembered = cars.hasRememberedDecisionScriptable(
    host,
    attrs,
    serverCert,
    {}
  );

  Assert.equal(
    hasRemembered,
    expected,
    `CAR ${expected ? "is set" : "is not set"} ${messageSuffix}`
  );
}

function addTestEntries() {
  let entries = [
    { host: "example.net" },
    { host: "test.example.net" },
    { host: "example.org" },
    { host: "example.com", topLevelBaseDomain: "example.net" },
    {
      host: "test.example.net",
      topLevelBaseDomain: "example.org",
    },
    {
      host: "foo.example.com",
      originAttributes: {
        privateBrowsingId: 1,
      },
    },
  ];

  info("Add test state");
  entries.forEach(addSecurityInfo);
  info("Ensure we have the correct state initially");
  entries.forEach(testSecurityInfo);
}

add_task(async () => {
  addTestEntries();

  info("Should not be set for unrelated host");
  [undefined, "example.org", "example.net", "example.com"].forEach(
    topLevelBaseDomain =>
      testSecurityInfo({
        host: "mochit.test",
        topLevelBaseDomain,
        expected: false,
      })
  );

  info("Should not be set for unrelated subdomain");
  testSecurityInfo({ host: "foo.example.net", expected: false });

  info("Should not be set for unpartitioned first party");
  testSecurityInfo({
    host: "example.com",
    expected: false,
  });

  info("Should not be set under different first party");
  testSecurityInfo({
    host: "example.com",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  testSecurityInfo({
    host: "test.example.net",
    topLevelBaseDomain: "example.com",
    expected: false,
  });

  info("Should not be set in partitioned context");
  ["example.com", "example.net", "example.org", "mochi.test"].forEach(
    topLevelBaseDomain =>
      testSecurityInfo({
        host: "foo.example.com",
        topLevelBaseDomain,
        expected: false,
      })
  );

  // Cleanup
  cars.clearRememberedDecisions();
});
