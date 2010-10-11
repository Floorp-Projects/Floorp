/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const Cu = Components.utils;

Cu.import("resource://gre/modules/HUDService.jsm");

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function tabLoaded() {
  gBrowser.selectedBrowser.removeEventListener("load", tabLoaded, true);

  waitForFocus(function () {
    HUDService.activateHUDForContext(gBrowser.selectedTab);

    // See bugs 574036, 586386 and 587617.

    let HUD = HUDService.getDisplayByURISpec(content.location.href);
    let filterBox = HUD.querySelector(".hud-filter-box");
    let outputNode = HUD.querySelector(".hud-output-node");
    let selection = getSelection();
    let jstermInput = HUD.querySelector(".jsterm-input-node");
    let console = content.wrappedJSObject.console;
    let contentSelection = content.getSelection();

    let make_selection = function () {
      let controller = top.document.commandDispatcher.
        getControllerForCommand("cmd_copy");
      is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

      console.log("Hello world!");

      let range = document.createRange();
      let selectedNode = outputNode.querySelector(".hud-group > label:last-child");
      range.selectNode(selectedNode);
      selection.addRange(range);

      selectedNode.focus();

      goUpdateCommand("cmd_copy");

      controller = top.document.commandDispatcher.
        getControllerForCommand("cmd_copy");
      is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

      waitForClipboard(selectedNode.textContent, clipboard_setup,
        clipboard_copy_done, clipboard_copy_done);
    };

    let clipboard_setup = function () {
      goDoCommand("cmd_copy");
    };

    let clipboard_copy_done = function () {
      selection.removeAllRanges();
      testEnd();
    };

    // Check if we first need to clear any existing selections.
    if (selection.rangeCount > 0 || contentSelection.rangeCount > 0 ||
        jstermInput.selectionStart != jstermInput.selectionEnd) {
      if (jstermInput.selectionStart != jstermInput.selectionEnd) {
        jstermInput.selectionStart = jstermInput.selectionEnd = 0;
      }

      if (selection.rangeCount > 0) {
        selection.removeAllRanges();
      }

      if (contentSelection.rangeCount > 0) {
        contentSelection.removeAllRanges();
      }

      goUpdateCommand("cmd_copy");
      make_selection();
    }
    else {
      make_selection();
    }
  });
}

function testEnd() {
  HUDService.deactivateHUDForContext(gBrowser.selectedTab);
  finish();
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedBrowser.addEventListener("load", tabLoaded, true);

  content.location = TEST_URI;
}

