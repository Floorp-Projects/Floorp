/*
 * Test that browser chrome UI interactions don't trigger a capture doorhanger.
 */

"use strict";

async function fillTestPage(aBrowser) {
  await ContentTask.spawn(aBrowser, null, async function() {
    content.document.getElementById("form-basic-username").value = "my_username";
    content.document.getElementById("form-basic-password").value = "my_password";
  });
  info("fields filled");
}

function withTestPage(aTaskFn) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: "https://example.com" + DIRECTORY_PATH + "formless_basic.html",
  }, async function(aBrowser) {
    info("tab opened");
    await fillTestPage(aBrowser);
    await aTaskFn(aBrowser);

    // Give a chance for the doorhanger to appear
    await new Promise(resolve => SimpleTest.executeSoon(resolve));
    ok(!getCaptureDoorhanger("any"), "No doorhanger should be present");
  });
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
    EventUtils.synthesizeKey("VK_RETURN", {});
    await BrowserTestUtils.browserLoaded(aBrowser, false, "http://mochi.test:8888/");
  });
});

add_task(async function test_urlbar_fragment_enter() {
  await withTestPage(function(aBrowser) {
    gURLBar.focus();
    EventUtils.synthesizeKey("VK_RIGHT", {});
    EventUtils.sendString("#fragment");
    EventUtils.synthesizeKey("VK_RETURN", {});
  });
});

add_task(async function test_backButton_forwardButton() {
  await withTestPage(async function(aBrowser) {
    // Load a new page in the tab so we can test going back
    aBrowser.loadURI("https://example.com" + DIRECTORY_PATH + "formless_basic.html?second");
    await BrowserTestUtils.browserLoaded(aBrowser, false,
                                         "https://example.com" + DIRECTORY_PATH +
                                         "formless_basic.html?second");
    await fillTestPage(aBrowser);

    let forwardButton = document.getElementById("forward-button");

    let forwardTransitionPromise;
    if (forwardButton.nextSibling == gURLBar) {
      // We need to wait for the forward button transition to complete before we
      // can click it, so we hook up a listener to wait for it to be ready.
      forwardTransitionPromise = BrowserTestUtils.waitForEvent(forwardButton, "transitionend");
    }

    let backPromise = BrowserTestUtils.browserStopped(aBrowser);
    EventUtils.synthesizeMouseAtCenter(document.getElementById("back-button"), {});
    await backPromise;

    // Give a chance for the doorhanger to appear
    await new Promise(resolve => SimpleTest.executeSoon(resolve));
    ok(!getCaptureDoorhanger("any"), "No doorhanger should be present");

    // Now go forward again after filling
    await fillTestPage(aBrowser);

    if (forwardTransitionPromise) {
      await forwardTransitionPromise;
      info("transition done");
    }

    await BrowserTestUtils.waitForCondition(() => {
      return forwardButton.disabled == false;
    });
    let forwardPromise = BrowserTestUtils.browserStopped(aBrowser);
    info("click the forward button");
    EventUtils.synthesizeMouseAtCenter(forwardButton, {});
    await forwardPromise;
  });
});


add_task(async function test_reloadButton() {
  await withTestPage(async function(aBrowser) {
    let reloadButton = document.getElementById("reload-button");
    let loadPromise = BrowserTestUtils.browserLoaded(aBrowser, false,
                                                     "https://example.com" + DIRECTORY_PATH +
                                                     "formless_basic.html");

    await BrowserTestUtils.waitForCondition(() =>
      !reloadButton.disabled && !reloadButton.hasAttribute("temporarily-disabled"));
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
    aBrowser.loadURI("https://example.com" + DIRECTORY_PATH + "formless_basic.html?second");
    await BrowserTestUtils.browserLoaded(aBrowser, false,
                                         "https://example.com" + DIRECTORY_PATH +
                                         "formless_basic.html?second");
    await fillTestPage(aBrowser);

    let backPromise = BrowserTestUtils.browserStopped(aBrowser);
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    await backPromise;
  });
});
