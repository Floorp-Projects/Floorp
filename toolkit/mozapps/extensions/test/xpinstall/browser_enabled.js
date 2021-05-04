"use strict";

// Test whether an InstallTrigger.enabled is working
add_task(async function test_enabled() {
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TESTROOT + "enabled.html"
  );

  let text = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    undefined,
    () => content.document.getElementById("enabled").textContent
  );

  is(text, "true", "installTrigger should have been enabled");
  gBrowser.removeCurrentTab();
});

// Test whether an InstallTrigger.enabled is working
add_task(async function test_disabled() {
  Services.prefs.setBoolPref("xpinstall.enabled", false);
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TESTROOT + "enabled.html"
  );

  let text = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    undefined,
    () => content.document.getElementById("enabled").textContent
  );

  is(text, "false", "installTrigger should have not been enabled");
  Services.prefs.clearUserPref("xpinstall.enabled");
  gBrowser.removeCurrentTab();
});

// Test whether an InstallTrigger.install call fails when xpinstall is disabled
add_task(async function test_disabled2() {
  let installDisabledCalled = false;

  Harness.installDisabledCallback = installInfo => {
    installDisabledCalled = true;
    ok(true, "Saw installation disabled");
  };

  Harness.installBlockedCallback = installInfo => {
    ok(false, "Should never see the blocked install notification");
    return false;
  };

  Harness.installConfirmCallback = panel => {
    ok(false, "Should never see an install confirmation dialog");
    return false;
  };

  Harness.setup();
  Services.prefs.setBoolPref("xpinstall.enabled", false);

  var triggers = encodeURIComponent(
    JSON.stringify({
      "Unsigned XPI": TESTROOT + "amosigned.xpi",
    })
  );

  BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TESTROOT + "installtrigger.html?" + triggers
  );

  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "InstallTriggered",
    true,
    undefined,
    true
  );

  let text = await ContentTask.spawn(
    gBrowser.selectedBrowser,
    undefined,
    () => content.document.getElementById("return").textContent
  );

  is(text, "false", "installTrigger should have not been enabled");
  ok(installDisabledCalled, "installDisabled callback was called");

  Services.prefs.clearUserPref("xpinstall.enabled");
  gBrowser.removeCurrentTab();
  Harness.finish();
});
