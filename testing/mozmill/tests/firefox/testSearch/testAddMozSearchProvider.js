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
var MODULE_REQUIRES = ['ModalDialogAPI', 'SearchAPI', 'UtilsAPI'];

const gDelay = 0;
const gTimeout = 5000;

const searchEngine = {name: "MDC",
                      url : "https://litmus.mozilla.org/testcase_files/firefox/search/mozsearch.html"};

var setupModule = function(module)
{
  controller = mozmill.getBrowserController();

  search = new SearchAPI.searchBar(controller);
}

var teardownModule = function(module)
{
  search.removeEngine(searchEngine.name);
  search.restoreDefaultEngines();
}

/**
 * Add a MozSearch Search plugin
 */
var testAddMozSearchPlugin = function()
{
  // Open the web page with the test MozSearch plugin
  controller.open(searchEngine.url);
  controller.waitForPageLoad();

  // Create a modal dialog instance to handle the installation dialog
  var md = new ModalDialogAPI.modalDialog(handleSearchInstall);
  md.start();

  // Add the search engine
  var addButton = new elementslib.Name(controller.tabs.activeTab, "add");
  controller.click(addButton);

  controller.waitForEval("subject.search.isEngineInstalled(subject.engine) == true", gTimeout, 100,
                         {search: search, engine: searchEngine.name});

  // The engine should not be selected by default
  controller.assertJS("subject.newEngineNotSelected == true",
                      {newEngineNotSelected: search.selectedEngine != searchEngine.name});

  // Select search engine and start a search
  search.selectedEngine = searchEngine.name;
  search.search({text: "Firefox", action: "goButton"});
}

/**
 * Handle the modal security dialog when installing a new search engine
 *
 * @param {MozMillController} controller
 *        MozMillController of the browser window to operate on
 */
var handleSearchInstall = function(controller)
{
  // Installation successful?
  var confirmTitle = UtilsAPI.getProperty("chrome://global/locale/search/search.properties",
                                          "addEngineConfirmTitle");

  if (mozmill.isMac)
    var title = controller.window.document.getElementById("info.title").textContent;
  else
    var title = controller.window.document.title;

  controller.assertJS("subject.windowTitle == subject.addEngineTitle",
                      {windowTitle: title, addEngineTitle: confirmTitle});

  // Check that litmus.mozilla.org is shown as domain
  var infoBody = controller.window.document.getElementById("info.body");
  controller.waitForEval("subject.textContent.indexOf('litmus.mozilla.org') != -1",
                         gTimeout, 100, infoBody);

  var addButton = new elementslib.Lookup(controller.window.document,
                                         '/id("commonDialog")/anon({"anonid":"buttons"})/{"dlgtype":"accept"}');
  controller.click(addButton);
}

/**
 * Map test functions to litmus tests
 */
// testAddMozSearchPlugin.meta = {litmusids : [8235]};
