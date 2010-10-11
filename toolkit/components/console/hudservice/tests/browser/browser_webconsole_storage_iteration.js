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

// Test that the iterator API of the console message store works.

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testStorageIteration, false);
}

function testStorageIteration() {
  browser.removeEventListener("DOMContentLoaded", testStorageIteration,
                              false);

  openConsole();

  let cs = HUDService.storage;

  // Must have enough entries present to avoid exhausting the iterators below.
  cs.createDisplay("foo");
  for (let i = 0; i < 300; i++) {
    cs.recordEntry("foo", { logLevel: "network", message: "foo" });
  }

  var id = "foo";
  var it = cs.displayStore(id);
  var entry = it.next();
  var entry2 = it.next();

  let entries = [];
  for (var i = 0; i < 100; i++) {
    let _entry = it.next();
    entries.push(_entry);
  }

  ok(entries.length == 100, "entries length == 100");

  let entries2 = [];

  for (var i = 0; i < 100; i++){
    let _entry = it.next();
    entries2.push(_entry);
  }

  ok(entries[0].id != entries2[0].id,
     "two distinct pages of log entries");

  cs.removeDisplay("foo");
  cs = null;
  
  finishTest();
}

