/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  David Drinan <ddrinan@netscape.com>
 */

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

