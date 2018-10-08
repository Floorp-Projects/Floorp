
"use strict";

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const URL = `${SECURE_TESTROOT}addons/browser_theme.xpi`;

add_task(async function test_theme_install() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webapi.testing", true],
          ["extensions.install.requireBuiltInCerts", false]],
  });

  await BrowserTestUtils.withNewTab(TESTPAGE, async (browser) => {
    let updates = [];
    function observer(subject, topic, data) {
      updates.push(data);
    }
    Services.obs.addObserver(observer, "lightweight-theme-styling-update");
    registerCleanupFunction(() => {
      Services.obs.removeObserver(observer, "lightweight-theme-styling-update");
    });

    let promptPromise = acceptAppMenuNotificationWhenShown("addon-installed");

    let installPromise = ContentTask.spawn(browser, URL, async (url) => {
      let install = await content.navigator.mozAddonManager.createInstall({url});
      return install.install();
    });

    await promptPromise;
    await installPromise;
    ok(true, "Theme install completed");

    Assert.equal(updates.length, 1, "Got a single theme update");
    let parsed = JSON.parse(updates[0]);
    ok(parsed.theme.headerURL.endsWith("/testImage.png"),
       "Theme update has the expected headerURL");
  });
});
