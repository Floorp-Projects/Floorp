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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 */


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;

var dialogParams;
var pkiParams;
var cert=null;

function onLoad()
{
  pkiParams    = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  dialogParams = pkiParams.QueryInterface(nsIDialogParamBlock);
 
  var isupports = pkiParams.getISupportAtIndex(1);
  cert = isupports.QueryInterface(nsIX509Cert); 
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var dispName = cert.commonName;
  if (dispName == null)
    dispName = cert.windowTitle;

  var msg = bundle.formatStringFromName("escrowFinalMessage",
                                        [dispName], 1);
  setText("message1",msg);

  var wdth = window.innerWidth; // THIS IS NEEDED,
  window.sizeToContent();
  windowReference.setAttribute("width",window.innerWidth + 30);
  var hght = window.innerHeight; // THIS IS NEEDED,
  window.sizeToContent();
  windowReference.setAttribute("height",window.innerHeight + 30);

}

function doOK()
{
  dialogParams.SetInt(1,1);
  window.close();
}

function doCancel()
{
  dialogParams.SetInt(1,0);
  window.close();
}

function viewCert()
{
  cert.view();
}
