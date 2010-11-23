/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-bug-601177-log-levels.html";

let msgs;

function onContentLoaded()
{
  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  msgs = HUD.outputNode.querySelectorAll(".hud-group > *");

  ok(findEntry("hud-info", "test-bug-601177-log-levels.html"),
    "found test-bug-601177-log-levels.html");

  ok(findEntry("hud-info", "test-bug-601177-log-levels.js"),
    "found test-bug-601177-log-levels.js");

  ok(findEntry("hud-info", "test-image.png"),
    "found test-image.png");

  ok(findEntry("hud-error", "foobar-known-to-fail.png"),
    "found foobar-known-to-fail.png");

  ok(findEntry("hud-exception", "foobarBug601177exception"),
    "found exception");

  ok(findEntry("hud-warn", "undefinedPropertyBug601177"),
    "found strict warning");

  msgs = null;
  Services.prefs.setBoolPref("javascript.options.strict", false);
  finishTest();
}

function findEntry(aClass, aString)
{
  for (let i = 0, n = msgs.length; i < n; i++) {
    if (msgs[i].classList.contains(aClass) &&
        msgs[i].textContent.indexOf(aString) > -1) {
      return true;
    }
  }
  return false;
}

function test()
{
  addTab("data:text/html,Web Console test for bug 601177: log levels");

  Services.prefs.setBoolPref("javascript.options.strict", true);

  browser.addEventListener("load", function(aEvent) {
    browser.removeEventListener(aEvent.type, arguments.callee, true);

    openConsole();

    browser.addEventListener("load", function(aEvent) {
      browser.removeEventListener(aEvent.type, arguments.callee, true);
      executeSoon(onContentLoaded);
    }, true);
    content.location = TEST_URI;
  }, true);
}

