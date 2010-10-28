/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console limits the number of lines displayed according to
// the user's preferences.

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testLineLimit,
                                            false);
}

function testLineLimit() {
  browser.removeEventListener("DOMContentLoaded",testLineLimit, false);

  openConsole();

  hudId = HUDService.displaysIndex()[0];
  let console = browser.contentWindow.wrappedJSObject.console;
  hudBox = HUDService.getHeadsUpDisplay(hudId);

  let prefBranch = Services.prefs.getBranch("devtools.hud.");
  prefBranch.setIntPref("loglimit", 20);

  for (let i = 0; i < 20; i++) {
    console.log("foo");
  }
  is(countMessageNodes(), 20, "there are 20 message nodes in the output " +
     "when the log limit is set to 20");
  isnot(countGroupNodes(), 0, "there is at least one group node in the " +
     "output when the log limit is set to 20");

  console.log("bar");
  is(countMessageNodes(), 20, "there are still 20 message nodes in the " +
     "output when adding one more");

  prefBranch.setIntPref("loglimit", 30);
  for (let i = 0; i < 20; i++) {
    console.log("boo");
  }
  is(countMessageNodes(), 30, "there are 30 message nodes in the output " +
     "when the log limit is set to 30");

  prefBranch.setIntPref("loglimit", 0);
  console.log("baz");
  is(countMessageNodes(), 0, "there are no message nodes in the output when " +
     "the log limit is set to zero");
  is(countGroupNodes(), 0, "there are no group nodes in the output when the " +
     "log limit is set to zero");

  prefBranch.clearUserPref("loglimit");
  prefBranch = console = null;
  finishTest();
}

function countMessageNodes() {
  let hudId = HUDService.displaysIndex()[0];
  let hudBox = HUDService.getHeadsUpDisplay(hudId);
  return hudBox.querySelectorAll(".hud-msg-node").length;
}

function countGroupNodes() {
  let hudId = HUDService.displaysIndex()[0];
  let hudBox = HUDService.getHeadsUpDisplay(hudId);
  return hudBox.querySelectorAll(".hud-group").length;
}
