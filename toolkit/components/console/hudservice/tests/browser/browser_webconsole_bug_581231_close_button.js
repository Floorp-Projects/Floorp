/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the Web Console close button functions.

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/HUDService.jsm");

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  waitForExplicitFinish();
  content.location.href = TEST_URI;
  waitForFocus(onFocus);
}

function onFocus() {
  let tabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  tabBrowser.addEventListener("DOMContentLoaded", testCloseButton, false);
}

function testCloseButton() {
  let tabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  tabBrowser.removeEventListener("DOMContentLoaded", testCloseButton, false);

  HUDService.activateHUDForContext(gBrowser.selectedTab);

  let hudId = HUDService.displaysIndex()[0];
  let hudBox = HUDService.getHeadsUpDisplay(hudId);

  let closeButton = hudBox.querySelector(".jsterm-close-button");
  ok(closeButton != null, "we have the close button");

  EventUtils.synthesizeMouse(closeButton, 0, 0, {});

  ok(!(hudId in HUDService.windowRegistry), "the console is closed when the " +
     "close button is pressed");

  finish();
}

