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

  doEnabling();
}

function doEnabling()
{
  var signersMenu = document.getElementById("signingCA");
  var signersURL = document.getElementById("serviceURL");
  var radiogroup = document.getElementById("securityOCSPEnabled");
  
  switch ( radiogroup.value ) {
   case "0":
   case "1":
    signersMenu.setAttribute("disabled", true);
    signersURL.setAttribute("disabled", true);
    break;
   case "2":
   default:
    signersMenu.removeAttribute("disabled");
    signersURL.removeAttribute("disabled");
  }
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
                'chrome,centerscreen,dialog,resizable');
}

