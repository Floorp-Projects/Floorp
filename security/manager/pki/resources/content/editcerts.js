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
 *  Bob Lord <lord@netscape.com>
 *  Ian McGreer <mcgreer@netscape.com>
 */

const nsIX509Cert = Components.interfaces.nsIX509Cert;
const nsX509CertDB = "@mozilla.org/security/x509certdb;1";
const nsIX509CertDB = Components.interfaces.nsIX509CertDB;
const nsIPKIParamBlock = Components.interfaces.nsIPKIParamBlock;

var certdb;
var cert;

function setWindowName()
{
  var dbkey = self.name;

  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  //var pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  //var isupport = pkiParams.getISupportAtIndex(1);
  //cert = isupport.QueryInterface(nsIX509Cert);
  cert = certdb.getCertByDBKey(dbkey, null);

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var windowReference = document.getElementById('editCaCert');

  var message1 = bundle.formatStringFromName("editTrustCA",
                                             [ cert.commonName ],
                                             1);
  setText("certmsg", message1);

  var ssl = document.getElementById("trustSSL");
  if (certdb.getCertTrust(cert, nsIX509Cert.CA_CERT, 
                          nsIX509CertDB.TRUSTED_SSL)) {
    ssl.setAttribute("checked", "true");
  } else {
    ssl.setAttribute("checked", "false");
  }
  var email = document.getElementById("trustEmail");
  if (certdb.getCertTrust(cert, nsIX509Cert.CA_CERT, 
                          nsIX509CertDB.TRUSTED_EMAIL)) {
    email.setAttribute("checked", "true");
  } else {
    email.setAttribute("checked", "false");
  }
  var objsign = document.getElementById("trustObjSign");
  if (certdb.getCertTrust(cert, nsIX509Cert.CA_CERT,  
                          nsIX509CertDB.TRUSTED_OBJSIGN)) {
    objsign.setAttribute("checked", "true");
  } else {
    objsign.setAttribute("checked", "false");
  }

  var xulWindow = document.getElementById("editCaCert");
  var wdth = window.innerWidth; // THIS IS NEEDED,
  window.sizeToContent();
  xulWindow.setAttribute("width",window.innerWidth + 30);
  var hght = window.innerHeight; // THIS IS NEEDED,
  window.sizeToContent();
  xulWindow.setAttribute("height",window.innerHeight + 70);
  

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
  window.close();
}

function doLoadForSSLCert()
{
  var dbkey = self.name;

  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  cert = certdb.getCertByDBKey(dbkey, null);

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var windowReference = document.getElementById('editWebsiteCert');

  var message1 = bundle.formatStringFromName("editTrustSSL",
                                             [ cert.commonName ],
                                             1);
  setText("certmsg", message1);

  setText("issuer", cert.issuerName);

  var cacert = getCaCertForServerCert(cert);
  if(cacert == null)
  {
     setText("explainations",bundle.GetStringFromName("issuerNotKnown"));
  }
  else if(certdb.getCertTrust(cacert, nsIX509Cert.CA_CERT,
                                                nsIX509CertDB.TRUSTED_SSL))
  {
     setText("explainations",bundle.GetStringFromName("issuerTrusted"));
  }
  else
  {
     setText("explainations",bundle.GetStringFromName("issuerNotTrusted"));
  }
/*
  if(cacert == null)
  {
     var editButton = document.getElementById('editca-button');
	 editButton.setAttribute("disabled","true");
  }
*/  
  var trustssl = document.getElementById("trustSSLCert");
  var notrustssl = document.getElementById("dontTrustSSLCert");
  if (certdb.getCertTrust(cert, nsIX509Cert.SERVER_CERT, 
                          nsIX509CertDB.TRUSTED_SSL)) {
    trustssl.radioGroup.selectedItem = trustssl;
  } else {
    trustssl.radioGroup.selectedItem = notrustssl;
  }

  var xulWindow = document.getElementById("editWebsiteCert");
  var wdth = window.innerWidth; // THIS IS NEEDED,
  window.sizeToContent();
  xulWindow.setAttribute("width",window.innerWidth + 30);
  var hght = window.innerHeight; // THIS IS NEEDED,
  window.sizeToContent();
  xulWindow.setAttribute("height",window.innerHeight + 70);

}

function doSSLOK()
{
  var ssl = document.getElementById("trustSSLCert");
  //var checked = ssl.getAttribute("value");
  var trustssl = ssl.selected ? nsIX509CertDB.TRUSTED_SSL : 0;
  //
  //  Set the cert trust
  //
  certdb.setCertTrust(cert, nsIX509Cert.SERVER_CERT, trustssl);
  window.close();
}

function editCaTrust()
{
   var cacert = getCaCertForServerCert(cert);
   if(cacert != null)
   {
      window.openDialog('chrome://pippki/content/editcacert.xul', cacert.dbKey,
                               'chrome,resizable=1,modal');
   }
   else
   {
      var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
      alert(bundle.GetStringFromName("issuerCertNotFound"));
   }
}

function getCaCertForServerCert(cert)
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
                                 (nextCertInChain.subjectName = lastSubjectName))
     {
        break;
     }

     lastSubjectName = nextCertInChain.subjectName;
     nextCertInChain = nextCertInChain.issuer;
   }

   return nextCertInChain;
}

