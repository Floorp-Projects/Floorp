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

// Tests the functionality of the "property panel", which allows JavaScript
// objects and DOM nodes to be inspected.

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testPropertyPanel, false);
}

function testPropertyPanel() {
  browser.removeEventListener("DOMContentLoaded", testPropertyPanel, false);

  openConsole();

  hudId = HUDService.displaysIndex()[0];

  var HUD = HUDService.hudReferences[hudId];
  var jsterm = HUD.jsterm;

  let propPanel = jsterm.openPropertyPanel("Test", [
    1,
    /abc/,
    null,
    undefined,
    function test() {},
    {}
  ]);
  is (propPanel.treeView.rowCount, 6, "six elements shown in propertyPanel");
  propPanel.destroy();

  propPanel = jsterm.openPropertyPanel("Test2", {
    "0.02": 0,
    "0.01": 1,
    "02":   2,
    "1":    3,
    "11":   4,
    "1.2":  5,
    "1.1":  6,
    "foo":  7,
    "bar":  8
  });
  is (propPanel.treeView.rowCount, 9, "nine elements shown in propertyPanel");

  let treeRows = propPanel.treeView._rows;
  is (treeRows[0].display, "0.01: 1", "1. element is okay");
  is (treeRows[1].display, "0.02: 0", "2. element is okay");
  is (treeRows[2].display, "1: 3",    "3. element is okay");
  is (treeRows[3].display, "1.1: 6",  "4. element is okay");
  is (treeRows[4].display, "1.2: 5",  "5. element is okay");
  is (treeRows[5].display, "02: 2",   "6. element is okay");
  is (treeRows[6].display, "11: 4",   "7. element is okay");
  is (treeRows[7].display, "bar: 8",  "8. element is okay");
  is (treeRows[8].display, "foo: 7",  "9. element is okay");
  propPanel.destroy();

  finishTest();
}

