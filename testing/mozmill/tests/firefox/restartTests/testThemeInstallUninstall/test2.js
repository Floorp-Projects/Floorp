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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Aakash Desai <adesai@mozilla.com>
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
var RELATIVE_ROOT = '../../../shared-modules';
var MODULE_REQUIRES = ['AddonsAPI', 'PrefsAPI', 'UtilsAPI'];

const gTimeout = 5000;

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
  addonsManager = new AddonsAPI.addonsManager();
}

/*
 * Verifies the theme has been installed
 */
var testCheckInstalledTheme = function()
{
  addonsManager.open(controller);
  addonsManager.paneId = "themes";

  // The installed theme should be the current theme in the list
  var theme = addonsManager.getListboxItem("addonID", persisted.themeId);
  addonsManager.controller.waitThenClick(theme, gTimeout);
  addonsManager.controller.assertJS("subject.isCurrentTheme == true",
                                    {isCurrentTheme: theme.getNode().getAttribute('current') == 'true'});

  addonsManager.close();
}

/*
 * Tests changing the theme back to default
 */
var testThemeChange = function() 
{
  addonsManager.open(controller);
  addonsManager.paneId = "themes";

  // Select the default theme and click the use theme button
  var theme = addonsManager.getListboxItem("addonID", persisted.defaultThemeId);
  addonsManager.controller.waitThenClick(theme, gTimeout);

  var useThemeButton = addonsManager.getElement({type: "listbox_button", subtype: "useTheme", value: theme});
  addonsManager.controller.waitThenClick(useThemeButton, gTimeout);

  // Wait for the restart button
  var restartButton = addonsManager.getElement({type: "notificationBar_buttonRestart"});
  addonsManager.controller.waitForElement(restartButton, gTimeout);

  // Verify useThemeButton is not visible 
  addonsManager.controller.assertProperty(useThemeButton, "disabled", "true");

  // ... and theme description has changed
  var description = theme.getNode().getAttribute('description');
  addonsManager.controller.assertJS("subject.hasDescriptionChanged == true",
                                    {hasDescriptionChanged: description.indexOf('Restart') != -1});
}

/**
 * Map test functions to litmus tests
 */
// testCheckInstalledTheme.meta = {litmusids : [7973]};
// testThemeChange.meta = {litmusids : [8168]};
