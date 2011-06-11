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
 *   Bob Lord <lord@netscape.com>
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

var certdb;
var cert;

function doPrompt(msg)
{
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  prompts.alert(window, null, msg);
}

function setWindowName()
{
  var dbkey = self.name;

  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  //var pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  //var isupport = pkiParams.getISupportAtIndex(1);
  //cert = isupport.QueryInterface(nsIX509Cert);
  cert = certdb.findCertByDBKey(dbkey, null);

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var windowReference = document.getElementById('editCaCert');

  var message1 = bundle.formatStringFromName("editTrustCA",
                                             [ cert.commonName ],
                                             1);
  setText("certmsg", message1);

  var ssl = document.getElementById("trustSSL");
  if (certdb.isCertTrusted(cert, nsIX509Cert.CA_CERT, 
                          nsIX509CertDB.TRUSTED_SSL)) {
    ssl.setAttribute("checked", "true");
  } else {
    ssl.setAttribute("checked", "false");
  }
  var email = document.getElementById("trustEmail");
  if (certdb.isCertTrusted(cert, nsIX509Cert.CA_CERT, 
                          nsIX509CertDB.TRUSTED_EMAIL)) {
    email.setAttribute("checked", "true");
  } else {
    email.setAttribute("checked", "false");
  }
  var objsign = document.getElementById("trustObjSign");
  if (certdb.isCertTrusted(cert, nsIX509Cert.CA_CERT,  
                          nsIX509CertDB.TRUSTED_OBJSIGN)) {
    objsign.setAttribute("checked", "true");
  } else {
    objsign.setAttribute("checked", "false");
  }
}

function doOK()
{
  var ssl = document.getElementById("trustSSL");
  var email = document.getElementById("trustEmail");
  var objsign = document.getElementById("trustObjSign");
  var trustssl = (ssl.checked) ? nsIX509CertDB.TRUSTED_SSL : 0;
  var trustemail = (email.checked) ? nsIX509CertDB.TRUSTED_EMAIL : 0;
  var trustobjsign = (objsign.checked) ? nsIX509CertDB.TRUSTED_OBJSIGN : 0;
  //
  //  Set the cert trust
  //
  certdb.setCertTrust(cert, nsIX509Cert.CA_CERT, 
                      trustssl | trustemail | trustobjsign);
  return true;
}

function doLoadForSSLCert()
{
  var dbkey = self.name;

  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  cert = certdb.findCertByDBKey(dbkey, null);

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var windowReference = document.getElementById('editWebsiteCert');

  var message1 = bundle.formatStringFromName("editTrustSSL",
                                             [ cert.commonName ],
                                             1);
  setText("certmsg", message1);

  setText("issuer", cert.issuerName);

  var cacert = getCaCertForEntityCert(cert);
  if(cacert == null)
  {
     setText("explanations", bundle.GetStringFromName("issuerNotKnown"));
  }
  else if(certdb.isCertTrusted(cacert, nsIX509Cert.CA_CERT,
                                                nsIX509CertDB.TRUSTED_SSL))
  {
     setText("explanations", bundle.GetStringFromName("issuerTrusted"));
  }
  else
  {
     setText("explanations", bundle.GetStringFromName("issuerNotTrusted"));
  }
/*
  if(cacert == null)
  {
     var editButton = document.getElementById('editca-button');
	 editButton.setAttribute("disabled","true");
  }
*/  
  var sslTrust = document.getElementById("sslTrustGroup");
  sslTrust.value = certdb.isCertTrusted(cert, nsIX509Cert.SERVER_CERT, 
                                        nsIX509CertDB.TRUSTED_SSL);
}

function doSSLOK()
{
  var sslTrust = document.getElementById("sslTrustGroup");
  var trustssl = sslTrust.value ? nsIX509CertDB.TRUSTED_SSL : 0;
  //
  //  Set the cert trust
  //
  certdb.setCertTrust(cert, nsIX509Cert.SERVER_CERT, trustssl);
  return true;
}

function doLoadForEmailCert()
{
  var dbkey = self.name;

  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  cert = certdb.findCertByDBKey(dbkey, null);

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var windowReference = document.getElementById('editEmailCert');

  var message1 = bundle.formatStringFromName("editTrustEmail",
                                             [ cert.commonName ],
                                             1);
  setText("certmsg", message1);

  setText("issuer", cert.issuerName);

  var cacert = getCaCertForEntityCert(cert);
  if(cacert == null)
  {
     setText("explanations", bundle.GetStringFromName("issuerNotKnown"));
  }
  else if(certdb.isCertTrusted(cacert, nsIX509Cert.CA_CERT,
                                                nsIX509CertDB.TRUSTED_EMAIL))
  {
     setText("explanations", bundle.GetStringFromName("issuerTrusted"));
  }
  else
  {
     setText("explanations", bundle.GetStringFromName("issuerNotTrusted"));
  }
/*
  if(cacert == null)
  {
     var editButton = document.getElementById('editca-button');
	 editButton.setAttribute("disabled","true");
  }
*/  
  var sslTrust = document.getElementById("sslTrustGroup");
  sslTrust.value = certdb.isCertTrusted(cert, nsIX509Cert.EMAIL_CERT, 
                                        nsIX509CertDB.TRUSTED_EMAIL);
}

function doEmailOK()
{
  var sslTrust = document.getElementById("sslTrustGroup");
  var trustemail = sslTrust.value ? nsIX509CertDB.TRUSTED_EMAIL : 0;
  //
  //  Set the cert trust
  //
  certdb.setCertTrust(cert, nsIX509Cert.EMAIL_CERT, trustemail);
  return true;
}

function editCaTrust()
{
   var cacert = getCaCertForEntityCert(cert);
   if(cacert != null)
   {
      window.openDialog('chrome://pippki/content/editcacert.xul', cacert.dbKey,
                        'chrome,centerscreen,modal');
   }
   else
   {
      var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
      doPrompt(bundle.GetStringFromName("issuerCertNotFound"));
   }
}

function getCaCertForEntityCert(cert)
{
   var i=1;
   var nextCertInChain;
   nextCertInChain = cert;
   var lastSubjectName="";
   while(true)
   {
     if(nextCertInChain == null)
     {
        return null;
     }
     if((nextCertInChain.type == nsIX509Cert.CA_CERT) || 
        (nextCertInChain.subjectName == lastSubjectName))
     {
        break;
     }

     lastSubjectName = nextCertInChain.subjectName;
     nextCertInChain = nextCertInChain.issuer;
   }

   return nextCertInChain;
}

