/*
 * Test that browser chrome UI interactions don't trigger a capture doorhanger.
 */

"use strict";

async function fillTestPage(
  aBrowser,
  username = "my_username",
  password = "my_password"
) {
  let notif = getCaptureDoorhanger("any", undefined, aBrowser);
  ok(!notif, "No doorhangers should be present before filling the form");

  await changeContentFormValues(aBrowser, {
    "#form-basic-username": username,
    "#form-basic-password": password,
  });
  if (LoginHelper.passwordEditCaptureEnabled) {
    // Filling the password will generate a dismissed doorhanger.
    // Check and remove that before running the rest of the task
    notif = await waitForDoorhanger(aBrowser, "any");
    ok(notif.dismissed, "Only a dismissed doorhanger should be present");
    await cleanupDoorhanger(notif);
  }
}

function withTestPage(aTaskFn) {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "formless_basic.html",
    },
    async function(aBrowser) {
      info("tab opened");
      await fillTestPage(aBrowser);
      await aTaskFn(aBrowser);

      // Give a chance for the doorhanger to appear
      await new Promise(resolve => SimpleTest.executeSoon(resolve));
      let notif = getCaptureDoorhanger("any");
      ok(!notif, "No doorhanger should be present");
      await cleanupDoorhanger(notif);
    }
  );
}

add_task(async function setup() {
  await SimpleTest.promiseFocus(window);
});

add_task(async function test_urlbar_new_URL() {
  await withTestPage(async function(aBrowser) {
    gURLBar.value = "";
    let focusPromise = BrowserTestUtils.waitForEvent(gURLBar, "focus");
    gURLBar.focus();
    await focusPromise;
    info("focused");
    EventUtils.sendString("http://mochi.test:8888/");
    EventUtils.synthesizeKey("KEY_Enter");
    await BrowserTestUtils.browserLoaded(
      aBrowser,
      false,
      "http://mochi.test:8888/"
    );
  });
});

add_task(async function test_urlbar_fragment_enter() {
  await withTestPage(function(aBrowser) {
    gURLBar.focus();
    gURLBar.select();
    EventUtils.synthesizeKey("KEY_ArrowRight");
    EventUtils.sendString("#fragment");
    EventUtils.synthesizeKey("KEY_Enter");
  });
});

add_task(async function test_backButton_forwardButton() {
  await withTestPage(async function(aBrowser) {
    info("Loading formless_basic.html?second");
    // Load a new page in the tab so we can test going back
    BrowserTestUtils.loadURI(
      aBrowser,
      "https://example.com" + DIRECTORY_PATH + "formless_basic.html?second"
    );
    await BrowserTestUtils.browserLoaded(
      aBrowser,
      false,
      "https://example.com" + DIRECTORY_PATH + "formless_basic.html?second"
    );
    info("Loaded formless_basic.html?second");
    await fillTestPage(aBrowser, "my_username", "password_2");

    info("formless_basic.html?second form is filled, clicking back");
    let backPromise = BrowserTestUtils.browserStopped(aBrowser);
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("back-button"),
      {}
    );
    await backPromise;

    // Give a chance for the doorhanger to appear
    await new Promise(resolve => SimpleTest.executeSoon(resolve));
    ok(!getCaptureDoorhanger("any"), "No doorhanger should be present");

    // Now go forward again after filling
    await fillTestPage(aBrowser, "my_username", "password_3");

    let forwardButton = document.getElementById("forward-button");
    await BrowserTestUtils.waitForCondition(() => {
      return !forwardButton.disabled;
    });
    let forwardPromise = BrowserTestUtils.browserStopped(aBrowser);
    info("click the forward button");
    EventUtils.synthesizeMouseAtCenter(forwardButton, {});
    await forwardPromise;
    info("done");
  });
});

add_task(async function test_reloadButton() {
  await withTestPage(async function(aBrowser) {
    let reloadButton = document.getElementById("reload-button");
    let loadPromise = BrowserTestUtils.browserLoaded(
      aBrowser,
      false,
      "https://example.com" + DIRECTORY_PATH + "formless_basic.html"
    );

    await BrowserTestUtils.waitForCondition(() => {
      return !reloadButton.disabled;
    });
    EventUtils.synthesizeMouseAtCenter(reloadButton, {});
    await loadPromise;
  });
});

add_task(async function test_back_keyboard_shortcut() {
  if (Services.prefs.getIntPref("browser.backspace_action") != 0) {
    ok(true, "Skipped testing backspace to go back since it's disabled");
    return;
  }
  await withTestPage(async function(aBrowser) {
    // Load a new page in the tab so we can test going back
    BrowserTestUtils.loadURI(
      aBrowser,
      "https://example.com" + DIRECTORY_PATH + "formless_basic.html?second"
    );
    await BrowserTestUtils.browserLoaded(
      aBrowser,
      false,
      "https://example.com" + DIRECTORY_PATH + "formless_basic.html?second"
    );
    await fillTestPage(aBrowser);

    let backPromise = BrowserTestUtils.browserStopped(aBrowser);
    EventUtils.synthesizeKey("KEY_Backspace");
    await backPromise;
  });
});
