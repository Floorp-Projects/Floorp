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

var gTimeout = 5000;

var setupModule = function(module)
{
  module.controller = mozmill.getBrowserController();

  // Effective TLD Service for grabbing certificate info
  module.gETLDService = Cc["@mozilla.org/network/effective-tld-service;1"]
                           .getService(Ci.nsIEffectiveTLDService);
}

/**
 * Test the Larry displays GREEN
 */
var testLarryGreen = function()
{
  // Go to a "green" website
  controller.open("https://www.verisign.com/");
  controller.waitForPageLoad();

  // Get the information from the certificate for comparison
  var securityUI = controller.window.getBrowser().mCurrentBrowser.securityUI;
  var cert = securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus.serverCert;

  // Check the label displays
  // Format: Organization (CountryCode)
  var identLabel = new elementslib.ID(controller.window.document, "identity-icon-label");
  var country = cert.subjectName.substring(cert.subjectName.indexOf("C=")+2,
                                           cert.subjectName.indexOf(",serialNumber="));
  var certIdent = cert.organization + ' (' + country + ')';
  controller.assertValue(identLabel, certIdent);

  // Check the favicon
  var favicon = new elementslib.ID(controller.window.document, "page-proxy-favicon");
  controller.assertProperty(favicon, "src" ,"https://www.verisign.com/favicon.ico");

  // Check the identity box shows green
  var identityBox = new elementslib.ID(controller.window.document, "identity-box");
  controller.assertProperty(identityBox, "className", "verifiedIdentity");

  // Click the identity button to display Larry
  controller.click(identityBox);

  // Make sure the doorhanger is "open" before continuing
  var doorhanger = new elementslib.ID(controller.window.document, "identity-popup");
  controller.waitForEval("subject.state == 'open'", 2000, 100, doorhanger.getNode());

  // Check that the Larry UI is verified (aka Green)
  controller.assertProperty(doorhanger, "className", "verifiedIdentity");

  // Check for the Lock icon is visible
  var lockIcon = new elementslib.ID(controller.window.document, "identity-popup-encryption-icon");
  var cssInfoLockImage = controller.window.getComputedStyle(lockIcon.getNode(), "");
  controller.assertJS("subject.getPropertyValue('list-style-image') != 'none'", cssInfoLockImage);

  // Check the site identifier string against the Cert
  // XXX: Larry strips the 'www.' from the CName using the eTLDService
  //      This is expected behaviour for the time being (bug 443116)
  var host = new elementslib.ID(controller.window.document, "identity-popup-content-host");
  controller.assertProperty(host, "textContent", gETLDService.getBaseDomainFromHost(cert.commonName));

  // Check the owner string against the Cert
  var owner = new elementslib.ID(controller.window.document, "identity-popup-content-owner");
  controller.assertProperty(owner, "textContent", cert.organization);

  // Check the owner location string against the Cert
  // Format: City
  //         State, Country Code
  var city = cert.subjectName.substring(cert.subjectName.indexOf("L=")+2,
                                        cert.subjectName.indexOf(",ST="));
  var state = cert.subjectName.substring(cert.subjectName.indexOf("ST=")+3,
                                         cert.subjectName.indexOf(",postalCode="));
  var country = cert.subjectName.substring(cert.subjectName.indexOf("C=")+2,
                                           cert.subjectName.indexOf(",serialNumber="));
  var location = city + '\n' + state + ', ' + country;
  var ownerLocation = new elementslib.ID(controller.window.document,
                                         "identity-popup-content-supplemental");
  controller.assertProperty(ownerLocation, "textContent", location);

  // Check the "Verified by: %S" string
  var l10nVerifierLabel = UtilsAPI.getProperty("chrome://browser/locale/browser.properties",
                                               "identity.identified.verifier");
  l10nVerifierLabel = l10nVerifierLabel.replace("%S", cert.issuerOrganization);
  var verifier = new elementslib.ID(controller.window.document,
                                    "identity-popup-content-verifier");
  controller.assertProperty(verifier, "textContent", l10nVerifierLabel);

  // Check the Encryption Label text
  var l10nEncryptionLabel = UtilsAPI.getProperty("chrome://browser/locale/browser.properties",
                                                 "identity.encrypted");
  var label = new elementslib.ID(controller.window.document,
                                 "identity-popup-encryption-label");
  controller.assertProperty(label, "textContent", l10nEncryptionLabel);

  // Check the More Information button
  var moreInfoButton = new elementslib.ID(controller.window.document,
                                          "identity-popup-more-info-button");
  controller.click(moreInfoButton);
  controller.sleep(500);

  // Check the Page Info - Security Tab
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
  var webIDOwnerLabel = new elementslib.ID(pageInfoController.window.document,
                                           "security-identity-owner-value");
  pageInfoController.assertValue(webIDOwnerLabel, cert.organization);

  // Check the Verifier label against the Cert Issuer
  var webIDVerifierLabel = new elementslib.ID(pageInfoController.window.document,
                                              "security-identity-verifier-value");
  pageInfoController.assertValue(webIDVerifierLabel, cert.issuerOrganization);

  // Press ESC to close the Page Info dialog
  pageInfoController.keypress(null, 'VK_ESCAPE', {});

  // Wait a bit to make sure the page info window has been closed
  controller.sleep(200);
}

/**
 * Map test functions to litmus tests
 */
// testLarryGreen.meta = {litmusids : [8805]};
