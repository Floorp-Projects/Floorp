/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
