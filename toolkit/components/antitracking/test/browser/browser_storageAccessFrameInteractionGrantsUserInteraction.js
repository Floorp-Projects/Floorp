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
  // Get the storage access permission
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  let p = document.requestStorageAccess();
  try {
    await p;
    ok(true, "gain storage access.");
  } catch {
    ok(false, "denied storage access.");
  }
  SpecialPowers.wrap(document).clearUserGestureActivation();

  // Wait until we have the permission before we remove it.
  waitUntilPermission(
    "https://tracking.example.org/",
    "storageAccessAPI",
    SpecialPowers.Services.perms.ALLOW_ACTION
  );

  // Remove the storageAccessAPI permission
  SpecialPowers.removePermission(
    "storageAccessAPI",
    "https://tracking.example.org/"
  );

  // Wait until the permission is removed
  waitUntilPermission(
    "https://tracking.example.org/",
    "storageAccessAPI",
    SpecialPowers.Services.perms.UNKNOWN_ACTION
  );

  // Interact with the third-party iframe and wait for the permission to appear
  SpecialPowers.wrap(document).userInteractionForTesting();
  waitUntilPermission(
    "https://tracking.example.org/",
    "storageAccessAPI",
    SpecialPowers.Services.perms.ALLOW_ACTION
  );
}

// This test verifies that interacting with a third-party iframe with
// storage access gives the storageAccessAPI permission, as if it were a first
// party. This is done by loading a page, then within an iframe in that page
// requesting storage access, ensuring there is no storageAccessAPI permission,
// then interacting with the page and waiting for that storageAccessAPI
// permission to reappear.
add_task(async function testInteractionGivesPermission() {
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
