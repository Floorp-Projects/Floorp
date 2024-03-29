// ----------------------------------------------------------------------------
// Test whether an install succeeds when a valid hash is included
// This verifies bug 302284
add_task(async function test_install_with_hash() {
  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  const xpiFilePath = getTestFilePath("./amosigned.xpi");
  const xpiFileHash = await IOUtils.computeHexDigest(xpiFilePath, "sha256");

  const deferredInstallCompleted = Promise.withResolvers();

  Harness.installEndedCallback = (install, addon) => addon.uninstall();
  Harness.installsCompletedCallback = deferredInstallCompleted.resolve;
  Harness.setup();

  PermissionTestUtils.add(
    "http://example.com/",
    "install",
    Services.perms.ALLOW_ACTION
  );

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: TESTROOT + "amosigned.xpi",
        Hash: `sha256:${xpiFileHash}`,
        toString() {
          return this.URL;
        },
      },
    })
  );
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.startLoadingURIString(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );

  info("Wait for the install to be completed");
  const count = await deferredInstallCompleted.promise;
  is(count, 1, "1 Add-on should have been successfully installed");

  PermissionTestUtils.remove("http://example.com", "install");

  gBrowser.removeCurrentTab();
  Harness.finish();
});
