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

// Tests that exceptions thrown by content don't show up twice in the Web
// Console.

const TEST_DUPLICATE_ERROR_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-duplicate-error.html";

function test() {
  addTab(TEST_DUPLICATE_ERROR_URI);
  browser.addEventListener("DOMContentLoaded", testDuplicateErrors, false);
}

function testDuplicateErrors() {
  browser.removeEventListener("DOMContentLoaded", testDuplicateErrors,
                              false);
  openConsole();

  let hudId = HUDService.displaysIndex()[0];
  HUDService.clearDisplay(hudId);

  Services.console.registerListener(consoleObserver);

  content.location.reload();
}

var consoleObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function (aMessage)
  {
    // we ignore errors we don't care about
    if (!(aMessage instanceof Ci.nsIScriptError) ||
      aMessage.category != "content javascript") {
      return;
    }

    Services.console.unregisterListener(this);

    var display = HUDService.getDisplayByURISpec(content.location.href);
    var outputNode = display.querySelectorAll(".hud-output-node")[0];

    executeSoon(function () {
      var text = outputNode.textContent;
      var error1pos = text.indexOf("fooDuplicateError1");
      ok(error1pos > -1, "found fooDuplicateError1");
      if (error1pos > -1) {
        ok(text.indexOf("fooDuplicateError1", error1pos + 1) == -1,
          "no duplicate for fooDuplicateError1");
      }

      ok(text.indexOf("test-duplicate-error.html") > -1,
        "found test-duplicate-error.html");

      finishTest();
    });
  }
};
