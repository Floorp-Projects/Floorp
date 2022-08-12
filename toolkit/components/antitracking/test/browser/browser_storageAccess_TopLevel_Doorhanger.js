const CHROME_BASE =
  "chrome://mochitests/content/browser/browser/modules/test/browser/";
Services.scriptloader.loadSubScript(CHROME_BASE + "head.js", this);
/* import-globals-from ../../../../../browser/modules/test/browser/head.js */

async function cleanUp() {
  Services.perms.removeAll();
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
}

add_task(async function testDoorhangerRequestStorageAccessUnderSite() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      ["dom.storage_access.prompt.testing", false],
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_4TH_PARTY_PAGE,
  });
  let browser = tab.linkedBrowser;
  let permChanged = TestUtils.topicObserved("perm-changed", (subject, data) => {
    let result =
      subject
        .QueryInterface(Ci.nsIPermission)
        .type.startsWith("AllowStorageAccessRequest^") &&
      subject.principal.origin == new URL(TEST_TOP_PAGE).origin &&
      data == "added";
    return result;
  }).then(() => {
    ok(true, "Permission changed to add intermediate permission");
  });
  let shownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  ).then(_ => {
    ok(true, "Must display doorhanger from RequestStorageAccessUnderSite");
    return clickMainAction();
  });
  let sp = SpecialPowers.spawn(browser, [TEST_DOMAIN], async tp => {
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    let p = content.document.requestStorageAccessUnderSite(tp);
    try {
      await p;
      ok(true, "Must resolve.");
    } catch {
      ok(false, "Must not reject.");
    }
  });

  await Promise.all([sp, shownPromise, permChanged]);
  await cleanUp();
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function testNoDoorhangerCompleteStorageAccessRequestFromSite() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.forward_declared.enabled", true],
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["dom.storage_access.prompt.testing", false],
      ["dom.storage_access.auto_grants", false],
      ["dom.storage_access.max_concurrent_auto_grants", 1],
    ],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: TEST_TOP_PAGE,
  });
  let browser = tab.linkedBrowser;
  let popupShown = false;
  // This promise is used to the absence of a doorhanger showing up.
  BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown")
    .then(_ => {
      // This will be called if a doorhanger is shown.
      ok(
        false,
        "Must not display doorhanger from CompleteStorageAccessRequestFromSite"
      );
      popupShown = true;
    })
    .catch(_ => {
      // This will be called when the test ends if a doorhanger is not shown
      ok(true, "It is expected for this popup to not show up.");
    });
  await SpecialPowers.spawn(browser, [TEST_4TH_PARTY_DOMAIN], async tp => {
    await SpecialPowers.pushPermissions([
      {
        type: "AllowStorageAccessRequest^http://example.com",
        allow: 1,
        context: content.document,
      },
    ]);
    SpecialPowers.wrap(content.document).notifyUserGestureActivation();
    let p = content.document.completeStorageAccessRequestFromSite(tp);
    try {
      await p;
      ok(true, "Must resolve.");
    } catch {
      ok(false, "Must not reject.");
    }
  });
  ok(!popupShown, "Must not have shown a popup during this test.");
  await cleanUp();
  await BrowserTestUtils.removeTab(tab);
});
