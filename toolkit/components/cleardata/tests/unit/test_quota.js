/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for the QuotaCleaner.
 */

"use strict";

// The following tests ensure we properly clear (partitioned/unpartitioned)
// localStorage and indexedDB when using deleteDataFromBaseDomain,
// deleteDataFromHost and deleteDataFromPrincipal.

// Skip localStorage tests when using legacy localStorage. The legacy
// localStorage implementation does not support clearing data by principal. See
// Bug 1688221, Bug 1688665.
const skipLocalStorageTests = Services.prefs.getBoolPref(
  "dom.storage.enable_unsupported_legacy_implementation"
);

/**
 * Create an origin with partitionKey.
 * @param {String} host - Host portion of origin to create.
 * @param {String} [topLevelBaseDomain] - Optional first party base domain to use for partitionKey.
 * @param {Object} [originAttributes] - Optional object of origin attributes to
 * set. If topLevelBaseDomain is passed, the partitionKey will be overwritten.
 * @returns {String} Origin with suffix.
 */
function getOrigin(host, topLevelBaseDomain, originAttributes = {}) {
  return getPrincipal(host, topLevelBaseDomain, originAttributes).origin;
}

function getPrincipal(host, topLevelBaseDomain, originAttributes = {}) {
  originAttributes = getOAWithPartitionKey(
    { topLevelBaseDomain },
    originAttributes
  );
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(`https://${host}`),
    originAttributes
  );
  return principal;
}

function getTestEntryName(host, topLevelBaseDomain) {
  if (!topLevelBaseDomain) {
    return host;
  }
  return `${host}_${topLevelBaseDomain}`;
}

function setTestEntry({
  storageType,
  host,
  topLevelBaseDomain = null,
  originAttributes = {},
}) {
  let origin = getOrigin(host, topLevelBaseDomain, originAttributes);
  if (storageType == "localStorage") {
    SiteDataTestUtils.addToLocalStorage(
      origin,
      getTestEntryName(host, topLevelBaseDomain),
      "bar"
    );
    return;
  }
  SiteDataTestUtils.addToIndexedDB(origin);
}

async function testEntryExists({
  storageType,
  host,
  topLevelBaseDomain = null,
  expected = true,
  originAttributes = {},
}) {
  let exists;
  let origin = getOrigin(host, topLevelBaseDomain, originAttributes);
  if (storageType == "localStorage") {
    exists = SiteDataTestUtils.hasLocalStorage(origin, [
      { key: getTestEntryName(host, topLevelBaseDomain), value: "bar" },
    ]);
  } else {
    exists = await SiteDataTestUtils.hasIndexedDB(origin);
  }

  let message = `${storageType} entry ${
    expected ? "is set" : "is not set"
  } for ${host}`;
  if (topLevelBaseDomain) {
    message += ` partitioned under ${topLevelBaseDomain}`;
  }
  Assert.equal(exists, expected, message);
  return exists;
}

async function setTestEntries(storageType) {
  // First party
  setTestEntry({ storageType, host: "example.net" });
  setTestEntry({ storageType, host: "test.example.net" });
  setTestEntry({ storageType, host: "example.org" });

  // Third-party partitioned.
  setTestEntry({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
  });
  setTestEntry({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
  });
  setTestEntry({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.org",
  });
  setTestEntry({
    storageType,
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });

  // Ensure we have the correct storage test state.
  await testEntryExists({ storageType, host: "example.net" });
  await testEntryExists({ storageType, host: "test.example.net" });
  await testEntryExists({ storageType, host: "example.org" });

  await testEntryExists({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
  });
  await testEntryExists({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
  });
  await testEntryExists({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.org",
  });
  await testEntryExists({
    storageType,
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
  });
}

/**
 * Run the base domain test with either localStorage or indexedDB.
 * @param {('localStorage'|'indexedDB')} storageType
 */
async function runTestBaseDomain(storageType) {
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });
  await setTestEntries(storageType);

  // Clear entries of example.net including partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromBaseDomain(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });

  await testEntryExists({ storageType, host: "example.net", expected: false });
  await testEntryExists({
    storageType,
    host: "test.example.net",
    expected: false,
  });
  await testEntryExists({ storageType, host: "example.org" });

  await testEntryExists({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expected: false,
  });
  await testEntryExists({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
    expected: false,
  });
  await testEntryExists({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  await testEntryExists({
    storageType,
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });

  // Cleanup
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });
}

/**
 * Run the host test with either localStorage or indexedDB.
 * @param {('localStorage'|'indexedDB')} storageType
 */
async function runTestHost(storageType) {
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });
  await setTestEntries(storageType);

  // Clear entries of example.net without partitions.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromHost(
      "example.net",
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });

  await testEntryExists({ storageType, host: "example.net", expected: false });
  // QuotaCleaner#deleteByHost also clears subdomains.
  await testEntryExists({
    storageType,
    host: "test.example.net",
    expected: false,
  });
  await testEntryExists({ storageType, host: "example.org" });

  await testEntryExists({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
    expected: true,
  });
  await testEntryExists({
    storageType,
    host: "example.com",
    topLevelBaseDomain: "example.net",
    originAttributes: { userContextId: 1 },
    expected: true,
  });
  // QuotaCleaner#deleteByHost ignores partitionKey.
  await testEntryExists({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });
  await testEntryExists({
    storageType,
    host: "test.example.net",
    topLevelBaseDomain: "example.org",
    expected: false,
  });

  // Cleanup
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });
}

