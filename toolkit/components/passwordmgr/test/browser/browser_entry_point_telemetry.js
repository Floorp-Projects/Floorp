const TEST_ORIGIN = "https://example.com";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons.visibilityToggle", true]],
  });
  Services.telemetry.clearEvents();
});

add_task(async function mainMenu_entryPoint() {
  await SimpleTest.promiseFocus();
  info("mainMenu_entryPoint, got focus");

  let mainMenu = document.getElementById("appMenu-popup");
  let target = document.getElementById("PanelUI-menu-button");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(target),
    "Main menu button should be visible."
  );
  info("mainMenu_entryPoint, Main menu button is visible");
  is(
    mainMenu.state,
    "closed",
    `Menu panel (${mainMenu.id}) is initally closed.`
  );

  info("mainMenu_entryPoint, clicking target and waiting for popup");
  let popupshown = BrowserTestUtils.waitForEvent(mainMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(target, {});
  await popupshown;

  info("mainMenu_entryPoint, main menu popup is shown");
  is(mainMenu.state, "open", `Menu panel (${mainMenu.id}) is open.`);

  let item = document.getElementById("appMenu-logins-button");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(item),
    "Logins and passwords button is visible."
  );

  info("mainMenu_entryPoint, clicking on Logins and passwords button");
  let openingFunc = () => EventUtils.synthesizeMouseAtCenter(item, {});
  let passwordManager = await openPasswordManager(openingFunc);
  info("mainMenu_entryPoint, password manager dialog shown");

  await LoginTestUtils.telemetry.waitForEventCount(1);
  TelemetryTestUtils.assertEvents(
    [["pwmgr", "open_management", "mainmenu"]],
    {
      category: "pwmgr",
      method: "open_management",
    },
    { clear: true, process: "content" }
  );

  info("mainMenu_entryPoint, close dialog and main menu");
  await passwordManager.close();
  mainMenu.hidePopup();
});

add_task(async function pageInfo_entryPoint() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN,
    },
    async function(browser) {
      info("pageInfo_entryPoint, opening pageinfo");
      let pageInfo = BrowserPageInfo(TEST_ORIGIN, "securityTab", {});
      await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");
      info(
        "pageInfo_entryPoint, got pageinfo, wait until password button is visible"
      );
      let passwordsButton = pageInfo.document.getElementById(
        "security-view-password"
      );

      await TestUtils.waitForCondition(
        () => BrowserTestUtils.is_visible(passwordsButton),
        "Show passwords button should be visible."
      );
      info("pageInfo_entryPoint, clicking the show passwords button...");
      await SimpleTest.promiseFocus(pageInfo);
      let openingFunc = () =>
        EventUtils.synthesizeMouseAtCenter(passwordsButton, {}, pageInfo);

      info("pageInfo_entryPoint, waiting for the passwords manager dialog");
      let passwordManager = await openPasswordManager(openingFunc);

      TelemetryTestUtils.assertEvents(
        [["pwmgr", "open_management", "pageinfo"]],
        { category: "pwmgr", method: "open_management" },
        { clear: true, process: "content" }
      );

      info("pageInfo_entryPoint, close dialog and pageInfo");
      await passwordManager.close();
      pageInfo.close();
    }
  );
});
