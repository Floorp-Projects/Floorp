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
 *  Ian McGreer <mcgreer@netscape.com>
 */

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsIPKIParamBlock = Components.interfaces.nsIPKIParamBlock;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var certdb;
var certs = [];
var helpUrl;

function setWindowName()
{
  var params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  
  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  
  var typeFlag = params.GetString(1);
  var numberOfCerts = params.GetInt(2);
  var dbkey;
  for(var x=0; x<numberOfCerts;x++)
  {
     dbkey = params.GetString(x+3);
     certs[x] = certdb.getCertByDBKey(dbkey , null);
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
     helpUrl = "chrome://help/content/help.xul?delete_my_certs"
  }
  else if(typeFlag == bundle.GetStringFromName("deleteSslCertFlag"))
  {
     title = bundle.GetStringFromName("deleteSslCertTitle");
     confirm = bundle.GetStringFromName("deleteSslCertConfirm");
     impact = bundle.GetStringFromName("deleteSslCertImpact");
     helpUrl = "chrome://help/content/help.xul?delete_web_certs"
  }
  else if(typeFlag == bundle.GetStringFromName("deleteCaCertFlag"))
  {
     title = bundle.GetStringFromName("deleteCaCertTitle");
     confirm = bundle.GetStringFromName("deleteCaCertConfirm");
     impact = bundle.GetStringFromName("deleteCaCertImpact");
     helpUrl = "chrome://help/content/help.xul?delete_ca_certs"   
  }
  else if(typeFlag == bundle.GetStringFromName("deleteEmailCertFlag"))
  {
     title = bundle.GetStringFromName("deleteEmailCertTitle");
     confirm = bundle.GetStringFromName("deleteEmailCertConfirm");
     impact = bundle.GetStringFromName("deleteEmailCertImpact");
     helpUrl = "chrome://help/content/help.xul?delete_email_certs"
  }
  else
  {
     return;
  }
  var windowReference = document.getElementById('deleteCertificate');
  var confirReference = document.getElementById('confirm');
  var impactReference = document.getElementById('impact');
  windowReference.setAttribute("title", title);
  
  setText("confirm",confirm);

  var box=document.getElementById("certlist");
  var text;
  for(var x=0;x<certs.length;x++)
  {
    text = document.createElement("text");
    text.setAttribute("value",certs[x].commonName);
    box.appendChild(text);
  }

  setText("impact",impact);

  var wdth = window.innerWidth; // THIS IS NEEDED,
  window.sizeToContent();
  windowReference.setAttribute("width",window.innerWidth + 30);
  var hght = window.innerHeight; // THIS IS NEEDED,
  window.sizeToContent();
  windowReference.setAttribute("height",window.innerHeight + 40);

  
}

function doOK()
{
  for(var i=0;i<certs.length;i++)
  {
    certdb.deleteCertificate(certs[i]);
  }
  window.close();
}

function doHelp()
{
   openHelp(helpUrl);
}
