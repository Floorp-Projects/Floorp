/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 *  Håkan Waara <hwaara@chello.se>
 */


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;

var dialogParams;
var pkiParams;
var cert;


function onLoad()
{

  pkiParams    = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  dialogParams = pkiParams.QueryInterface(nsIDialogParamBlock);

  var isupport = pkiParams.getISupportAtIndex(1);
  cert = isupport.QueryInterface(nsIX509Cert);

  var bundle = srGetStrBundle("chrome://pippki/locale/newserver.properties");
  var gBundleBrand = srGetStrBundle("chrome://global/locale/brand.properties");

  var brandName = gBundleBrand.GetStringFromName("brandShortName");
  var continueButton = bundle.GetStringFromName("continueButton");

  document.documentElement.getButton("accept").label = continueButton;

  var message = 
    bundle.formatStringFromName("newServerMessage",
                                 [cert.commonName],
                                 1);
  var notRecognized = 
    bundle.formatStringFromName("certNotRecognized", 
                                 [brandName], 
                                 1);

  setText("message", message);
  setText("notRecognized", notRecognized);
  window.sizeToContent();
}

function doHelpButton()
{
  openHelp('new_web_cert');
}

function doOK()
{
  // the user pressed OK
  dialogParams.SetInt(1,1);
  var checkbox = document.getElementById("alwaysAccept");

  // 0 = accept perm, 1 = accept for this session - just the opposite
  // of the checkbox value.
  dialogParams.SetInt(2, !checkbox.checked);
  return true;
}

function doCancel()
{
  // the user pressed cancel
  dialogParams.SetInt(1,0);
  return true;
}

function viewCert()
{
  cert.view();
}
