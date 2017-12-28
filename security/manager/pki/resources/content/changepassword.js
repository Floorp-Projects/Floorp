/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const nsIPK11TokenDB = Components.interfaces.nsIPK11TokenDB;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Components.interfaces.nsIPKCS11ModuleDB;
const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
const nsIPK11Token = Components.interfaces.nsIPK11Token;

var params;
var tokenName = "";
var pw1;

function doPrompt(msg) {
  let prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
    getService(Components.interfaces.nsIPromptService);
  prompts.alert(window, null, msg);
}

function onLoad() {
  document.documentElement.getButton("accept").disabled = true;

  pw1 = document.getElementById("pw1");
  params = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);
  tokenName = params.GetString(1);

  document.getElementById("tokenName").setAttribute("value", tokenName);

  process();
}

function process() {
  let bundle = document.getElementById("pippki_bundle");

  // If the token is unitialized, don't use the old password box.
  // Otherwise, do.

  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);
  let token;
  if (tokenName.length > 0) {
    token = tokenDB.findTokenByName(tokenName);
  } else {
    token = tokenDB.getInternalKeyToken();
  }
  if (token) {
    let oldpwbox = document.getElementById("oldpw");
    let msgBox = document.getElementById("message");
    if ((token.needsLogin() && token.needsUserInit) || !token.needsLogin()) {
      oldpwbox.setAttribute("hidden", "true");
      msgBox.setAttribute("value", bundle.getString("password_not_set"));
      msgBox.setAttribute("hidden", "false");

      if (!token.needsLogin()) {
        oldpwbox.setAttribute("inited", "empty");
      } else {
        oldpwbox.setAttribute("inited", "true");
      }

      // Select first password field
      document.getElementById("pw1").focus();
    } else {
      // Select old password field
      oldpwbox.setAttribute("hidden", "false");
      msgBox.setAttribute("hidden", "true");
      oldpwbox.setAttribute("inited", "false");
      oldpwbox.focus();
    }
  }

  if (params) {
    // Return value 0 means "canceled"
    params.SetInt(1, 0);
  }

  checkPasswords();
}

function setPassword() {
  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                  .getService(Ci.nsIPK11TokenDB);
  let token;
  if (tokenName.length > 0) {
    token = tokenDB.findTokenByName(tokenName);
  } else {
    token = tokenDB.getInternalKeyToken();
  }

  var oldpwbox = document.getElementById("oldpw");
  var initpw = oldpwbox.getAttribute("inited");
  var bundle = document.getElementById("pippki_bundle");

  var success = false;

  if (initpw == "false" || initpw == "empty") {
    try {
      var oldpw = "";
      var passok = 0;

      if (initpw == "empty") {
        passok = 1;
      } else {
        oldpw = oldpwbox.value;
        passok = token.checkPassword(oldpw);
      }

      if (passok) {
        if (initpw == "empty" && pw1.value == "") {
          // checkPasswords() should have prevented this path from being reached.
        } else {
          if (pw1.value == "") {
            var secmoddb = Components.classes[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
            if (secmoddb.isFIPSEnabled) {
              // empty passwords are not allowed in FIPS mode
              doPrompt(bundle.getString("pw_change2empty_in_fips_mode"));
              passok = 0;
            }
          }
          if (passok) {
            token.changePassword(oldpw, pw1.value);
            if (pw1.value == "") {
              doPrompt(bundle.getString("pw_erased_ok")
                    + " "
                    + bundle.getString("pw_empty_warning"));
            } else {
              doPrompt(bundle.getString("pw_change_ok"));
            }
            success = true;
          }
        }
      } else {
        oldpwbox.focus();
        oldpwbox.setAttribute("value", "");
        doPrompt(bundle.getString("incorrect_pw"));
      }
    } catch (e) {
      doPrompt(bundle.getString("failed_pw_change"));
    }
  } else {
    token.initPassword(pw1.value);
    if (pw1.value == "") {
      doPrompt(bundle.getString("pw_not_wanted") + " " +
               bundle.getString("pw_empty_warning"));
    }
    success = true;
  }

  if (success && params) {
    // Return value 1 means "successfully executed ok"
    params.SetInt(1, 1);
  }

  // Terminate dialog
  return success;
}

function setPasswordStrength() {
  // We weigh the quality of the password by checking the number of:
  //  - Characters
  //  - Numbers
  //  - Non-alphanumeric chars
  //  - Upper and lower case characters

  let pw = document.getElementById("pw1").value;

  let pwlength = pw.length;
  if (pwlength > 5) {
    pwlength = 5;
  }

  let numnumeric = pw.replace(/[0-9]/g, "");
  let numeric = pw.length - numnumeric.length;
  if (numeric > 3) {
    numeric = 3;
  }

  let symbols = pw.replace(/\W/g, "");
  let numsymbols = pw.length - symbols.length;
  if (numsymbols > 3) {
    numsymbols = 3;
  }

  let numupper = pw.replace(/[A-Z]/g, "");
  let upper = pw.length - numupper.length;
  if (upper > 3) {
    upper = 3;
  }

  let pwstrength = (pwlength * 10) - 20 + (numeric * 10) + (numsymbols * 15) +
                   (upper * 10);

  // Clamp strength to [0, 100].
  if (pwstrength < 0) {
    pwstrength = 0;
  }
  if (pwstrength > 100) {
    pwstrength = 100;
  }

  let meter = document.getElementById("pwmeter");
  meter.setAttribute("value", pwstrength);
}

function checkPasswords() {
  let pw1 = document.getElementById("pw1").value;
  let pw2 = document.getElementById("pw2").value;

  var oldpwbox = document.getElementById("oldpw");
  if (oldpwbox) {
    var initpw = oldpwbox.getAttribute("inited");

    if (initpw == "empty" && pw1 == "") {
      // The token has already been initialized, therefore this dialog
      // was called with the intention to change the password.
      // The token currently uses an empty password.
      // We will not allow changing the password from empty to empty.
      document.documentElement.getButton("accept").disabled = true;
      return;
    }
  }

  document.documentElement.getButton("accept").disabled = (pw1 != pw2);
}
