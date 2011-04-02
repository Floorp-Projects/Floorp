/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the getHudReferenceForOutputNode returns a reference when passed
// a hudBox (xul:vbox) or an output box (richlistbox).

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testHudRef,
                           false);
}

function testHudRef() {
  browser.removeEventListener("DOMContentLoaded",testHudRef, false);

  openConsole();
  let hudId = HUDService.displaysIndex()[0];
  let hudBox = HUDService.getHeadsUpDisplay(hudId);
  let hudRef = HUDService.getHudReferenceForOutputNode(hudBox);

  ok(hudRef, "We have a hudRef");

  let outBox = HUDService.getOutputNodeById(hudId);
  let hudRef2 = HUDService.getHudReferenceForOutputNode(outBox);

  ok(hudRef2, "We have the second hudRef");
  is(hudRef, hudRef2, "The two hudRefs are identical");

  finishTest();
}
