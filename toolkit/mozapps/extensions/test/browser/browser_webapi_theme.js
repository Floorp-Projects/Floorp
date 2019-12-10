"use strict";

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const URL = `${SECURE_TESTROOT}addons/browser_theme.xpi`;

add_task(async function test_theme_install() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.install.requireBuiltInCerts", false],
      ["extensions.allowPrivateBrowsingByDefault", false],
    ],
  });

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    let updates = [];
    function observer(subject, topic, data) {
      updates.push(JSON.stringify(subject.wrappedJSObject));
    }
    Services.obs.addObserver(observer, "lightweight-theme-styling-update");
    registerCleanupFunction(() => {
      Services.obs.removeObserver(observer, "lightweight-theme-styling-update");
    });

    let sawConfirm = false;
    promisePopupNotificationShown("addon-install-confirmation").then(panel => {
      sawConfirm = true;
      panel.button.click();
    });

    let prompt1 = waitAppMenuNotificationShown(
      "addon-installed",
      "theme@tests.mozilla.org",
      false
    );
    let installPromise = SpecialPowers.spawn(browser, [URL], async url => {
      let install = await content.navigator.mozAddonManager.createInstall({
        url,
      });
      return install.install();
    });
    await prompt1;

    ok(sawConfirm, "Confirm notification was displayed before installation");

    // Open a new window and test the app menu panel from there.  This verifies the
    // incognito checkbox as well as finishing install in this case.
    let newWin = await BrowserTestUtils.openNewBrowserWindow();
    await waitAppMenuNotificationShown(
      "addon-installed",
      "theme@tests.mozilla.org",
      true,
      newWin
    );
    await installPromise;
    ok(true, "Theme install completed");

    await BrowserTestUtils.closeWindow(newWin);

    Assert.equal(updates.length, 1, "Got a single theme update");
    let parsed = JSON.parse(updates[0]);
    ok(
      parsed.theme.headerURL.endsWith("/testImage.png"),
      "Theme update has the expected headerURL"
    );
    is(
      parsed.theme.id,
      "theme@tests.mozilla.org",
      "Theme update includes the theme ID"
    );
    is(
      parsed.theme.version,
      "1.0",
      "Theme update includes the theme's version"
    );

    let addon = await AddonManager.getAddonByID(parsed.theme.id);
    await addon.uninstall();
  });
});
