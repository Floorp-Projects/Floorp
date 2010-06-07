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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anthony Hughes <ashughes@mozilla.com>
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

var gDelay = 2000;

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
}

/**
 * Test the Get Me Out Of Here button from an Untrusted Error page
 */
var testUntrustedPageGetMeOutOfHereButton = function()
{
  // Go to an untrusted website
  controller.open("https://mozilla.org");
  controller.sleep(gDelay);
  
  // Get a reference to the Get Me Out Of Here button
  var getMeOutOfHereButton = new elementslib.ID(controller.tabs.activeTab, 
                                                "getMeOutOfHereButton");
  controller.assertNode(getMeOutOfHereButton);
  
  // Click the button
  controller.click(getMeOutOfHereButton);
  
  // Wait for the redirected page to load
  controller.waitForPageLoad();
  
  // Verify the loaded page is the homepage
  var defaultHomePage = UtilsAPI.getProperty("resource:/browserconfig.properties", 
                                             "browser.startup.homepage");
  
  UtilsAPI.assertLoadedUrlEqual(controller, defaultHomePage);
  
}

/**
 * Map test functions to litmus tests
 */
// testUntrustedPageGetMeOutOfHereButton.meta = {litmusids : [8581]};
