/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that the input box expands as the user types long lines.

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testJSInputExpansion, false);
}

function testJSInputExpansion() {
  browser.removeEventListener("DOMContentLoaded", testJSInputExpansion,
                              false);

  openConsole();

  hudId = HUDService.displaysIndex()[0];

  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let input = jsterm.inputNode;
  input.focus();

  is(input.getAttribute("multiline"), "true", "multiline is enabled");
  // Tests if the inputNode expands.
  input.value = "hello\nworld\n";
  let length = input.value.length;
  input.selectionEnd = length;
  input.selectionStart = length;
  function getHeight()
  {
    let h = browser.contentDocument.defaultView.getComputedStyle(input, null)
      .getPropertyValue("height");
    return parseInt(h);
  }
  let initialHeight = getHeight();
  // Performs an "d". This will trigger/test for the input event that should
  // change the "row" attribute of the inputNode.
  EventUtils.synthesizeKey("d", {});
  let newHeight = getHeight();
  ok(initialHeight < newHeight, "Height changed: " + newHeight);

  // Add some more rows. Tests for the 8 row limit.
  input.value = "row1\nrow2\nrow3\nrow4\nrow5\nrow6\nrow7\nrow8\nrow9\nrow10\n";
  length = input.value.length;
  input.selectionEnd = length;
  input.selectionStart = length;
  EventUtils.synthesizeKey("d", {});
  let newerHeight = getHeight();

  ok(newerHeight > newHeight, "height changed: " + newerHeight);

  // Test if the inputNode shrinks again.
  input.value = "";
  EventUtils.synthesizeKey("d", {});
  let height = getHeight();
  ok(height == initialHeight, "height shrank to original size");

  finishTest();
}
