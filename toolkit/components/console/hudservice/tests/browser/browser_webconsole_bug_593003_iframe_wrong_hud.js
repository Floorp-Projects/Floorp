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
 *  Mihai Șucan <mihai.sucan@gmail.com>
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

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-bug-593003-iframe-wrong-hud.html";

const TEST_IFRAME_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-bug-593003-iframe-wrong-hud-iframe.html";

const TEST_DUMMY_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

let tab1, tab2;

function test() {
  addTab(TEST_URI);
  tab1 = tab;
  browser.addEventListener("load", tab1Loaded, true);
}

/**
 * Check if a log entry exists in the HUD output node.
 *
 * @param {Element} aOutputNode the HUD output node.
 * @param {string} aMatchString the string you want to check if it exists in the
 * output node.
 * @param {boolean} [aOnlyVisible=false] find only messages that are visible,
 * not hidden by the filter.
 * @param {boolean} [aFailIfFound=false] fail the test if the string is found in
 * the output node.
 */

function tab1Loaded(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);
  browser.contentWindow.wrappedJSObject.console.log("FOO");
  try {
    openConsole();
  }
  catch (ex) {
    log(ex);
    log(ex.stack);
  }

  tab2 = gBrowser.addTab(TEST_DUMMY_URI);
  gBrowser.selectedTab = tab2;
  gBrowser.selectedBrowser.addEventListener("load", tab2Loaded, true);
}

function tab2Loaded(aEvent) {
  tab2.linkedBrowser.removeEventListener(aEvent.type, arguments.callee, true);

  HUDService.activateHUDForContext(gBrowser.selectedTab);

  tab1.linkedBrowser.addEventListener("load", tab1Reloaded, true);
  tab1.linkedBrowser.contentWindow.location.reload();
}

function tab1Reloaded(aEvent) {
  tab1.linkedBrowser.removeEventListener(aEvent.type, arguments.callee, true);

  let hudId1 = HUDService.getHudIdByWindow(tab1.linkedBrowser.contentWindow);

  let display1 = HUDService.getOutputNodeById(hudId1);
  let outputNode1 = display1.querySelector(".hud-output-node");

  const successMsg1 = "Found the iframe network request in tab1";
  const errorMsg1 = "Failed to find the iframe network request in tab1";

  testLogEntry(outputNode1, TEST_IFRAME_URI,
               { success: successMsg1, err: errorMsg1}, true);

  let hudId2 = HUDService.getHudIdByWindow(tab2.linkedBrowser.contentWindow);
  let display2 = HUDService.getOutputNodeById(hudId2);
  let outputNode2 = display2.querySelector(".hud-output-node");

  isnot(display1, display2, "the two HUD displays must be different");
  isnot(outputNode1, outputNode2,
        "the two HUD outputNodes must be different");

  const successMsg2 = "The iframe network request is not in tab2";
  const errorMsg2 = "Found the iframe network request in tab2";

  testLogEntry(outputNode2, TEST_IFRAME_URI,
               { success: successMsg2, err: errorMsg2}, true, true);

  HUDService.deactivateHUDForContext(tab2);
  gBrowser.removeTab(tab2);

  tab1 = tab2 = null;

  finishTest();
}
