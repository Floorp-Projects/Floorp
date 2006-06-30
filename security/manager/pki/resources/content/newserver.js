/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
 *   Håkan Waara <hwaara@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
  
  // Focus the accept button explicitly, because the dialog onLoad handler
  // focuses the first focusable element in the dialog, which is the "View
  // Certificate" button.
  // Never explicitly focus buttons on OS X, as this breaks default focus
  // handling, outlined in dialog.xml
#ifndef XP_MACOSX
  document.documentElement.getButton("accept").focus();
#endif
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

  return true;
}

function doCancel()
{
  dialogParams.SetInt(1,0);
  return true;
}

function viewCert()
{
  viewCertHelper(window, cert);
}
