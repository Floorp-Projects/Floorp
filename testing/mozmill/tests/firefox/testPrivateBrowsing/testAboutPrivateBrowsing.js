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
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var RELATIVE_ROOT = '../../shared-modules';
var MODULE_REQUIRES = ['PrivateBrowsingAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  // Create Private Browsing instance and set handler
  pb = new PrivateBrowsingAPI.privateBrowsing(controller);
}

var setupTest = function(module)
{
  // Make sure we are not in PB mode and don't show a prompt
  pb.enabled = false;
  pb.showPrompt = false;
}

var teardownTest = function(test)
{
  pb.reset();
}

/**
 * Verify about:privatebrowsing in regular mode
 */
var testCheckRegularMode = function()
{
  controller.open("about:privatebrowsing");
  controller.waitForPageLoad();

  // XXX Bug 504635 - Can't check DTD entities yet
  var statusText = new elementslib.ID(controller.tabs.activeTab, "errorShortDescTextNormal");

  // Check button to enter Private Browsing mode
  var button = new elementslib.ID(controller.tabs.activeTab, "startPrivateBrowsing");
  controller.click(button);
  controller.waitForPageLoad();

  controller.waitForEval("subject.privateBrowsing.enabled == true", gTimeout, 100,
                         {privateBrowsing: pb});
}

/**
 * Verify about:privatebrowsing in private browsing mode
 */
var testCheckPrivateBrowsingMode = function()
{
  // Start the Private Browsing mode
  pb.start();
  controller.waitForPageLoad();

  var moreInfo = new elementslib.ID(controller.tabs.activeTab, "moreInfoLink");
  controller.waitThenClick(moreInfo, gTimeout);

  // Clicking on the more info link opens a new tab with a page on SUMO
  var targetUrl = UtilsAPI.formatUrlPref("app.support.baseURL") + "private-browsing";

  controller.waitForEval("subject.length == 2", gTimeout, 100, controller.tabs);
  controller.waitForPageLoad();
  UtilsAPI.assertLoadedUrlEqual(controller, targetUrl);
}

/**
 * Map test functions to litmus tests
 */
// testCheckAboutPrivateBrowsing.meta = {litmusids : [9203]};
