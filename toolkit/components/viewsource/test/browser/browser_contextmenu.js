/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let source = "data:text/html,text<link%20href='http://example.com/'%20/>more%20text<a%20href='mailto:abc@def.ghi'>email</a>";
let gViewSourceWindow, gContextMenu, gCopyLinkMenuItem, gCopyEmailMenuItem;

let expectedData = [];
let currentTest = 0;
let partialTestRunning = false;

function test() {
  waitForExplicitFinish();
  openViewSourceWindow(source, onViewSourceWindowOpen);
}

function onViewSourceWindowOpen(aWindow) {
  gViewSourceWindow = aWindow;

  gContextMenu = aWindow.document.getElementById("viewSourceContextMenu");
  gCopyLinkMenuItem = aWindow.document.getElementById("context-copyLink");
  gCopyEmailMenuItem = aWindow.document.getElementById("context-copyEmail");

  let aTags = aWindow.gBrowser.contentDocument.querySelectorAll("a[href]");
  is(aTags[0].href, "view-source:http://example.com/", "Link has correct href");
  is(aTags[1].href, "mailto:abc@def.ghi", "Link has correct href");
  let spanTag = aWindow.gBrowser.contentDocument.querySelector("span");

  expectedData.push([aTags[0], true, false, "http://example.com/"]);
  expectedData.push([aTags[1], false, true, "abc@def.ghi"]);
  expectedData.push([spanTag, false, false, null]);

  waitForFocus(runNextTest, aWindow);
}

function runNextTest() {
  if (currentTest == expectedData.length) {
    closeViewSourceWindow(gViewSourceWindow, function() {
      if (partialTestRunning) {
        finish();
        return;
      }
      partialTestRunning = true;
      currentTest = 0;
      expectedData = [];
      openDocumentSelect(source, "body", onViewSourceWindowOpen);
    });
    return;
  }
  let test = expectedData[currentTest++];
  checkMenuItems(test[0], test[1], test[2], test[3]);
}

function checkMenuItems(popupNode, copyLinkExpected, copyEmailExpected, expectedClipboardContent) {
  popupNode.scrollIntoView();

  let cachedEvent = null;
  let mouseFn = function(event) {
    cachedEvent = event;
  };

  gViewSourceWindow.gBrowser.contentWindow.addEventListener("mousedown", mouseFn, false);
  EventUtils.synthesizeMouseAtCenter(popupNode, { type: "contextmenu", button: 2 }, gViewSourceWindow.gBrowser.contentWindow);
  gViewSourceWindow.gBrowser.contentWindow.removeEventListener("mousedown", mouseFn, false);

  gContextMenu.openPopup(popupNode, "after_start", 0, 0, false, false, cachedEvent);

  is(gCopyLinkMenuItem.hidden, !copyLinkExpected, "Copy link menuitem is " + (copyLinkExpected ? "not hidden" : "hidden"));
  is(gCopyEmailMenuItem.hidden, !copyEmailExpected, "Copy email menuitem is " + (copyEmailExpected ? "not hidden" : "hidden"));

  if (!copyLinkExpected && !copyEmailExpected) {
    runNextTest();
    return;
  }

  waitForClipboard(expectedClipboardContent, function() {
    if (copyLinkExpected)
      gCopyLinkMenuItem.doCommand();
    else
      gCopyEmailMenuItem.doCommand();
    gContextMenu.hidePopup();
  }, runNextTest, runNextTest);
}
