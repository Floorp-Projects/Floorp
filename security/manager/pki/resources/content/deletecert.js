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

  var windowReference = document.getElementById('deleteCert');
  windowReference.setAttribute("title", cert.commonName);

  var certname = document.getElementById("certname");
  certname.setAttribute("value", cert.commonName);

}

function doOK()
{
  certdb.deleteCertificate(cert);
  window.close();
}
