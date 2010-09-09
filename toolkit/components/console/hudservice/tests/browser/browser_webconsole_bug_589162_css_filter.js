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

const Cc = Components.classes;

Cu.import("resource://gre/modules/HUDService.jsm");

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

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
function testLogEntry(aOutputNode, aMatchString, aSuccessErrObj, aOnlyVisible, aFailIfFound)
{
  let found = true;
  let notfound = false;
  let foundMsg = aSuccessErrObj.success;
  let notfoundMsg = aSuccessErrObj.err;

  if (aFailIfFound) {
    found = false;
    notfound = true;
    foundMsg = aSuccessErrObj.err;
    notfoundMsg = aSuccessErrObj.success;
  }

  let selector = ".hud-group > *";

  // Skip entries that are hidden by the filter.
  if (aOnlyVisible) {
    selector += ":not(.hud-filtered-by-type)";
  }

  let msgs = aOutputNode.querySelectorAll(selector);
  for (let i = 1, n = msgs.length; i < n; i++) {
    let message = msgs[i].textContent.indexOf(aMatchString);
    if (message > -1) {
      ok(found, foundMsg);
      return;
    }
  }

  ok(notfound, notfoundMsg);
}

function onContentLoaded() {
  gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

  let HUD = HUDService.getDisplayByURISpec(content.location.href);
  let hudId = HUD.getAttribute("id");
  let filterBox = HUD.querySelector(".hud-filter-box");
  let outputNode = HUD.querySelector(".hud-output-node");

  let warningFound = "the unknown CSS property warning is displayed";
  let warningNotFound = "could not find the unknown CSS property warning";

  testLogEntry(outputNode, "foobarCssParser",
    { success: warningFound, err: warningNotFound });

  HUDService.setFilterState(hudId, "cssparser", false);

  warningNotFound = "the unknown CSS property warning is not displayed, " +
    "after filtering";
  warningFound = "the unknown CSS property warning is still displayed, " +
    "after filtering";

  testLogEntry(outputNode, "foobarCssParser",
    { success: warningNotFound, err: warningFound }, true, true);

  HUDService.deactivateHUDForContext(gBrowser.selectedTab);
  finish();
}

/**
 * Unit test for bug 589162:
 * CSS filtering on the console does not work
 */
function test() {
  waitForExplicitFinish();

  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);

    waitForFocus(function () {
      HUDService.activateHUDForContext(gBrowser.selectedTab);

      gBrowser.selectedBrowser.addEventListener("load", onContentLoaded, true);

      content.location = "data:text/html,<div style='font-size:3em;" +
        "foobarCssParser:baz'>test CSS parser filter</div>";
    });
  }, true);

  content.location = TEST_URI;
}
