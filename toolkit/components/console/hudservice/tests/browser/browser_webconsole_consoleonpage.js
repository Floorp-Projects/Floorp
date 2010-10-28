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

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-own-console.html";

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);
    testOpenWebConsole();
  }, true);
}

function testOpenWebConsole()
{
  openConsole();
  is(HUDService.displaysIndex().length, 1, "WebConsole was opened");

  hudId = HUDService.displaysIndex()[0];
  hud = HUDService.hudWeakReferences[hudId].get();

  testOwnConsole();
}

function testConsoleOnPage(console) {
  // let console = browser.contentWindow.wrappedJSObject.console;
  isnot(console, undefined, "Console object defined on page");
  is(console.foo, "bar", "Custom console is not overwritten");
}

function testOwnConsole()
{
  let console = browser.contentWindow.wrappedJSObject.console;
  // Test console on the page. There is already one so it shouldn't be
  // overwritten by the WebConsole's console.
  testConsoleOnPage(console);

  // Check that the console object is set on the jsterm object although there
  // is no console object added to the page.
  ok(hud.jsterm.console, "JSTerm console is defined");
  ok(hud.jsterm.console === hud._console, "JSTerm console is same as HUD console");

  let iframe =
    browser.contentWindow.document.querySelector("iframe");

  function consoleTester()
  {
    testIFrameConsole(iframe);
  }

  iframe.contentWindow.
    addEventListener("load", consoleTester ,false);

  iframe.contentWindow.document.location = "http://example.com/";

  function testIFrameConsole(iFrame)
  {
    iFrame.contentWindow.removeEventListener("load", consoleTester, true);

    // Test the console in the iFrame.
    let consoleIFrame = iFrame.wrappedJSObject.contentWindow.console;
    // TODO: Fix this test. Not sure what it is intending, and this will
    // change drastically once the lazy console lands
    // ok(browser.contentWindow.wrappedJSObject.console === hud._console, "Console on the page is hud console");

    // Close the hud and see which console is still around.
    HUDService.deactivateHUDForContext(tab);

    executeSoon(function () {
      consoleIFrame = iFrame.contentWindow.console;
      is(consoleIFrame, undefined, "Console object was removed from iFrame");
      testConsoleOnPage(browser.contentWindow.wrappedJSObject.console);
      finishTest();
  });
}

  browser.contentWindow.wrappedJSObject.loadIFrame();
}
