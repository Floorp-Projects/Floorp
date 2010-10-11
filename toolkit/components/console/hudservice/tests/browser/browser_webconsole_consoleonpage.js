/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/HUDService.jsm");

let hud;
let hudId;

function testOpenWebConsole()
{
  HUDService.activateHUDForContext(gBrowser.selectedTab);
  is(HUDService.displaysIndex().length, 1, "WebConsole was opened");

  hudId = HUDService.displaysIndex()[0];
  hud = HUDService.hudWeakReferences[hudId].get();

  testOwnConsole();
}

function testConsoleOnPage() {
  let console = content.wrappedJSObject.console;
  isnot(console, undefined, "Console object defined on page");
  is(console.foo, "bar", "Custom console is not overwritten");
}

function testOwnConsole()
{
  // Test console on the page. There is already one so it shouldn't be
  // overwritten by the WebConsole's console.
  testConsoleOnPage();

  // Check that the console object is set on the jsterm object although there
  // is no console object added to the page.
  ok(hud.jsterm.console, "JSTerm console is defined");
  ok(hud.jsterm.console === hud._console, "JSTerm console is same as HUD console");

  content.wrappedJSObject.loadIFrame(function(iFrame) {
    // Test the console in the iFrame.
    let consoleIFrame = iFrame.wrappedJSObject.contentWindow.console;
    isnot(consoleIFrame, undefined, "Console object defined in iFrame");

    ok(consoleIFrame === hud._console, "Console on the page is hud console");

    // Close the hud and see which console is still around.
    HUDService.deactivateHUDForContext(gBrowser.selectedTab);

    executeSoon(function () {
      consoleIFrame = iFrame.wrappedJSObject.contentWindow.console;
      is(consoleIFrame, undefined, "Console object was removed from iFrame");
      testConsoleOnPage();

      hud = hudId = null;
      gBrowser.removeCurrentTab();
      finish();
    });
  });
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    waitForFocus(testOpenWebConsole, content);
  }, true);

  content.location =
    "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-own-console.html";
}
