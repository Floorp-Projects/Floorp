/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "data:text/html,test for bug 587615";

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let HUDBox = HUD.HUDBox;

  content.wrappedJSObject.console.log("hello world!");

  isnot(HUDBox.lastTimestamp, 0, "display.lastTimestamp is not 0");

  HUDService.clearDisplay(hudId);

  is(HUDBox.lastTimestamp, 0, "display.lastTimestamp is 0");

  content.wrappedJSObject.console.log("hello world 2!");

  isnot(HUDBox.lastTimestamp, 0, "display.lastTimestamp is not 0");

  HUD.jsterm.clearOutput();

  is(HUDBox.lastTimestamp, 0, "display.lastTimestamp is 0");

  finishTest();
}

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoad, true);
}

