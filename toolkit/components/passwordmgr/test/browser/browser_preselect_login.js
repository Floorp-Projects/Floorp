const TEST_ORIGIN = "https://example.org";
const ABOUT_LOGINS_ORIGIN = "about:logins";
const TEST_URL_PATH = `${TEST_ORIGIN}${DIRECTORY_PATH}form_basic_login.html`;

const LOGINS_DATA = [
  {
    origin: "https://aurl.com",
    username: "user1",
    password: "pass1",
    guid: Services.uuid.generateUUID().toString(),
  },
  {
    origin: TEST_ORIGIN,
    username: "user2",
    password: "pass2",
    guid: Services.uuid.generateUUID().toString(),
  },
  {
    origin: TEST_ORIGIN,
    username: "user3",
    password: "pass3",
    guid: Services.uuid.generateUUID().toString(),
  },
];

const waitForAppMenu = async () => {
  const appMenu = document.getElementById("appMenu-popup");
  const appMenuButton = document.getElementById("PanelUI-menu-button");
  await TestUtils.waitForCondition(
    () => BrowserTestUtils.is_visible(appMenuButton),
    "App menu button should be visible."
  );

  let popupshown = BrowserTestUtils.waitForEvent(appMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(appMenuButton, {});
  await popupshown;
  Assert.equal(
    appMenu.state,
    "open",
    `Menu panel (${appMenu.id}) should be visible`
  );
};

const isExpectedLoginItemSelected = async ({ expectedGuid }) => {
  const loginList = content.document.querySelector("login-list").shadowRoot;

  await ContentTaskUtils.waitForCondition(
    () =>
      loginList.querySelector("login-list-item[aria-selected='true']")?.dataset
        ?.guid === expectedGuid,
    "Wait for login item to be selected"
  );

  Assert.equal(
    loginList.querySelector("login-list-item[aria-selected='true']")?.dataset
      ?.guid,
    expectedGuid,
    "Expected login is preselected"
  );
};

add_setup(async () => {
  await Services.logins.addLogins(
    LOGINS_DATA.map(login => LoginTestUtils.testData.formLogin(login))
  );
});

add_task(async function test_about_logins_defaults_to_first_item() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins#random-guid",
    },
    async function (gBrowser) {
      await SpecialPowers.spawn(
        gBrowser,
        [{ expectedGuid: LOGINS_DATA[0].guid }],
        isExpectedLoginItemSelected
      );

      Assert.ok(
        true,
        "First item of the list is selected when random hash is supplied."
      );
    }
  );
});

add_task(
  async function test_gear_icon_opens_about_logins_with_preselected_login() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function (browser) {
        const popup = document.getElementById("PopupAutoComplete");

        await openACPopup(popup, browser, "#form-basic-username");

        const secondLoginItem = popup.firstChild.getItemAtIndex(1);
        const secondLoginItemSecondaryAction = secondLoginItem.querySelector(
          ".ac-secondary-action"
        );

        Assert.ok(
          !secondLoginItemSecondaryAction.checkVisibility({
            checkVisibilityCSS: true,
          }),
          "Secondary action should not be visible initially"
        );

        await EventUtils.synthesizeKey("KEY_ArrowDown");
        await EventUtils.synthesizeKey("KEY_ArrowDown");

        await BrowserTestUtils.waitForCondition(
          () => secondLoginItem.attributes.selected,
          "Wait for second login to become active"
        );

        Assert.ok(
          secondLoginItemSecondaryAction.checkVisibility({
            checkVisibilityCSS: true,
          }),
          "Secondary action should be visible when item is active"
        );

        const aboutLoginsTabPromise = BrowserTestUtils.waitForNewTab(
          gBrowser,
          url => url.includes(ABOUT_LOGINS_ORIGIN),
          true
        );

        EventUtils.synthesizeMouseAtCenter(secondLoginItemSecondaryAction, {});
        const aboutLoginsTab = await aboutLoginsTabPromise;

        await SpecialPowers.spawn(
          aboutLoginsTab.linkedBrowser,
          [{ expectedGuid: LOGINS_DATA[2].guid }],
          isExpectedLoginItemSelected
        );

        await closePopup(popup);
        gBrowser.removeTab(aboutLoginsTab);
      }
    );
  }
);

add_task(
  async function test_password_menu_opens_about_logins_with_preselected_login() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function (browser) {
        await waitForAppMenu();

        const appMenuPasswordsButton = document.getElementById(
          "appMenu-passwords-button"
        );

        const aboutLoginsTabPromise = BrowserTestUtils.waitForNewTab(
          gBrowser,
          url => url.includes(ABOUT_LOGINS_ORIGIN),
          true
        );

        EventUtils.synthesizeMouseAtCenter(appMenuPasswordsButton, {});

        const aboutLoginsTab = await aboutLoginsTabPromise;

        await SpecialPowers.spawn(
          aboutLoginsTab.linkedBrowser,
          [{ expectedGuid: LOGINS_DATA[1].guid }],
          isExpectedLoginItemSelected
        );

        gBrowser.removeTab(aboutLoginsTab);
      }
    );
  }
);

add_task(async function test_new_login_url_has_correct_hash() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:logins",
    },
    async function (gBrowser) {
      await SpecialPowers.spawn(gBrowser, [], async () => {
        const loginList =
          content.document.querySelector("login-list").shadowRoot;
        const createLoginButton = loginList.querySelector(
          "login-command-button.create-login-button"
        );

        createLoginButton.click();

        await ContentTaskUtils.waitForCondition(
          () =>
            ContentTaskUtils.is_visible(
              loginList.querySelector("#new-login-list-item")
            ),
          "Wait for new login-list-item to become visible"
        );

        Assert.equal(
          content.location.hash,
          "",
          "Location hash should be empty"
        );
      });
    }
  );
});

add_task(async function test_no_logins_empty_url_hash() {
  Services.logins.removeAllUserFacingLogins();
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URL_PATH,
    },
    async function () {
      await waitForAppMenu();

      const appMenuPasswordsButton = document.getElementById(
        "appMenu-passwords-button"
      );

      const aboutLoginsTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        url => new URL(url).hash === "",
        true
      );

      EventUtils.synthesizeMouseAtCenter(appMenuPasswordsButton, {});

      const aboutLoginsTab = await aboutLoginsTabPromise;

      gBrowser.removeTab(aboutLoginsTab);
    }
  );
});
