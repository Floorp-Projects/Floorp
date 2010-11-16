/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "data:text/html,Web Console test for bug 588342";
let fm, notificationBox, input;

function test()
{
  fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  notificationBox = gBrowser.getNotificationBox(browser);
  let DOMNodeInserted = false;

  document.addEventListener("DOMNodeInserted", function(aEvent) {
    input = notificationBox.querySelector(".jsterm-input-node");
    if (input && !DOMNodeInserted) {
      DOMNodeInserted = true;
      document.removeEventListener(aEvent.type, arguments.callee, false);
      if (!input.getAttribute("focused")) {
        input.addEventListener("focus", function(aEvent) {
          input.removeEventListener(aEvent.type, arguments.callee, false);
          executeSoon(runTest);
        }, false);
      }
      else {
        executeSoon(runTest);
      }
    }
  }, false);

  waitForFocus(function() {
    openConsole();
  }, content);
}

function runTest() {
  is(input.getAttribute("focused"), "true", "input node is focused");
  isnot(fm.focusedWindow, content, "content document has no focus");

  let DOMNodeRemoved = false;
  function domNodeRemoved(aEvent) {
    executeSoon(function() {
      if (!DOMNodeRemoved && !notificationBox.querySelector(".hud-box")) {
        DOMNodeRemoved = true;
        document.removeEventListener(aEvent.type, domNodeRemoved, false);
        is(fm.focusedWindow, browser.contentWindow,
           "content document has focus");
        input = notificationBox = fm = null;
        finishTest();
      }
    });
  }
  document.addEventListener("DOMNodeRemoved", domNodeRemoved, false);
  HUDService.deactivateHUDForContext(tab);
}

