/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIX509Cert = Components.interfaces.nsIX509Cert;

var params;
var caName;
var cert;

function onLoad()
{
  params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  cert = params.objects.queryElementAt(0, nsIX509Cert);

  var bundle = document.getElementById("pippki_bundle");

  caName = cert.commonName;
  if (caName.length == 0) {
    caName = bundle.getString("unnamedCA");
  }

  var message2 = bundle.getFormattedString("newCAMessage1", [caName]);
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
