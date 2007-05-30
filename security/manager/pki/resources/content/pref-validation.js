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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Drinan <ddrinan@netscape.com>
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

const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIOCSPResponder = Components.interfaces.nsIOCSPResponder;
const nsISupportsArray = Components.interfaces.nsISupportsArray;

var certdb;
var ocspResponders;
var cacheRadio = 0;

function onLoad()
{
  var ocspEntry;
  var i;

  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  ocspResponders = certdb.getOCSPResponders();

  var signersMenu = document.getElementById("signingCA");
  var signersURL = document.getElementById("serviceURL");
  for (i=0; i<ocspResponders.length; i++) {
    ocspEntry = ocspResponders.queryElementAt(i, nsIOCSPResponder);
    var menuItemNode = document.createElement("menuitem");
    menuItemNode.setAttribute("value", ocspEntry.responseSigner);
    menuItemNode.setAttribute("label", ocspEntry.responseSigner);
    signersMenu.firstChild.appendChild(menuItemNode);
  }

  parent.initPanel('chrome://pippki/content/pref-validation.xul');

  doEnabling(0);
}

function doEnabling(called_by)
{
  var signingCA = document.getElementById("signingCA");
  var serviceURL = document.getElementById("serviceURL");
  var securityOCSPEnabled = document.getElementById("securityOCSPEnabled");
  var requireWorkingOCSP = document.getElementById("requireWorkingOCSP");
  var enableOCSPBox = document.getElementById("enableOCSPBox");
  var certOCSP = document.getElementById("certOCSP");
  var proxyOCSP = document.getElementById("proxyOCSP");

  var OCSPPrefValue = parseInt(securityOCSPEnabled.value);

  if (called_by == 0) {
    // the radio button changed, or we init the stored value from prefs
    enableOCSPBox.checked = (OCSPPrefValue != 0);
  }
  else {
    // the user toggled the checkbox to enable/disable OCSP
    var new_val = 0;
    if (enableOCSPBox.checked) {
      // now enabled. if we have a cached radio val, restore it.
      // if not, use the first setting
      new_val = (cacheRadio > 0) ? cacheRadio : 1;
    }
    else {
      // now disabled. remember current value
      cacheRadio = OCSPPrefValue;
    }
    securityOCSPEnabled.value = OCSPPrefValue = new_val;
  }

  certOCSP.disabled = (OCSPPrefValue == 0);
  proxyOCSP.disabled = (OCSPPrefValue == 0);
  signingCA.disabled = serviceURL.disabled = OCSPPrefValue == 0 || OCSPPrefValue == 1;
  requireWorkingOCSP.disabled = (OCSPPrefValue == 0);
}

function changeURL()
{
  var signersMenu = document.getElementById("signingCA");
  var signersURL = document.getElementById("serviceURL");
  var CA = signersMenu.getAttribute("value");
  var i;
  var ocspEntry;

  for (i=0; i < ocspResponders.length; i++) {
    ocspEntry = ocspResponders.queryElementAt(i, nsIOCSPResponder);
    if (CA == ocspEntry.responseSigner) {
      signersURL.setAttribute("value", ocspEntry.serviceURL);
      break;
    }
  }
}

function openCrlManager()
{
    window.open('chrome://pippki/content/crlManager.xul',  "",
                'chrome,centerscreen,resizable');
}

