/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudWeakReferences[hudId].get();

  let longMessage = "";
  for (let i = 0; i < 20; i++) {
    longMessage += "LongNonwrappingMessage";
  }

  for (let i = 0; i < 100; i++) {
    HUD.console.log("test message " + i);
  }

  HUD.console.log(longMessage);

  for (let i = 0; i < 100; i++) {
    HUD.console.log("test message " + i);
  }

  HUD.jsterm.execute("1+1");

  executeSoon(function() {
    isnot(HUD.outputNode.scrollTop, 0, "scroll location is not at the top");

    let node = HUD.outputNode.querySelector(".hud-group > *:last-child");
    let rectNode = node.getBoundingClientRect();
    let rectOutput = HUD.outputNode.getBoundingClientRect();

    // Visible scroll viewport.
    let height = HUD.outputNode.scrollHeight - HUD.outputNode.scrollTop;

    // Top position of the last message node, relative to the outputNode.
    let top = rectNode.top - rectOutput.top;

    // Bottom position of the last message node, relative to the outputNode.
    let bottom = rectNode.bottom - rectOutput.top;

    ok(top >= 0 && bottom <= height, "last message is visible");

    finishTest();
  });
}

function test() {
  addTab("data:text/html,Web Console test for bug 601352");
  browser.addEventListener("load", tabLoad, true);
}

