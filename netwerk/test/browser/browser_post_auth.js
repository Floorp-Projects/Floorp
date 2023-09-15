"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

const FOLDER = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    `${FOLDER}post.html`
  );
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    `${FOLDER}post.html`
  );
  await browserLoadedPromise;

  let finalLoadPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    `${FOLDER}auth_post.sjs`
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let file = new content.File(
      [new content.Blob(["1234".repeat(1024 * 500)], { type: "text/plain" })],
      "test-name"
    );
    content.document.getElementById("input_file").mozSetFileArray([file]);
    content.document.getElementById("form").submit();
  });

  let promptPromise = PromptTestUtils.handleNextPrompt(
    tab.linkedBrowser,
    {
      modalType: Services.prefs.getIntPref("prompts.modalType.httpAuth"),
      promptType: "promptUserAndPass",
    },
    { buttonNumClick: 0, loginInput: "user", passwordInput: "pass" }
  );

  await promptPromise;

  await finalLoadPromise;

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    Assert.ok(content.location.href.includes("auth_post.sjs"));
    Assert.ok(content.document.body.innerHTML.includes("1234"));
  });

  BrowserTestUtils.removeTab(tab);

  // Clean up any active logins we added during the test.
  Services.obs.notifyObservers(null, "net:clear-active-logins");
});
