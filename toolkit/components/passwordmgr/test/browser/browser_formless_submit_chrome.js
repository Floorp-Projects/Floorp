/*
 * Test that browser chrome UI interactions don't trigger a capture doorhanger.
 */

"use strict";

function* fillTestPage(aBrowser) {
  yield ContentTask.spawn(aBrowser, null, function*() {
    content.document.getElementById("form-basic-username").value = "my_username";
    content.document.getElementById("form-basic-password").value = "my_password";
  });
  info("fields filled");
}

function* withTestPage(aTaskFn) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: "https://example.com" + DIRECTORY_PATH + "formless_basic.html",
  }, function*(aBrowser) {
    info("tab opened");
    yield fillTestPage(aBrowser);
    yield* aTaskFn(aBrowser);

    // Give a chance for the doorhanger to appear
    yield new Promise(resolve => SimpleTest.executeSoon(resolve));
    ok(!getCaptureDoorhanger("any"), "No doorhanger should be present");
  });
}

add_task(function* setup() {
  yield SimpleTest.promiseFocus(window);
});

add_task(function* test_urlbar_new_URL() {
  yield withTestPage(function*(aBrowser) {
    gURLBar.value = "";
    let focusPromise = BrowserTestUtils.waitForEvent(gURLBar, "focus");
    gURLBar.focus();
    yield focusPromise;
    info("focused");
    EventUtils.sendString("http://mochi.test:8888/");
    EventUtils.synthesizeKey("VK_RETURN", {});
    yield BrowserTestUtils.browserLoaded(aBrowser, false, "http://mochi.test:8888/");
  });
});

add_task(function* test_urlbar_fragment_enter() {
  yield withTestPage(function*(aBrowser) {
    gURLBar.focus();
    EventUtils.synthesizeKey("VK_RIGHT", {});
    EventUtils.sendString("#fragment");
    EventUtils.synthesizeKey("VK_RETURN", {});
  });
});

add_task(function* test_backButton_forwardButton() {
  yield withTestPage(function*(aBrowser) {
    // Load a new page in the tab so we can test going back
    aBrowser.loadURI("https://example.com" + DIRECTORY_PATH + "formless_basic.html?second");
    yield BrowserTestUtils.browserLoaded(aBrowser, false,
                                         "https://example.com" + DIRECTORY_PATH +
                                         "formless_basic.html?second");
    yield fillTestPage(aBrowser);

    let forwardButton = document.getElementById("forward-button");
    // We need to wait for the forward button transition to complete before we
    // can click it, so we hook up a listener to wait for it to be ready.
    let forwardTransitionPromise = BrowserTestUtils.waitForEvent(forwardButton, "transitionend");

    let backPromise = BrowserTestUtils.browserStopped(aBrowser);
    EventUtils.synthesizeMouseAtCenter(document.getElementById("back-button"), {});
    yield backPromise;

    // Give a chance for the doorhanger to appear
    yield new Promise(resolve => SimpleTest.executeSoon(resolve));
    ok(!getCaptureDoorhanger("any"), "No doorhanger should be present");

    // Now go forward again after filling
    yield fillTestPage(aBrowser);

    yield forwardTransitionPromise;
    info("transition done");
    yield BrowserTestUtils.waitForCondition(() => {
      return forwardButton.disabled == false;
    });
    let forwardPromise = BrowserTestUtils.browserStopped(aBrowser);
    info("click the forward button");
    EventUtils.synthesizeMouseAtCenter(forwardButton, {});
    yield forwardPromise;
  });
});


add_task(function* test_reloadButton() {
  yield withTestPage(function*(aBrowser) {
    let reloadButton = document.getElementById("urlbar-reload-button");
    let loadPromise = BrowserTestUtils.browserLoaded(aBrowser, false,
                                                     "https://example.com" + DIRECTORY_PATH +
                                                     "formless_basic.html");

    yield BrowserTestUtils.waitForCondition(() => {
      return reloadButton.disabled == false;
    });
    EventUtils.synthesizeMouseAtCenter(reloadButton, {});
    yield loadPromise;
  });
});

add_task(function* test_back_keyboard_shortcut() {
  if (Services.prefs.getIntPref("browser.backspace_action") != 0) {
    ok(true, "Skipped testing backspace to go back since it's disabled");
    return;
  }
  yield withTestPage(function*(aBrowser) {
    // Load a new page in the tab so we can test going back
    aBrowser.loadURI("https://example.com" + DIRECTORY_PATH + "formless_basic.html?second");
    yield BrowserTestUtils.browserLoaded(aBrowser, false,
                                         "https://example.com" + DIRECTORY_PATH +
                                         "formless_basic.html?second");
    yield fillTestPage(aBrowser);

    let backPromise = BrowserTestUtils.browserStopped(aBrowser);
    EventUtils.synthesizeKey("VK_BACK_SPACE", {});
    yield backPromise;
  });
});
