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
var certs = [];
var helpUrl;
var gParams;

function setWindowName()
{
  gParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  
  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  
  var typeFlag = gParams.GetString(0);
  var numberOfCerts = gParams.GetInt(0);
  var dbkey;
  for(var x=0; x<numberOfCerts;x++)
  {
     dbkey = gParams.GetString(x+1);
     certs[x] = certdb.findCertByDBKey(dbkey , null);
  }
  
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var title;
  var confirm;
  var impact;
  
  if(typeFlag == bundle.GetStringFromName("deleteUserCertFlag"))
  {
     title = bundle.GetStringFromName("deleteUserCertTitle");
     confirm = bundle.GetStringFromName("deleteUserCertConfirm");
     impact = bundle.GetStringFromName("deleteUserCertImpact");
     helpUrl = "delete_my_certs"
  }
  else if(typeFlag == bundle.GetStringFromName("deleteSslCertFlag"))
  {
     title = bundle.GetStringFromName("deleteSslCertTitle");
     confirm = bundle.GetStringFromName("deleteSslCertConfirm");
     impact = bundle.GetStringFromName("deleteSslCertImpact");
     helpUrl = "delete_web_certs"
  }
  else if(typeFlag == bundle.GetStringFromName("deleteCaCertFlag"))
  {
     title = bundle.GetStringFromName("deleteCaCertTitle");
     confirm = bundle.GetStringFromName("deleteCaCertConfirm");
     impact = bundle.GetStringFromName("deleteCaCertImpact");
     helpUrl = "delete_ca_certs"   
  }
  else if(typeFlag == bundle.GetStringFromName("deleteEmailCertFlag"))
  {
     title = bundle.GetStringFromName("deleteEmailCertTitle");
     confirm = bundle.GetStringFromName("deleteEmailCertConfirm");
     impact = bundle.GetStringFromName("deleteEmailCertImpact");
     helpUrl = "delete_email_certs"
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
  for(x=0;x<certs.length;x++)
  {
    if (!certs[x])
      continue;
    text = document.createElement("text");
    text.setAttribute("value",certs[x].commonName);
    box.appendChild(text);
  }

  setText("impact",impact);
  window.sizeToContent();
}

function doOK()
{
  // On returning our param list will contain keys of those certs that were deleted.
  // It will contain empty strings for those certs that are still alive.

  for(var i=0;i<certs.length;i++)
  {
    if (certs[i]) {
      try {
        certdb.deleteCertificate(certs[i]);
      }
      catch (e) {
        gParams.SetString(i+1, "");
      }
      certs[i] = null;
    }
  }
  gParams.SetInt(1, 1); // means OK
  window.close();
}

function doCancel()
{
  var numberOfCerts = gParams.GetInt(0);
  for(var x=0; x<numberOfCerts;x++)
  {
     gParams.SetString(x+1, "");
  }
  gParams.SetInt(1, 0); // means CANCEL
  window.close();
}

function doHelp()
{
   openHelp(helpUrl);
}
