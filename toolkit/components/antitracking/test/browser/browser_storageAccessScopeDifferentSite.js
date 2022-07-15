/* import-globals-from ../../../../../browser/modules/test/browser/head.js */
/* import-globals-from head.js */
/* import-globals-from storage_access_head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/modules/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);

add_task(async function testInitialBlock() {
  await setPreferences();

  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    getExpectPopupAndClick("reject"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectFailure
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function testDifferentSitePermission() {
  await setPreferences(/*alwaysPartitionStorage*/ false);

  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    getExpectPopupAndClick("accept"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectSuccess
  );

  await openPageAndRunCode(
    TEST_TOP_PAGE,
    getExpectPopupAndClick("reject"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectFailure
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});

add_task(async function testDifferentSitePermissionAPS() {
  await setPreferences(/*alwaysPartitionStorage*/ true);

  await openPageAndRunCode(
    TEST_TOP_PAGE_7,
    getExpectPopupAndClick("accept"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectSuccess
  );

  await openPageAndRunCode(
    TEST_TOP_PAGE,
    getExpectPopupAndClick("reject"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectFailure
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
