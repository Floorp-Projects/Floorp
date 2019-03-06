"use strict";

const TEST_HOSTNAME = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";

function getSubmitMessage() {
  info("getSubmitMessage");
  return new Promise((resolve, reject) => {
    Services.mm.addMessageListener("RemoteLogins:onFormSubmit", function onFormSubmit() {
      Services.mm.removeMessageListener("RemoteLogins:onFormSubmit", onFormSubmit);
      resolve();
    });
  });
}

add_task(async function test() {
  let nsLoginInfo = new Components.Constructor("@mozilla.org/login-manager/loginInfo;1",
                                               Ci.nsILoginInfo, "init");
  let login = new nsLoginInfo("https://example.com", "https://example.com", null,
                              "bob", "mypassword", "form-basic-username", "form-basic-password");
  login = Services.logins.addLogin(login);
  is(login.timesUsed, 1, "The timesUsed should be 1 after creation");

  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url,
  });

  ContentTask.spawn(tab.linkedBrowser, login, (addedLogin) => {
    const { LoginManagerContent, LoginFormFactory } = ChromeUtils.import("resource://gre/modules/LoginManagerContent.jsm");

    let inputElement = content.document.querySelector("input");
    let formLike = LoginFormFactory.createFromField(inputElement);
    info("Calling _fillForm with FormLike");
    LoginManagerContent._fillForm(formLike, [addedLogin], null, {
      inputElement,
      autofillForm: true,
      clobberUsername: true,
      clobberPassword: true,
      userTriggered: true,
    });
  });

  let processedPromise = getSubmitMessage();
  ContentTask.spawn(tab.linkedBrowser, null, () => {
    content.document.getElementById("form-basic").submit();
  });
  await processedPromise;

  let logins = Services.logins.getAllLogins();

  is(logins.length, 1, "There should only be one login saved");
  is(logins[0].guid, login.guid, "The saved login should match the one added and used above");
  checkOnlyLoginWasUsedTwice({ justChanged: false });

  BrowserTestUtils.removeTab(tab);
});
