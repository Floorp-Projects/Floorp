/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;

var dialogParams;
var pkiParams;
var cert;
var hostport;

function initCertErrorDialog()
{
  pkiParams    = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  dialogParams = pkiParams.QueryInterface(nsIDialogParamBlock);

  var isupport = pkiParams.getISupportAtIndex(1);
  cert = isupport.QueryInterface(nsIX509Cert);

  var portNumber = dialogParams.GetInt(1);
  var hostName = dialogParams.GetString(1);
  var msg = dialogParams.GetString(2);

  hostport = hostName + ":" + portNumber;
  setText("warningText", msg);
}

function viewCert()
{
  viewCertHelper(window, cert);
}
