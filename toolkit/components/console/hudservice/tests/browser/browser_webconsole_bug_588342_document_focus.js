/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/HUDService.jsm");

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
let notificationBox;
let input;

function tabLoad(aEvent) {
  gBrowser.selectedBrowser.removeEventListener(aEvent.type, arguments.callee, true);

  notificationBox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
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
    HUDService.activateHUDForContext(gBrowser.selectedTab);
  }, content);
}

function runTest() {
  is(input.getAttribute("focused"), "true", "input node is focused");
  isnot(fm.focusedWindow, content, "content document has no focus");

  let DOMNodeRemoved = false;
  document.addEventListener("DOMNodeRemoved", function(aEvent) {
    executeSoon(function() {
      if (!DOMNodeRemoved && !notificationBox.querySelector(".hud-box")) {
        DOMNodeRemoved = true;
        document.removeEventListener(aEvent.type, arguments.callee, false);
        is(fm.focusedWindow, content, "content document has focus");
        input = notificationBox = fm = null;
        finish();
      }
    });
  }, false);

  HUDService.deactivateHUDForContext(gBrowser.selectedTab);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedBrowser.addEventListener("load", tabLoad, true);

  content.location = TEST_URI;
}

