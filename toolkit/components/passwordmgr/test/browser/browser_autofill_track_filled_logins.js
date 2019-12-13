"use strict";

const TEST_HOSTNAME = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";
const BASIC_FORM_NO_USERNAME_PAGE_PATH =
  DIRECTORY_PATH + "form_basic_no_username.html";

add_task(async function test() {
  let nsLoginInfo = new Components.Constructor(
    "@mozilla.org/login-manager/loginInfo;1",
    Ci.nsILoginInfo,
    "init"
  );
  for (let usernameRequested of [true, false]) {
    info(
      "Testing page with " +
        (usernameRequested ? "" : "no ") +
        "username requested"
    );
    let url = usernameRequested
      ? TEST_HOSTNAME + BASIC_FORM_PAGE_PATH
      : TEST_HOSTNAME + BASIC_FORM_NO_USERNAME_PAGE_PATH;

    // The login here must be a different domain than the page for this testcase.
    let login = new nsLoginInfo(
      "https://example.org",
      "https://example.org",
      null,
      "bob",
      "mypassword",
      "form-basic-username",
      "form-basic-password"
    );
    login = Services.logins.addLogin(login);
    is(login.timesUsed, 1, "The timesUsed should be 1 after creation");

    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url,
    });

    // Convert the login object to a plain JS object for passing across process boundaries.
    login = LoginHelper.loginToVanillaObject(login);
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [{ login, usernameRequested }],
      async ({ login: addedLogin, usernameRequested: aUsernameRequested }) => {
        const { LoginFormFactory } = ChromeUtils.import(
          "resource://gre/modules/LoginFormFactory.jsm"
        );
        const { LoginManagerChild } = ChromeUtils.import(
          "resource://gre/modules/LoginManagerChild.jsm"
        );
        const { LoginHelper } = ChromeUtils.import(
          "resource://gre/modules/LoginHelper.jsm"
        );

        let password = content.document.querySelector("#form-basic-password");
        let formLike = LoginFormFactory.createFromField(password);
        info("Calling _fillForm with FormLike");
        addedLogin = LoginHelper.vanillaObjectToLogin(addedLogin);
        LoginManagerChild.forWindow(content)._fillForm(
          formLike,
          [addedLogin],
          null,
          {
            autofillForm: true,
            clobberUsername: true,
            clobberPassword: true,
            userTriggered: true,
          }
        );

        if (aUsernameRequested) {
          let username = content.document.querySelector("#form-basic-username");
          is(username.value, "bob", "Filled username should match");
        }
        is(password.value, "mypassword", "Filled password should match");
      }
    );

    let processedPromise = listenForTestNotification("FormSubmit");
    SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      content.document.getElementById("form-basic").submit();
    });
    await processedPromise;

    let logins = Services.logins.getAllLogins();

    is(logins.length, 1, "There should only be one login saved");
    is(
      logins[0].guid,
      login.guid,
      "The saved login should match the one added and used above"
    );
    checkOnlyLoginWasUsedTwice({ justChanged: false });

    BrowserTestUtils.removeTab(tab);

    // Reset all passwords before next iteration.
    Services.logins.removeAllLogins();
  }
});
