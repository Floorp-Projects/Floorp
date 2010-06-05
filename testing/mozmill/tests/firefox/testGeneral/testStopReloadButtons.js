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
 * The Original Code is Mozmill Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aakash Desai <adesai@mozilla.com>
 *   Henrik Skupin <hskupin@mozilla.com>
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

// Include necessary modules
var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
}

/**
 * Test the stop and reload buttons
 */
var testStopAndReload = function()
{
  var url = "http://www.mozilla.com/en-US/";

  // Make sure we have a blank page
  controller.open("about:blank");
  controller.waitForPageLoad();

  // Go to the URL and start loading for some milliseconds
  controller.open(url);
  controller.sleep(100);
  controller.click(new elementslib.ID(controller.window.document, "stop-button"));

  // Even an element at the top of a page shouldn't exist when we hit the stop
  // button extremely fast
  var elem = new elementslib.ID(controller.tabs.activeTab, "query");
  controller.assertNodeNotExist(elem);
  controller.sleep(gDelay);

  // Reload, wait for it to completely loading and test again
  controller.open(url);
  controller.waitForPageLoad();

  controller.waitForElement(elem, gTimeout);
}

/**
 * Map test functions to litmus tests
 */
// testStopAndReload.meta = {litmusids : [8030]};
