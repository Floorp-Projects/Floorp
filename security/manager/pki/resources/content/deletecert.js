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
 *   Ian McGreer <mcgreer@netscape.com>
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

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsIPKIParamBlock = Components.interfaces.nsIPKIParamBlock;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var certdb;
var gParams;

function setWindowName()
{
  gParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  
  var typeFlag = gParams.GetString(0);
  var numberOfCerts = gParams.GetInt(0);
  
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var title;
  var confirm;
  var impact;
  
  if(typeFlag == "mine_tab")
  {
     title = bundle.GetStringFromName("deleteUserCertTitle");
     confirm = bundle.GetStringFromName("deleteUserCertConfirm");
     impact = bundle.GetStringFromName("deleteUserCertImpact");
  }
  else if(typeFlag == "websites_tab")
  {
     title = bundle.GetStringFromName("deleteSslCertTitle3");
     confirm = bundle.GetStringFromName("deleteSslCertConfirm3");
     impact = bundle.GetStringFromName("deleteSslCertImpact3");
  }
  else if(typeFlag == "ca_tab")
  {
     title = bundle.GetStringFromName("deleteCaCertTitle2");
     confirm = bundle.GetStringFromName("deleteCaCertConfirm2");
     impact = bundle.GetStringFromName("deleteCaCertImpactX2");
  }
  else if(typeFlag == "others_tab")
  {
     title = bundle.GetStringFromName("deleteEmailCertTitle");
     confirm = bundle.GetStringFromName("deleteEmailCertConfirm");
     impact = bundle.GetStringFromName("deleteEmailCertImpactDesc");
  }
  else if(typeFlag == "orphan_tab")
  {
     title = bundle.GetStringFromName("deleteOrphanCertTitle");
     confirm = bundle.GetStringFromName("deleteOrphanCertConfirm");
     impact = "";
  }
  else
  {
     return;
  }
  var confirReference = document.getElementById('confirm');
  var impactReference = document.getElementById('impact');
  document.title = title;
  
  setText("confirm",confirm);

  var box=document.getElementById("certlist");
  var text;
  for(var x=0;x<numberOfCerts;x++)
  {
    text = document.createElement("text");
    text.setAttribute("value", gParams.GetString(x+1));
    box.appendChild(text);
  }

  setText("impact",impact);
}

function doOK()
{
  gParams.SetInt(1, 1); // means OK
  return true;
}

function doCancel()
{
  gParams.SetInt(1, 0); // means CANCEL
  return true;
}
