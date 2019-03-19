/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the MozTogglePictureInPicture event is ignored if
 * fired by unprivileged web content.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab({
    url: TEST_PAGE,
    gBrowser,
  }, async browser => {
    // For now, the easiest way to ensure that this didn't happen is to fail
    // if we receive the PictureInPicture:Request message.
    const MESSAGE = "PictureInPicture:Request";
    let sawMessage = false;
    let listener = msg => {
      sawMessage = true;
    };
    browser.messageManager.addMessageListener(MESSAGE, listener);
    await ContentTask.spawn(browser, null, async () => {
      content.wrappedJSObject.fireEvents();
    });
    browser.messageManager.removeMessageListener(MESSAGE, listener);
    ok(!sawMessage, "Got PictureInPicture:Request message unexpectedly.");
  });
});
