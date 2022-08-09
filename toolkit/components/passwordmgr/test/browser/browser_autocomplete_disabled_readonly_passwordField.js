const TEST_URL_PATH =
  "https://example.org" +
  DIRECTORY_PATH +
  "form_disabled_readonly_passwordField.html";
const FIRST_ITEM = 0;

/**
 * Add two logins to prevent autofilling, but the AutocompletePopup will be displayed
 */
add_setup(async () => {
  let login1 = LoginTestUtils.testData.formLogin({
    origin: "https://example.org",
    formActionOrigin: "https://example.org",
    username: "username1",
    password: "password1",
  });

  let login2 = LoginTestUtils.testData.formLogin({
    origin: "https://example.org",
    formActionOrigin: "https://example.org",
    username: "username2",
    password: "password2",
  });

  Services.logins.addLogin(login1);
  Services.logins.addLogin(login2);
});

add_task(
  async function test_autocomplete_for_usernameField_with_disabled_passwordField() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function(browser) {
        let popup = document.getElementById("PopupAutoComplete");

        ok(popup, "Got Popup");

        await openACPopup(
          popup,
          browser,
          "#login_form_disabled_password input[name=username]"
        );

        info("Popup opened");

        let promiseHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
        popup.firstChild.getItemAtIndex(FIRST_ITEM).click();
        await promiseHidden;

        info("Popup closed");

        let [username, password] = await SpecialPowers.spawn(
          browser,
          [],
          async () => {
            let doc = content.document;
            let contentUsername = doc.querySelector(
              "#login_form_disabled_password input[name=username]"
            ).value;
            let contentPassword = doc.querySelector(
              "#login_form_disabled_password input[name=password]"
            ).value;
            return [contentUsername, contentPassword];
          }
        );
        is(
          username,
          "username1",
          "Username was autocompleted with correct value."
        );
        is(
          password,
          "",
          "Password was not autocompleted, because field is disabled."
        );
      }
    );
  }
);

add_task(
  async function test_autocomplete_for_usernameField_with_readonly_passwordField() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: TEST_URL_PATH,
      },
      async function(browser) {
        let popup = document.getElementById("PopupAutoComplete");

        ok(popup, "Got Popup");

        await openACPopup(
          popup,
          browser,
          "#login_form_readonly_password input[name=username]"
        );

        info("Popup opened");

        let promiseHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
        popup.firstChild.getItemAtIndex(FIRST_ITEM).click();
        await promiseHidden;

        info("Popup closed");

        let [username, password] = await SpecialPowers.spawn(
          browser,
          [],
          async () => {
            let doc = content.document;
            let contentUsername = doc.querySelector(
              "#login_form_readonly_password input[name=username]"
            ).value;
            info(contentUsername);
            let contentPassword = doc.querySelector(
              "#login_form_readonly_password input[name=password]"
            ).value;
            info(contentPassword);
            return [contentUsername, contentPassword];
          }
        );
        is(
          username,
          "username1",
          "Username was autocompleted with correct value."
        );
        is(
          password,
          "",
          "Password was not autocompleted, because field is readonly."
        );
      }
    );
  }
);
