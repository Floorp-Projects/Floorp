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

//var myName;
// XXX yes?
var certdb;
var cert;

function setWindowName()
{
  myName = self.name;
  //var windowReference=document.getElementById('certDetails');
  //windowReference.setAttribute("title","Certificate Detail: \""+myName+"\"");

  //  Get the cert from the cert database
  certdb = Components.classes[nsX509CertDB].getService(nsIX509CertDB);
  //var cert = certdb.getCertByNickname(token, myName);
  cert = certdb.getCertByNickname(null, myName);

  var ssl = document.getElementById("trustSSL");
  if (certdb.getCertTrust(cert, 1)) {
    ssl.setAttribute("checked", "true");
  } else {
    ssl.setAttribute("checked", "false");
  }
  var email = document.getElementById("trustEmail");
  if (certdb.getCertTrust(cert, 2)) {
    email.setAttribute("checked", "true");
  } else {
    email.setAttribute("checked", "false");
  }
  var objsign = document.getElementById("trustObjSign");
  if (certdb.getCertTrust(cert, 4)) {
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
  // clean this up
  var trustssl = (ssl.checked) ? 1 : 0;
  var trustemail = (email.checked) ? 2 : 0;
  var trustobjsign = (objsign.checked) ? 4 : 0;
  //
  //  Set the cert trust
  //
  certdb.setCertTrust(cert, 1, trustssl | trustemail | trustobjsign);
  window.close();
}
