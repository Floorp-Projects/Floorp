Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/modules/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);

add_task(async function testGrantGivesPermission() {
  await setPreferences();

  await openPageAndRunCode(
    TEST_TOP_PAGE,
    getExpectPopupAndClick("reject"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectFailure
  );

  await SpecialPowers.testPermission(
    "storageAccessAPI",
    SpecialPowers.Services.perms.UNKNOWN_ACTION,
    {
      url: "https://tracking.example.org/",
    }
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
