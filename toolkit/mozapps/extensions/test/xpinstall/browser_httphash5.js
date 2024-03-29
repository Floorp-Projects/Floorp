// ----------------------------------------------------------------------------
// Test that only the first HTTPS hash is used
// This verifies bug 591070
add_task(async function test_only_first_https_hash_used() {
  // This test currently depends on InstallTrigger.install availability.
  setInstallTriggerPrefs();

  await SpecialPowers.pushPrefEnv({
    set: [[PREF_INSTALL_REQUIREBUILTINCERTS, false]],
  });

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

  var url = "https://example.com/browser/" + RELATIVE_DIR + "hashRedirect.sjs";
  url += `?sha256:${xpiFileHash}|`;
  url += "https://example.com/browser/" + RELATIVE_DIR + "hashRedirect.sjs";
  url += "?sha1:foobar|" + TESTROOT + "amosigned.xpi";

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": {
        URL: url,
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
  await SpecialPowers.popPrefEnv();

  gBrowser.removeCurrentTab();
  Harness.finish();
});
