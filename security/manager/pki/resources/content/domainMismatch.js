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
 */


const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;

var pkiParams;
var dialogParams;
var cert;

function onLoad()
{
  pkiParams = window.arguments[0].QueryInterface(nsIPKIParamBlock);  
  var isupport = pkiParams.getISupportAtIndex(1);
  cert     = isupport.QueryInterface(nsIX509Cert);
  dialogParams = pkiParams.QueryInterface(nsIDialogParamBlock);
  var connectURL = dialogParams.GetString(1); 

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

  var message1 = bundle.formatStringFromName("mismatchDomainMsg1", 
                                             [ connectURL, cert.commonName ],
                                             2);
  var message2 = bundle.formatStringFromName("mismatchDomainMsg2", 
                                             [ connectURL ],
                                              1);
  setText("message1", message1);
  setText("message2", message2);
  //Set the focus so key press events work
  document.getElementById('ok-button').focus();
  window.sizeToContent();
}

function viewCert()
{
  viewCertHelper(window, cert);
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
