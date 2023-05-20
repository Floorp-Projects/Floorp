/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function resizeWindow(windowToResize, width, height) {
  let resizePromise = BrowserTestUtils.waitForEvent(
    windowToResize,
    "resize",
    false
  );
  windowToResize.resizeTo(width, height);
  await resizePromise;
}

add_task(async () => {
  const TEST_LINK = "https://example.com/";
  let originalBrowserWindow =
    Services.wm.getMostRecentWindow("navigator:browser");
  let originalBrowser = originalBrowserWindow.gBrowser;
  // Resize this window so we can check for WM_NCCALCSIZE events below
  await resizeWindow(originalBrowserWindow, 500, 400);
  await resizeWindow(originalBrowserWindow, 600, 500);
  await resizeWindow(originalBrowserWindow, 700, 600);
  let newWindow = await BrowserTestUtils.openNewBrowserWindow({
    url: TEST_LINK,
  });
  registerCleanupFunction(async function () {
    await BrowserTestUtils.closeWindow(newWindow);
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser: originalBrowser, url: "about:windows-messages" },
    async browser => {
      let messagesList = content.document.getElementById("windows-div");
      // This is tricky because the test framework has its own windows
      Assert.greaterOrEqual(
        messagesList.childNodes.length,
        2,
        "should have enough window entries"
      );

      let firstList = true;
      for (let sublist of messagesList.childNodes) {
        const messages = Array.from(sublist.querySelectorAll("li.message"));
        const numberOfMessages = messages.length;
        // Every window gets a few window messages when it opens, so there
        // should be at least a few here.
        Assert.greaterOrEqual(
          numberOfMessages,
          3,
          "should have enough messages for the current window"
        );
        if (firstList) {
          // It would be nice if we could check for a less obscure event.
          // However, since we're resizing the window programatically we don't
          // get WM_SIZE or WM_SIZING events.
          Assert.greaterOrEqual(
            messages.filter(messageLi =>
              messageLi.innerHTML.includes("WM_NCCALCSIZE")
            ).length,
            5,
            "active window should have enough WM_NCCALCSIZE events"
          );
          firstList = false;
        }
        let buttons = sublist.querySelectorAll("button");
        Assert.equal(buttons.length, 1, "should have only one button");
        let clipboardButton = buttons[0];
        clipboardButton.click();
        // Wait until copying is done and the button becomes clickable.
        await BrowserTestUtils.waitForMutationCondition(
          clipboardButton,
          { attributes: true },
          () => !clipboardButton.disabled
        );
        const clipboardText = await navigator.clipboard.readText();
        const numberOfLines = Array.from(clipboardText.matchAll("\n")).length;
        Assert.equal(
          numberOfLines,
          numberOfMessages,
          "should copy the right number of lines to the clipboard"
        );
      }
    }
  );
});
