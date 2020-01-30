const TEST_URL_PATH = "https://example.org" + DIRECTORY_PATH;

add_task(async function setup() {
  let login = LoginTestUtils.testData.formLogin({
    origin: "https://example.org",
    formActionOrigin: "https://example.org",
    username: "username1",
    password: "password1",
  });
  Services.logins.addLogin(login);
  login = LoginTestUtils.testData.formLogin({
    origin: "https://example.org",
    formActionOrigin: "https://example.org",
    username: "username2",
    password: "password2",
  });
  Services.logins.addLogin(login);
});

// Verify that the autocomplete popup opens when the username field in autofocused.
add_task(async function test_autofocus_autocomplete() {
  let popup = document.getElementById("PopupAutoComplete");
  let popupShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

  let formFilled = listenForTestNotification("FormProcessed");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_URL_PATH + "form_autofocus_frame.html"
  );

  await formFilled;
  await popupShown;

  ok(true, "popup opened");

  let promiseHidden = BrowserTestUtils.waitForEvent(popup, "popuphidden");
  popup.firstChild.getItemAtIndex(0).click();
  await promiseHidden;

  ok(true, "popup closed");

  let password = await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return content.document.getElementById("form-basic-password").value;
  });
  is(password, "password1", "password filled in");

  gBrowser.removeTab(tab);
});
