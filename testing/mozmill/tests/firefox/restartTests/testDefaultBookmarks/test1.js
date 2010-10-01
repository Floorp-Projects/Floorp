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
 *   Geo Mealer <gmealer@mozilla.com>
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
var MODULE_REQUIRES = ['ModalDialogAPI', 'PlacesAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
  module.bs = PlacesAPI.bookmarksService;
  module.hs = PlacesAPI.historyService;
  module.ls = PlacesAPI.livemarkService;
}

var testVerifyDefaultBookmarks = function() {
  var toolbarElemString = "/*[name()='window']/*[name()='deck'][1]" +
                          "/*[name()='vbox'][1]/*[name()='toolbox'][1]" +
                          "/*[name()='toolbar'][3]";
  var elemString = toolbarElemString + "/*[name()='toolbaritem'][1]" +
                   "/*[name()='hbox'][1]/*[name()='hbox'][1]" +
                   "/*[name()='scrollbox'][1]/*[name()='toolbarbutton'][%1]";

  // Default bookmarks toolbar should be closed
  var toolbar = new elementslib.XPath(controller.window.document, toolbarElemString);
  controller.assertProperty(toolbar, "collapsed", true);

  // Open the bookmarks toolbar via bookmarks button for the rest of the test
  var bookmarksButton = new elementslib.ID(controller.window.document, "bookmarks-menu-button");
  controller.click(bookmarksButton);
  
  var bookmarkBarItem = new elementslib.ID(controller.window.document, "BMB_viewBookmarksToolbar");
  controller.mouseDown(bookmarkBarItem);
  controller.mouseUp(bookmarkBarItem);
  
  // Make sure bookmarks toolbar is now open
  controller.waitFor(function() {
    return toolbar.getNode().collapsed == false;
  }, gTimeout, 100, 'Bookmarks toolbar is open' );

  // Get list of items on the bookmarks toolbar and open container
  var toolbarNodes = getBookmarkToolbarItems();
  toolbarNodes.containerOpen = true;

  // For a default profile there should be exactly 3 items
  controller.assertJS("subject.toolbarItemCount == 3",
                      {toolbarItemCount: toolbarNodes.childCount});

  // Check if the Most Visited folder is visible and has the correct title
  var mostVisited = new elementslib.XPath(controller.window.document,
                                          elemString.replace("%1", "1"));
  controller.assertProperty(mostVisited, "label", toolbarNodes.getChild(0).title);

  // Check Getting Started bookmarks title and URI
  var gettingStarted = new elementslib.XPath(controller.window.document,
                                             elemString.replace("%1", "2"));
  controller.assertProperty(gettingStarted, "label", toolbarNodes.getChild(1).title);

  var locationBar = new elementslib.ID(controller.window.document, "urlbar");
  controller.click(gettingStarted);
  controller.waitForPageLoad();

  // Check for the correct path in the URL which also includes the locale
  var uriSource = UtilsAPI.createURI(toolbarNodes.getChild(1).uri, null, null);
  var uriTarget = UtilsAPI.createURI(locationBar.getNode().value, null, null);
  controller.assertJS("subject.source.path == subject.target.path",
                      {source: uriSource, target: uriTarget});

  // Check the title of the default RSS feed toolbar button
  var RSS = new elementslib.XPath(controller.window.document, elemString.replace("%1", "3"));
  controller.assertProperty(RSS, "label", toolbarNodes.getChild(2).title);

  // Close container again
  toolbarNodes.containerOpen = false;

  // Create modal dialog observer
  var md = new ModalDialogAPI.modalDialog(feedHandler);
  md.start();

  // Open the properties dialog of the feed
  controller.rightClick(RSS);
  controller.sleep(100);
  controller.click(new elementslib.ID(controller.window.document, "placesContext_show:info"));
}

/**
 * Callback handler for modal bookmark properties dialog
 *
 * @param controller {MozMillController} Controller of the modal dialog
 */
function feedHandler(controller) {
  try {
    // Get list of items on the bookmarks toolbar and open container
    var toolbarNodes = getBookmarkToolbarItems();
    toolbarNodes.containerOpen = true;
    var child = toolbarNodes.getChild(2);

    // Check if the child is a Livemark
	controller.assertJS("subject.isLivemark == true",
	                    {isLivemark: ls.isLivemark(child.itemId)});

    // Compare the site and feed URI's
    var siteLocation = new elementslib.ID(controller.window.document, "editBMPanel_siteLocationField");
    controller.assertValue(siteLocation, ls.getSiteURI(child.itemId).spec);

    var feedLocation = new elementslib.ID(controller.window.document, "editBMPanel_feedLocationField");
    controller.assertValue(feedLocation, ls.getFeedURI(child.itemId).spec);

    // Close container again
    toolbarNodes.containerOpen = false;
  } catch(e) {
  }

  controller.keypress(null, "VK_ESCAPE", {});
}

/**
 * Get the Bookmarks Toolbar items
 */
function getBookmarkToolbarItems() {
  var options = hs.getNewQueryOptions();
  var query = hs.getNewQuery();

  query.setFolders([bs.toolbarFolder], 1);

  return hs.executeQuery(query, options).root;
}

/**
 * Map test functions to litmus tests
 */
// testVerifyDefaultBookmarks.meta = {litmusids : [8751]};
