/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

document.addEventListener("dialogaccept", resetPassword);

function resetPassword() {
  var pk11db = Cc["@mozilla.org/security/pk11tokendb;1"]
                 .getService(Ci.nsIPK11TokenDB);
  var token = pk11db.getInternalKeyToken();
  token.reset();

  try {
    Services.logins.removeAllLogins();
  } catch (e) {
  }

  var bundle = document.getElementById("pippki_bundle");
  var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService();
  promptService = promptService.QueryInterface(Ci.nsIPromptService);
  if (promptService && bundle) {
    promptService.alert(window,
                        bundle.getString("resetPasswordConfirmationTitle"),
                        bundle.getString("resetPasswordConfirmationMessage"));
  }
}
