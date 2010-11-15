/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-network.html";

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  hud = HUDService.hudWeakReferences[hudId].get();

  for (let i = 0; i < 200; i++) {
    hud.console.log("test message " + i);
  }

  HUDService.setFilterState(hudId, "network", false);

  hud.filterBox.value = "test message";
  HUDService.updateFilterText(hud.filterBox);

  browser.addEventListener("load", tabReload, true);

  executeSoon(function() {
    content.location.reload();
  });
}

function tabReload(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  let msgNode = hud.outputNode.querySelector(".hud-network");
  ok(msgNode, "found network message");
  ok(msgNode.classList.contains("hud-filtered-by-type"),
    "network message is filtered by type");
  ok(msgNode.classList.contains("hud-filtered-by-string"),
    "network message is filtered by string");

  ok(hud.outputNode.scrollTop > 0, "scroll location is not at the top");

  is(hud.outputNode.scrollTop,
    hud.outputNode.scrollHeight - hud.outputNode.clientHeight,
    "scroll location is correct");

  executeSoon(finishTest);
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}

