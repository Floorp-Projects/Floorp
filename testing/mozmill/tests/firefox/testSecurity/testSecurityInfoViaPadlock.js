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

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();
}

/**
 * Test clicking the padlock in the statusbar opens the Page Info dialog
 * to the Security tab
 */
var testSecurityInfoViaPadlock = function()
{
  // Go to a secure website
  controller.open("https://www.verisign.com/");
  controller.waitForPageLoad();

  // Get the information from the certificate for comparison
  var secUI = controller.window.getBrowser().mCurrentBrowser.securityUI;
  var cert = secUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus.serverCert;

  // Click the padlock icon
  controller.click(new elementslib.ID(controller.window.document, "security-button"));

  // Check the Page Info - Security Tab
  controller.sleep(500);
  var window = mozmill.wm.getMostRecentWindow('Browser:page-info');
  var pageInfoController = new mozmill.controller.MozMillController(window);

  // Check that the Security tab is selected by default
  var securityTab = new elementslib.ID(pageInfoController.window.document, "securityTab");
  pageInfoController.assertProperty(securityTab, "selected", "true");

  // Check the Web Site label against the Cert CName
  var webIDDomainLabel = new elementslib.ID(pageInfoController.window.document, "security-identity-domain-value");
  pageInfoController.waitForEval("subject.domainLabel.indexOf(subject.CName) != -1", gTimeout, 100,
                                 {domainLabel: webIDDomainLabel.getNode().value, CName: cert.commonName});


  // Check the Owner label against the Cert Owner
  var webIDOwnerLabel = new elementslib.ID(pageInfoController.window.document, "security-identity-owner-value");
  pageInfoController.assertValue(webIDOwnerLabel, cert.organization);

  // Check the Verifier label against the Cert Issuer
  var webIDVerifierLabel = new elementslib.ID(pageInfoController.window.document, "security-identity-verifier-value");
  pageInfoController.assertValue(webIDVerifierLabel, cert.issuerOrganization);

  // Press ESC to close the Page Info dialog
  pageInfoController.keypress(null, 'VK_ESCAPE', {});

  // Wait a bit to make sure the page info window has been closed
  controller.sleep(200);
}

/**
 * Map test functions to litmus tests
 */
// testSecurityInfoViaPadlock.meta = {litmusids : [8205]};
