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

  var bundle = document.getElementById("newserver_bundle");

  var intro = bundle.getFormattedString(
    "newserver.intro", [cert.commonName]);

  var reason3 = bundle.getFormattedString(
    "newserver.reason3", [cert.commonName]);

  var question = bundle.getFormattedString(
    "newserver.question", [cert.commonName]);

  setText("intro", intro);
  setText("reason3", reason3);
  setText("question", question);

  window.sizeToContent();
}

function doHelpButton()
{
  openHelp('new_web_cert');
}

function doOK()
{
  var selectedID = document.getElementById("whatnow").selectedItem.id;

  if (selectedID == "refuse") {
    dialogParams.SetInt(1,0);
  }
  else {
    dialogParams.SetInt(1,1);

    // 0 = accept perm, 1 = accept for this session
    var userchoice = 1;
    
    if (selectedID == "remember") {
      userchoice = 0;
    }

    dialogParams.SetInt(2, userchoice);
  }
}

function doCancel()
{
  dialogParams.SetInt(1,0);
}

function viewCert()
{
  viewCertHelper(window, cert);
}
