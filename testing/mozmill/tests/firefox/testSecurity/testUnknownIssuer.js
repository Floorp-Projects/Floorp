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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

const gDelay = 0;
const gTimeout = 5000;

var setupModule = function(module) {
  module.controller = mozmill.getBrowserController();
}

/**
 * Test to see if the Unknown Issuer error page appears
 *
 */
var testUnknownIssuer = function() {
  // Go to a website with an unknown cert issuer
  controller.open('https://mur.at');
  controller.waitForPageLoad(1000);

  // Verify the link in Technical Details is correct
  var link = new elementslib.ID(controller.tabs.activeTab, "cert_domain_link");
  controller.waitForElement(link, gTimeout);
  controller.assertProperty(link, "textContent", "secure.mur.at");

  // Verify "Get Me Out Of Here!" button appears
  controller.assertNode(new elementslib.ID(controller.tabs.activeTab, "getMeOutOfHereButton"));

  // Verify "Add Exception" button appears
  controller.assertNode(new elementslib.ID(controller.tabs.activeTab, "exceptionDialogButton"));

  // Verify the error code is correct
  var text = new elementslib.ID(controller.tabs.activeTab, "technicalContentText");
  controller.waitForElement(text, gTimeout);
  controller.assertJS("subject.errorMessage.indexOf('sec_error_unknown_issuer') != -1",
                      {errorMessage: text.getNode().textContent});
}

/**
 * Map test functions to litmus tests
 */
// testUnknownIssuer.meta = {litmusids : [8900]};
