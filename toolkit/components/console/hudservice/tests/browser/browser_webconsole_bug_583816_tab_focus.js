/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

let inputNode;

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  waitForFocus(function() {
    openConsole();

    let hudId = HUDService.getHudIdByWindow(content);
    HUD = HUDService.hudReferences[hudId];

    let display = HUDService.getOutputNodeById(hudId);
    inputNode = display.querySelector(".jsterm-input-node");

    inputNode.focus();
    executeSoon(function() {
      is(inputNode.getAttribute("focused"), "true", "inputNode is focused");
      HUD.jsterm.setInputValue("doc");
      inputNode.addEventListener("keyup", firstTab, false);
      EventUtils.synthesizeKey("VK_TAB", {});
    });
  }, content);
}

function firstTab(aEvent) {
  this.removeEventListener(aEvent.type, arguments.callee, false);

  is(inputNode.getAttribute("focused"), "true", "inputNode is still focused");
  isnot(this.value, "doc", "input autocompleted");

  HUD.jsterm.setInputValue("foobarbaz" + Date.now());

  EventUtils.synthesizeKey("VK_TAB", {});

  executeSoon(secondTab);
}

function secondTab() {
  isnot(inputNode.getAttribute("focused"), "true",
          "inputNode is no longer focused");

  HUD = inputNode = null;
  HUDService.deactivateHUDForContext(gBrowser.selectedTab);
  executeSoon(finish);
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}

