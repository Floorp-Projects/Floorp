Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/modules/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);
/* import-globals-from storageAccessAPIHelpers.js */

async function testEmbeddedPageBehavior() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  var p = document.requestStorageAccess();
  try {
    await p;
    ok(true, "gain storage access.");
  } catch {
    ok(false, "denied storage access.");
  }
  SpecialPowers.wrap(document).clearUserGestureActivation();
  waitUntilPermission(
    "https://tracking.example.org/",
    "storageAccessAPI",
    SpecialPowers.Services.perms.ALLOW_ACTION
  );
}

add_task(async function testGrantGivesPermission() {
  await setPreferences();

  await openPageAndRunCode(
    TEST_TOP_PAGE,
    getExpectPopupAndClick("accept"),
    TEST_3RD_PARTY_PAGE,
    testEmbeddedPageBehavior
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
