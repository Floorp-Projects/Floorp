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

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
}

var teardownModule = function(module)
{
  UtilsAPI.closeContentAreaContextMenu(controller);
}

var testAccessPageInfo = function ()
{
  // List of all available panes inside the page info window
  var panes = [
               {button: 'generalTab', panel: 'generalPanel'},
               {button: 'mediaTab', panel: 'mediaPanel'},
               {button: 'feedTab', panel: 'feedListbox'},
               {button: 'permTab', panel: 'permPanel'},
               {button: 'securityTab', panel: 'securityPanel'}
              ];

  // Load web page with RSS feed
  controller.open('http://www.cnn.com');
  controller.waitForPageLoad();

  // Open context menu on the html element and select Page Info entry
  controller.rightclick(new elementslib.XPath(controller.tabs.activeTab, "/html"));
  controller.sleep(gDelay);
  controller.click(new elementslib.ID(controller.window.document, "context-viewinfo"));
  controller.sleep(500);

  // Check if the Page Info window has been opened
  var window = mozmill.wm.getMostRecentWindow('Browser:page-info');
  var piController = new mozmill.controller.MozMillController(window);

  // Step through each of the tabs
  for each (pane in panes) {
    piController.click(new elementslib.ID(piController.window.document, pane.button));
    piController.sleep(gDelay);

    // Check if the panel has been shown
    var node = new elementslib.ID(piController.window.document, pane.panel);
    piController.waitForElement(node, gTimeout);
  }

  // Close the Page Info window by pressing Escape
  piController.keypress(null, 'VK_ESCAPE', {});

  // Wait a bit to make sure the page info window has been closed
  controller.sleep(200);
}

/**
 * Map test functions to litmus tests
 */
// testAccessPageInfo.meta = {litmusids : [8413]};
