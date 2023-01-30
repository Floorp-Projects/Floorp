Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/modules/test/browser/head.js",
  this
);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/antitracking/test/browser/storage_access_head.js",
  this
);

async function setAutograntPreferences() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", true],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
}

add_task(async function testPopupWithUserInteraction() {
  await setPreferences();
  await setAutograntPreferences();

  // Test that requesting storage access initially does not autogrant.
  // If the autogrant doesn't occur, we click reject on the door hanger
  // and expect the promise returned by requestStorageAccess to reject.
  await openPageAndRunCode(
    TEST_TOP_PAGE,
    getExpectPopupAndClick("reject"),
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectFailure
  );
  // Grant the storageAccessAPI permission to the third-party.
  // This signifies that it has been interacted with and should allow autogrants
  // among other behaviors.
  const uri = Services.io.newURI(TEST_3RD_PARTY_DOMAIN);
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  Services.perms.addFromPrincipal(
    principal,
    "storageAccessAPI",
    Services.perms.ALLOW_ACTION
  );

  // Test that requesting storage access autogrants here. If a popup occurs,
  // expectNoPopup will cause an error in this test.
  await openPageAndRunCode(
    TEST_TOP_PAGE,
    expectNoPopup,
    TEST_3RD_PARTY_PAGE,
    requestStorageAccessAndExpectSuccess
  );

  await cleanUpData();
  await SpecialPowers.flushPrefEnv();
});
