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
 *   Anthony Hughes <ahughes@mozilla.com>
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Tobias Markus <tobbi.bugs@googlemail.com>
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

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  controller = mozmill.getBrowserController();

  containerString = '/id("main-window")/id("browser-bottombox")/id("FindToolbar")' +
                    '/anon({"anonid":"findbar-container"})';
  findBar = new elementslib.Lookup(controller.window.document, containerString);
  findBarTextField = new elementslib.Lookup(controller.window.document,
                                            containerString + '/anon({"anonid":"findbar-textbox"})');
  findBarNextButton = new elementslib.Lookup(controller.window.document,
                                             containerString + '/anon({"anonid":"find-next"})');
  findBarPrevButton = new elementslib.Lookup(controller.window.document,
                                             containerString + '/anon({"anonid":"find-previous"})');
  findBarCloseButton = new elementslib.Lookup(controller.window.document,
                                              containerString + '/anon({"anonid":"find-closebutton"})');
}

var teardownModule = function(module) {
  try {
     // Just press Ctrl/Cmd + F to select the whole search string
    controller.keypress(null, "f", {accelKey:true});

    // Clear search text from the text field
    controller.keypress(findBarTextField, 'VK_DELETE', {});

    // Make sure the find bar is closed by click the X button
    controller.click(findBarCloseButton);
  } catch(e) {
  }
}

/**
 * Test find in page functionality
 *
 */
var testFindInPage = function() {
  var searchTerm = "mozilla";
  var comparator = Ci.nsIDOMRange.START_TO_START;
  var tabContent = controller.tabs.activeTabWindow;

  // Open a website
  controller.open("http://www.mozilla.org");
  controller.waitForPageLoad();

  // Press Ctrl/Cmd + F to open the find bar
  controller.keypress(null, "f", {accelKey:true});
  controller.sleep(gDelay);

  // Check that the find bar is visible
  controller.waitForElement(findBar, gTimeout);

  // Type "mozilla" into the find bar text field and press return to start the search
  controller.type(findBarTextField, searchTerm);
  controller.keypress(null, "VK_RETURN", {});
  controller.sleep(gDelay);

  // Check that some text on the page has been highlighted
  // (Lower case because we aren't checking for Match Case option)
  var selectedText = tabContent.getSelection();
  controller.assertJS("subject.selectedText == subject.searchTerm",
                      {selectedText: selectedText.toString().toLowerCase(), searchTerm: searchTerm});

  // Remember DOM range of first search result
  var range = selectedText.getRangeAt(0);

  // Click the next button and check the strings again
  controller.click(findBarNextButton);
  controller.sleep(gDelay);

  selectedText = tabContent.getSelection();
  controller.assertJS("subject.selectedText == subject.searchTerm",
                      {selectedText: selectedText.toString().toLowerCase(), searchTerm: searchTerm});

  // Check that the next result has been selected
  controller.assertJS("subject.isNextResult == true",
                      {isNextResult: selectedText.getRangeAt(0).compareBoundaryPoints(comparator, range) != 0});

  // Click the prev button and check the strings again
  controller.click(findBarPrevButton);
  controller.sleep(gDelay);

  selectedText = tabContent.getSelection();
  controller.assertJS("subject.selectedText == subject.searchTerm",
                      {selectedText: selectedText.toString().toLowerCase(), searchTerm: searchTerm});

  // Check that the first result has been selected again
  controller.assertJS("subject.isFirstResult == true",
                      {isFirstResult: selectedText.getRangeAt(0).compareBoundaryPoints(comparator, range) == 0});
}

/**
 * Map test functions to litmus tests
 */
// testFindInPage.meta = {litmusids : [7970]};
