/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var source = "data:text/html,text<link%20href='http://example.com/'%20/>more%20text<a%20href='mailto:abc@def.ghi'>email</a>";
var gViewSourceWindow, gContextMenu, gCopyLinkMenuItem, gCopyEmailMenuItem;

var expectedData = [];

add_task(function *() {
  // Full source in view source window
  let newWindow = yield loadViewSourceWindow(source);
  yield SimpleTest.promiseFocus(newWindow);

  yield* onViewSourceWindowOpen(newWindow, false);

  let contextMenu = gViewSourceWindow.document.getElementById("viewSourceContextMenu");

  for (let test of expectedData) {
    yield* checkMenuItems(contextMenu, false, test[0], test[1], test[2], test[3]);
  }

  yield new Promise(resolve => {
    closeViewSourceWindow(newWindow, resolve);
  });

  // Selection source in view source tab
  expectedData = [];
  let newTab = yield openDocumentSelect(source, "body");
  yield* onViewSourceWindowOpen(window, true);

  contextMenu = document.getElementById("contentAreaContextMenu");

  for (let test of expectedData) {
    yield* checkMenuItems(contextMenu, true, test[0], test[1], test[2], test[3]);
  }

  gBrowser.removeTab(newTab);

  // Selection source in view source window
  yield pushPrefs(["view_source.tab", false]);

  expectedData = [];
  newWindow = yield openDocumentSelect(source, "body");
  yield SimpleTest.promiseFocus(newWindow);

  yield* onViewSourceWindowOpen(newWindow, false);

  contextMenu = newWindow.document.getElementById("viewSourceContextMenu");

  for (let test of expectedData) {
    yield* checkMenuItems(contextMenu, false, test[0], test[1], test[2], test[3]);
  }

  yield new Promise(resolve => {
    closeViewSourceWindow(newWindow, resolve);
  });
});

function* onViewSourceWindowOpen(aWindow, aIsTab) {
  gViewSourceWindow = aWindow;

  gCopyLinkMenuItem = aWindow.document.getElementById(aIsTab ? "context-copylink" : "context-copyLink");
  gCopyEmailMenuItem = aWindow.document.getElementById(aIsTab ? "context-copyemail" : "context-copyEmail");

  let browser = aIsTab ? gBrowser.selectedBrowser : gViewSourceWindow.gBrowser;
  yield ContentTask.spawn(browser, null, function* (arg) {
    let tags = content.document.querySelectorAll("a[href]");
    Assert.equal(tags[0].href, "view-source:http://example.com/", "Link has correct href");
    Assert.equal(tags[1].href, "mailto:abc@def.ghi", "Link has correct href");
  });

  expectedData.push(["a[href]", true, false, "http://example.com/"]);
  expectedData.push(["a[href^=mailto]", false, true, "abc@def.ghi"]);
  expectedData.push(["span", false, false, null]);
}

function* checkMenuItems(contextMenu, isTab, selector, copyLinkExpected, copyEmailExpected, expectedClipboardContent) {

  let browser = isTab ? gBrowser.selectedBrowser : gViewSourceWindow.gBrowser;
  yield ContentTask.spawn(browser, { selector: selector }, function* (arg) {
    content.document.querySelector(arg.selector).scrollIntoView();
  });

  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  yield BrowserTestUtils.synthesizeMouseAtCenter(selector,
          { type: "contextmenu", button: 2}, browser);
  yield popupShownPromise;

  is(gCopyLinkMenuItem.hidden, !copyLinkExpected, "Copy link menuitem is " + (copyLinkExpected ? "not hidden" : "hidden"));
  is(gCopyEmailMenuItem.hidden, !copyEmailExpected, "Copy email menuitem is " + (copyEmailExpected ? "not hidden" : "hidden"));

  if (copyLinkExpected || copyEmailExpected) {
    yield new Promise((resolve, reject) => {
      waitForClipboard(expectedClipboardContent, function() {
        if (copyLinkExpected)
          gCopyLinkMenuItem.click();
        else
          gCopyEmailMenuItem.click();
      }, resolve, reject);
    });
  }

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  yield popupHiddenPromise;
}