/**
 * Run the principal test with either localStorage or indexedDB.
 * @param {('localStorage'|'indexedDB')} storageType
 */
async function runTestPrincipal(storageType) {
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });

  // First party
  setTestEntry({ storageType, host: "example.net" });
  setTestEntry({
    storageType,
    host: "example.net",
    originAttributes: { userContextId: 2 },
  });
  setTestEntry({
    storageType,
    host: "example.net",
    originAttributes: { privateBrowsingId: 1 },
  });
  setTestEntry({ storageType, host: "test.example.net" });
  setTestEntry({ storageType, host: "example.org" });

  // Third-party partitioned.
  setTestEntry({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.com",
  });

  // Ensure we have the correct storage test state.
  await testEntryExists({ storageType, host: "example.net" });
  await testEntryExists({
    storageType,
    host: "example.net",
    originAttributes: { userContextId: 2 },
  });
  await testEntryExists({
    storageType,
    host: "example.net",
    originAttributes: { privateBrowsingId: 1 },
  });
  await testEntryExists({ storageType, host: "test.example.net" });
  await testEntryExists({ storageType, host: "example.org" });
  await testEntryExists({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.com",
  });

  // Clear entries from principal with custom OA.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      getPrincipal("example.net", null, { userContextId: 2 }),
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });

  // Test that we only deleted entries for the exact origin.
  await testEntryExists({ storageType, host: "example.net" });
  await testEntryExists({
    expected: false,
    storageType,
    host: "example.net",
    originAttributes: { userContextId: 2 },
  });
  await testEntryExists({
    storageType,
    host: "example.net",
    originAttributes: { privateBrowsingId: 1 },
  });
  await testEntryExists({ storageType, host: "test.example.net" });
  await testEntryExists({ storageType, host: "example.org" });
  await testEntryExists({
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.com",
  });

  // Clear entries of from partitioned principal.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      getPrincipal("example.net", "example.com"),
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });

  // Test that we only deleted entries for the partition.
  await testEntryExists({ storageType, host: "example.net" });
  await testEntryExists({
    expected: false,
    storageType,
    host: "example.net",
    originAttributes: { userContextId: 2 },
  });
  await testEntryExists({
    storageType,
    host: "example.net",
    originAttributes: { privateBrowsingId: 1 },
  });
  await testEntryExists({ storageType, host: "test.example.net" });
  await testEntryExists({ storageType, host: "example.org" });
  await testEntryExists({
    expected: false,
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.com",
  });

  // Clear entries of from principal without suffix.
  await new Promise(aResolve => {
    Services.clearData.deleteDataFromPrincipal(
      getPrincipal("example.net", null),
      false,
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });

  // Test that we only deleted entries for the given principal, and not entries
  // for principals with the same host, but different OriginAttributes or
  // subdomains.
  await testEntryExists({ expected: false, storageType, host: "example.net" });
  await testEntryExists({
    expected: false,
    storageType,
    host: "example.net",
    originAttributes: { userContextId: 2 },
  });
  await testEntryExists({
    storageType,
    host: "example.net",
    originAttributes: { privateBrowsingId: 1 },
  });

  await testEntryExists({ storageType, host: "test.example.net" });
  await testEntryExists({ storageType, host: "example.org" });
  await testEntryExists({
    expected: false,
    storageType,
    host: "example.net",
    topLevelBaseDomain: "example.com",
  });

  // Cleanup
  await new Promise(aResolve => {
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_DOM_QUOTA,
      aResolve
    );
  });
}

// Tests

add_task(function setup() {
  // Allow setting local storage in xpcshell tests.
  Services.prefs.setBoolPref("dom.storage.client_validation", false);
});

/**
 * Tests deleting localStorage entries by host.
 */
add_task(async function test_host_localStorage() {
  await runTestHost("localStorage");
});

/**
 * Tests deleting indexedDB entries by host.
 */
add_task(async function test_host_indexedDB() {
  await runTestHost("indexedDB");
});

/**
 * Tests deleting (partitioned) localStorage entries by base domain.
 */
add_task(async function test_baseDomain_localStorage() {
  await runTestBaseDomain("localStorage");
});

/**
 * Tests deleting (partitioned) indexedDB entries by base domain.
 */
add_task(async function test_baseDomain_indexedDB() {
  await runTestBaseDomain("indexedDB");
});

/**
 * Tests deleting localStorage entries by principal.
 */
add_task(async function test_principal_localStorage() {
  // Bug 1688221, Bug 1688665.
  if (skipLocalStorageTests) {
    info("Skipping test");
    return;
  }
  await runTestPrincipal("localStorage");
});
