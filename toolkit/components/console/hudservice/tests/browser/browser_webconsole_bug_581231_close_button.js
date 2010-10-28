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

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testCloseButton, false);
}

function testCloseButton() {
  browser.removeEventListener("DOMContentLoaded", testCloseButton, false);

  openConsole();

  hudId = HUDService.displaysIndex()[0];
  hudBox = HUDService.getHeadsUpDisplay(hudId);

  let closeButton = hudBox.querySelector(".jsterm-close-button");
  ok(closeButton != null, "we have the close button");


  // XXX: ASSERTION: ###!!! ASSERTION: XPConnect is being called on a scope without a 'Components' property!: 'Error', file /home/ddahl/code/moz/mozilla-central/mozilla-central/js/src/xpconnect/src/xpcwrappednativescope.cpp, line 795

  EventUtils.synthesizeMouse(closeButton, 0, 0, {});

  executeSoon(function (){
    ok(!(hudId in HUDService.windowRegistry), "the console is closed when the " +
     "close button is pressed");
    closeButton = null;
    finishTest();
  });
}
