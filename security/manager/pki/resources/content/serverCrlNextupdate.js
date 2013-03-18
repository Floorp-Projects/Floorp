/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;

var dialogParams;
var pkiParams;
var bundle;

function onLoad()
{
  pkiParams    = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  dialogParams = pkiParams.QueryInterface(nsIDialogParamBlock);
  var isupport = pkiParams.getISupportAtIndex(1);
  var cert = isupport.QueryInterface(nsIX509Cert);
  var connectURL = dialogParams.GetString(1); 
  var gBundleBrand = document.getElementById("brand_bundle");
  var brandName = gBundleBrand.getString("brandShortName");

  bundle = document.getElementById("pippki_bundle");
 
  var message1 = bundle.getFormattedString("crlNextUpdateMsg1",
                                           [brandName, connectURL]);
  var message2 = bundle.getFormattedString("crlNextUpdateMsg2",
                                           [cert.issuerOrganization]);
  setText("message1", message1);
  setText("message2", message2);
}
