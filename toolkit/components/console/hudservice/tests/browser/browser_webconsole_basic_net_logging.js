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

// Tests that the page's resources are displayed in the console as they're
// loaded

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";
const TEST_NETWORK_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-network.html" + "?_date=" + Date.now();

function test() {
  addTab(TEST_NETWORK_URI);
  browser.addEventListener("DOMContentLoaded", onLoad, false);
}

function onLoad() {
  browser.removeEventListener("DOMContentLoaded", onLoad, false);
  openConsole();
  hudId = HUDService.displaysIndex()[0];
  // Now reload with the console visible.
  // HUDService.clearDisplay(hudId);
  browser.addEventListener("DOMContentLoaded", testBasicNetLogging,
                            false);
  browser.contentWindow.wrappedJSObject.document.location = TEST_NETWORK_URI;
}

function testBasicNetLogging() {
  browser.removeEventListener("DOMContentLoaded", testBasicNetLogging,
                              false);
  hudBox = HUDService.getHeadsUpDisplay(hudId);
  outputNode = hudBox.querySelector(".hud-output-node");
  log(outputNode);
  let testOutput = [];
  let nodes = outputNode.querySelectorAll(".hud-msg-node");
  log(nodes);

  executeSoon(function (){

    ok(nodes.length == 2, "2 children in output");
    ok(nodes[0].textContent.indexOf("test-network") > -1, "found test-network");
    // TODO: Need to figure out why these 2 are never logged - see bug 603251
    // ok(testOutput[1].indexOf("testscript") > -1, "found testscript");
    // ok(testOutput[2].indexOf("test-image") > -1, "found test-image");
    ok(nodes[1].textContent.indexOf("network console") > -1, "found network console");
    finishTest();
  });
}


