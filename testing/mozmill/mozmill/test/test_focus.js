/* * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
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
 * **** END LICENSE BLOCK ***** */

var gDelay = 500;

var setupModule = function(module) {
  controller = mozmill.getBrowserController();
}

var checkKeypressFunction = function(element) {
  element.getNode().value = "";

  // controller.keypress should not focus element when window is given as element
  controller.keypress(null, "F", {});
  controller.sleep(gDelay);
  controller.assertValue(element, "");

  // controller.keypress should focus the element when the element itself is given as parameter
  controller.keypress(element, "M", {});
  controller.sleep(gDelay);
  controller.assertValue(element, "M");

  // controller.keypress should not clear formerly entered text
  controller.keypress(element, "F", {});
  controller.sleep(gDelay);
  controller.assertValue(element, "MF");
}

var checkTypeFunction = function(element) {
  element.getNode().value = "";

  // controller.type should not focus element when window is given as element
  controller.type(null, "Firefox");
  controller.sleep(gDelay);
  controller.assertValue(element, "");

  // controller.type should focus the element when the element itself is given as parameter
  controller.type(element, "Mozilla");
  controller.sleep(gDelay);
  controller.assertValue(element, "Mozilla");

  // controller.type should not clear formerly entered text
  controller.type(element, " Firefox");
  controller.sleep(gDelay);
  controller.assertValue(element, "Mozilla Firefox");
}

var testContentTextboxFocus = function() {
  controller.open("http://www.mozilla.org");
  controller.waitForPageLoad(controller.tabs.activeTab);

  var searchField = new elementslib.ID(controller.tabs.activeTab, "q");
  controller.waitForElement(searchField, 5000);
  controller.sleep(gDelay);

  checkKeypressFunction(searchField);
  checkTypeFunction(searchField);
}

var testChromeTextboxFocus = function() {
  var searchBar = new elementslib.ID(controller.window.document, "searchbar");

  checkKeypressFunction(searchBar);

  // Move focus to the location bar to blur the search bar
  controller.keypress(null, "l", {accelKey: true});
  checkTypeFunction(searchBar);
}