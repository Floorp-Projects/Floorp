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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aakash Desai <adesai@mozilla.com>
 *   Raymond Etornam Agbeame <retornam@mozilla.com>
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
var MODULE_REQUIRES = ['AddonsAPI'];

const gDelay = 0;
const gTimeout = 5000;

const localTestFolder = collector.addHttpResource('./files');

var plugins = {"darwin": "DefaultPlugin.plugin",
               "winnt": "npnul32.dll",
               "linux": "libnullplugin.so"};

var setupModule = function(module) 
{
  controller = mozmill.getBrowserController();
  addonsManager = new AddonsAPI.addonsManager();
}

var teardownModule = function(module)
{
  addonsManager.close();
}

/**
 * Test disabling the default plugin
 */
var testDisableEnablePlugin = function()
{
  var pluginId = plugins[mozmill.platform];

  // Open Add-ons Manager and go to the themes pane
  addonsManager.open(controller);

  // Select the default plugin and disable it
  addonsManager.setPluginState("addonID", pluginId, false);

  // Check that the plugin is shown as disabled on web pages
  var status = new elementslib.ID(controller.tabs.activeTab, "status");

  controller.open(localTestFolder + "plugin.html");
  controller.waitForPageLoad();
  controller.assertText(status, "disabled");

  // Enable the default plugin
  addonsManager.setPluginState("addonID", pluginId, true);

  // Check that the plugin is shown as disabled on web pages
  controller.open(localTestFolder + "plugin.html");
  controller.waitForPageLoad();
  controller.assertText(status, "enabled");
}

/**
 * Map test functions to litmus tests
 */
// testDisableEnablePlugin.meta = {litmusids : [8511]};
