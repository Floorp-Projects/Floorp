/*
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
 */

const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;

var tokenName;

function onLoad()
{
  if ("arguments" in window) {
    var params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    tokenName = params.GetString(1);
  } else {
    tokenName = self.name;
  }

  var okButton = document.getElementById("ok");
  var cancelButton = document.getElementById("cancel");
  var helpButton = document.getElementById("help");
  
  var bundle = document.getElementById("pippki_bundle");

  doSetOKCancel(resetPassword, null, null, null);

  if (okButton && cancelButton && bundle) {
    okButton.setAttribute("label", bundle.getString("resetPasswordButtonLabel"));
    okButton.removeAttribute("default");
    cancelButton.setAttribute("default", "true");
    cancelButton.focus();
  }
}

function resetPassword()
{
  var pk11db = Components.classes[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var token = pk11db.findTokenByName(tokenName);
  token.reset();

  var pref = Components.classes['@mozilla.org/preferences;1'];
  if (pref) {
    pref = pref.getService(Components.interfaces.nsIPrefBranch);
    try {
      if (pref.getBoolPref("wallet.crypto")) {
        // data in wallet is encrypted, clear it
        var wallet = Components.classes['@mozilla.org/wallet/wallet-service;1'];
        if (wallet) {
          wallet = wallet.getService(Components.interfaces.nsIWalletService);
          wallet.WALLET_DeleteAll();
        }
      }
    }
    catch(e) {
      // wallet.crypto pref is missing
    }
  }

  var bundle = document.getElementById("pippki_bundle");
  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);
  if (promptService && bundle) {
    promptService.alert(window,
      bundle.getString("resetPasswordConfirmationTitle"), 
      bundle.getString("resetPasswordConfirmationMessage"));
  }

  window.close();
}

function doHelpButton()
{
  openHelp('chrome://help/content/help.xul?reset_pwd');
}
