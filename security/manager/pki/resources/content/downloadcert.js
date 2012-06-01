/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIPKIParamBlock = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert = Components.interfaces.nsIX509Cert;

var pkiParams;
var params;
var caName;
var cert;

function onLoad()
{
  pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  params = pkiParams.QueryInterface(nsIDialogParamBlock);
  var isupport = pkiParams.getISupportAtIndex(1);
  cert = isupport.QueryInterface(nsIX509Cert);

  caName = cert.commonName; 

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

  if (!caName.length)
    caName = bundle.GetStringFromName("unnamedCA");

  var message2 = bundle.formatStringFromName("newCAMessage1",
                                             [ caName ],
                                              1);
  setText("message2", message2);
}

function viewCert()
{
  viewCertHelper(window, cert);
}

function doOK()
{
  var checkSSL = document.getElementById("trustSSL");
  var checkEmail = document.getElementById("trustEmail");
  var checkObjSign = document.getElementById("trustObjSign");
  if (checkSSL.checked)
    params.SetInt(2,1);
  else
    params.SetInt(2,0);
  if (checkEmail.checked)
    params.SetInt(3,1);
  else
    params.SetInt(3,0);
  if (checkObjSign.checked)
    params.SetInt(4,1);
  else
    params.SetInt(4,0);
  params.SetInt(1,1);
  return true;
}

function doCancel()
{
  params.SetInt(1,0);
  return true;
}
