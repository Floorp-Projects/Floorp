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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Anthony Hughes <anthony.s.hughes@gmail.com>
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

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
}

/**
 * Test what displays in the status bar for an SSL secured domain
 */
var testSSLDomainLabelInStatusBar = function()
{
  // Go to a secure website
  controller.open("https://addons.mozilla.org");
  controller.waitForPageLoad();
  
  // Check for the Padlock icon in the status bar
  var padlockIcon = new elementslib.ID(controller.window.document, 
                                       "security-button");
  controller.assertNode(padlockIcon);                                      

  // Check the domain name does NOT display in the status bar
  var lookupPath = '/id("main-window")' +
                   '/id("browser-bottombox")' +
                   '/id("status-bar")' +
                   '/id("security-button")' +
                   '/anon({"class":"statusbarpanel-text"})';
  var domainText = new elementslib.Lookup(controller.window.document, lookupPath);
  controller.assertNodeNotExist(domainText);
}

/**
 * Map test functions to litmus tests
 */
// testSSLDomainLabelInStatusBar.meta = {litmusids : [9328]};
