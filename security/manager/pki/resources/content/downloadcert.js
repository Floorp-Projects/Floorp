/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

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
  let checkSSL = document.getElementById("trustSSL");
  let checkEmail = document.getElementById("trustEmail");
  let checkObjSign = document.getElementById("trustObjSign");

  // Signal which trust bits the user wanted to enable.
  params.SetInt(2, checkSSL.checked ? 1 : 0);
  params.SetInt(3, checkEmail.checked ? 1 : 0);
  params.SetInt(4, checkObjSign.checked ? 1 : 0);

  // Signal that the user accepted.
  params.SetInt(1, 1);
  return true;
}

function doCancel()
{
  params.SetInt(1, 0); // Signal that the user cancelled.
  return true;
}
