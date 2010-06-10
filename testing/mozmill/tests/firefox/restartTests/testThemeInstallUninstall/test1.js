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
var MODULE_REQUIRES = ['AddonsAPI', 'ModalDialogAPI', 'TabbedBrowsingAPI'];

const gTimeout = 5000;
const gDownloadTimeout = 60000;

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
  addonsManager = new AddonsAPI.addonsManager();

  persisted.themeName = "Walnut for Firefox";
  persisted.themeId = "{5A170DD3-63CA-4c58-93B7-DE9FF536C2FF}";
  persisted.defaultThemeId = "{972ce4c6-7e08-4474-a285-3208198ce6fd}";

  TabbedBrowsingAPI.closeAllTabs(controller);
}

/*
 * Tests theme installation
 */
var testInstallTheme = function() 
{
  addonsManager.open(controller);
  addonsManager.paneId = "search";

  // Wait for the Browse All Add-ons link and click on it
  var browseAllAddons = addonsManager.getElement({type: "link_browseAddons"});
  addonsManager.controller.waitThenClick(browseAllAddons, gTimeout);

  // The target web page is loaded lazily so wait for the newly created tab first
  controller.waitForEval("subject.tabs.length == 2", gTimeout, 100, controller);
  controller.waitForPageLoad();

  // Open the web page for the Walnut theme directly
  controller.open("https://preview.addons.mozilla.org/en-US/firefox/addon/122");
  controller.waitForPageLoad();

  // Create a modal dialog instance to handle the Software Installation dialog
  var md = new ModalDialogAPI.modalDialog(handleTriggerDialog);
  md.start();

  // Click link to install the theme which triggers a modal dialog
  var triggerLink = new elementslib.XPath(controller.tabs.activeTab,
                                          "//div[@id='addon-summary']/div/div/div/p/a/span");
  controller.waitThenClick(triggerLink, gTimeout);

  // Wait that the Installation pane is selected after the extension has been installed
  addonsManager.controller.waitForEval("subject.manager.paneId == 'installs'", 10000, 100,
                                       {manager: addonsManager});

  // Wait until the Theme has been installed.
  var theme = addonsManager.getListboxItem("addonID", persisted.themeId);
  addonsManager.controller.waitForElement(theme, gDownloadTimeout);

  var themeName = theme.getNode().getAttribute('name');
  addonsManager.controller.assertJS("subject.isValidThemeName == true",
                                    {isValidThemeName: themeName == persisted.themeName});

  addonsManager.controller.assertJS("subject.isThemeInstalled == true",
                                    {isThemeInstalled: theme.getNode().getAttribute('state') == 'success'});

  // Check if restart button is present
  var restartButton = addonsManager.getElement({type: "notificationBar_buttonRestart"});
  addonsManager.controller.waitForElement(restartButton, gTimeout);
}

/**
 * Handle the Software Installation dialog
 */
var handleTriggerDialog = function(controller) 
{
  // Get list of themes which should be installed
  var itemElem = controller.window.document.getElementById("itemList");
  var itemList = new elementslib.Elem(controller.window.document, itemElem);
  controller.waitForElement(itemList, gTimeout);

  // There should be one theme for installation
  controller.assertJS("subject.themes.length == 1",
                      {themes: itemElem.childNodes});

  // Check if the correct theme name is shown
  controller.assertJS("subject.name == '" + persisted.themeName + "'",
                      itemElem.childNodes[0]);

  // Will the theme be installed from https://addons.mozilla.org/?
  controller.assertJS("subject.url.indexOf('addons.mozilla.org/') != -1",
                      itemElem.childNodes[0]);

  // Check if the Cancel button is present
  var cancelButton = new elementslib.Lookup(controller.window.document,
                                            '/id("xpinstallConfirm")/anon({"anonid":"buttons"})/{"dlgtype":"cancel"}');
  controller.assertNode(cancelButton);

  // Wait for the install button is enabled before clicking on it
  var installButton = new elementslib.Lookup(controller.window.document,
                                             '/id("xpinstallConfirm")/anon({"anonid":"buttons"})/{"dlgtype":"accept"}');
  controller.waitForEval("subject.disabled != true", undefined, 100, installButton.getNode());
  controller.click(installButton);
}

/**
 * Map test functions to litmus tests
 */
// testInstallTheme.meta = {litmusids : [7973]};
