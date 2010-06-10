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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tracy Walker <twalker@mozilla.com>
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

// Holds the instance of the newly created window
var newWindow = null;

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
}

var teardownModule = function(module) {
  if (newWindow != controller.window) {
    newWindow.close();
  }
}

var testNewWindow = function () {
  // Open the current homepage to get its value
  controller.click(new elementslib.ID(controller.window.document, "home-button"));
  controller.waitForPageLoad();

  var homePageURL = controller.window.document.getElementById("urlbar").value;
  controller.sleep(gDelay);

  // Ensure current window does not have the home page loaded
  controller.open('about:blank');
  controller.waitForPageLoad();

  // Get the pre test window count
  var windowCountBeforeTest = mozmill.utils.getWindows().length;

  // Open a new window, by default it will open with the default home page
  controller.click(new elementslib.Elem(controller.menus['file-menu'].menu_newNavigator));
  controller.sleep(1000);

  // Check if a new window has been opened
  newWindow = mozmill.wm.getMostRecentWindow("navigator:browser");
  var controller2 = new mozmill.controller.MozMillController(newWindow);
  controller2.waitForPageLoad();

  // Get the post test window count
  var windowCountAfterTest = mozmill.utils.getWindows().length;
  controller.assertJS("subject.countAfter == subject.countBefore",
                      {countAfter: windowCountAfterTest, countBefore: windowCountBeforeTest + 1});

  // Checks that the new window contains the default home page.
  var locationBar = new elementslib.ID(controller2.window.document, "urlbar");
  controller2.waitForElement(locationBar, gTimeout);
  controller2.assertValue(locationBar, homePageURL);
}

/**
 * Map test functions to litmus tests
 */
// testNewWindow.meta = {litmusids : [7954]};
