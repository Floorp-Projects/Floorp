// -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const nsPK11TokenDB = "@mozilla.org/security/pk11tokendb;1";
const nsIPK11TokenDB = Ci.nsIPK11TokenDB;
const nsIDialogParamBlock = Ci.nsIDialogParamBlock;
const nsPKCS11ModuleDB = "@mozilla.org/security/pkcs11moduledb;1";
const nsIPKCS11ModuleDB = Ci.nsIPKCS11ModuleDB;
const nsIPKCS11Slot = Ci.nsIPKCS11Slot;
const nsIPK11Token = Ci.nsIPK11Token;

var params;
var pw1;

function init() {
  pw1 = document.getElementById("pw1");

  process();
  document.addEventListener("dialogaccept", setPassword);
}

function process() {
  // If the token is unitialized, don't use the old password box.
  // Otherwise, do.

  let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
    Ci.nsIPK11TokenDB
  );
  let token = tokenDB.getInternalKeyToken();
  if (token) {
    let oldpwbox = document.getElementById("oldpw");
    let msgBox = document.getElementById("message");
    if ((token.needsLogin() && token.needsUserInit) || !token.needsLogin()) {
      oldpwbox.setAttribute("hidden", "true");
      msgBox.removeAttribute("hidden");

      if (!token.needsLogin()) {
        oldpwbox.setAttribute("inited", "empty");
      } else {
        oldpwbox.setAttribute("inited", "true");
      }

      // Select first password field
      document.getElementById("pw1").focus();
    } else {
      // Select old password field
      oldpwbox.removeAttribute("hidden");
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

async function createAlert(titleL10nId, messageL10nId) {
  const [title, message] = await document.l10n.formatValues([
    { id: titleL10nId },
    { id: messageL10nId },
  ]);
  Services.prompt.alert(window, title, message);
}

function setPassword() {
  var pk11db = Cc[nsPK11TokenDB].getService(nsIPK11TokenDB);
  var token = pk11db.getInternalKeyToken();

  var oldpwbox = document.getElementById("oldpw");
  var initpw = oldpwbox.getAttribute("inited");

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
          // This makes no sense that we arrive here,
          // we reached a case that should have been prevented by checkPasswords.
        } else {
          if (pw1.value == "") {
            var secmoddb = Cc[nsPKCS11ModuleDB].getService(nsIPKCS11ModuleDB);
            if (secmoddb.isFIPSEnabled) {
              // empty passwords are not allowed in FIPS mode
              createAlert(
                "pw-change-failed-title",
                "pw-change2empty-in-fips-mode"
              );
              passok = 0;
            }
          }
          if (passok) {
            token.changePassword(oldpw, pw1.value);
            if (pw1.value == "") {
              createAlert("pw-change-success-title", "pw-erased-ok");
            } else {
              createAlert("pw-change-success-title", "pw-change-ok");
            }
          }
        }
      } else {
        oldpwbox.focus();
        oldpwbox.setAttribute("value", "");
        createAlert("pw-change-failed-title", "incorrect-pw");
      }
    } catch (e) {
      Cu.reportError(e);
      createAlert("pw-change-failed-title", "failed-pw-change");
    }
  } else {
    token.initPassword(pw1.value);
    if (pw1.value == "") {
      createAlert("pw-change-success-title", "pw-not-wanted");
    }
  }
}

function setPasswordStrength() {
  // Here is how we weigh the quality of the password
  // number of characters
  // numbers
  // non-alpha-numeric chars
  // upper and lower case characters

  var pw = document.getElementById("pw1").value;

  // length of the password
  var pwlength = pw.length;
  if (pwlength > 5) {
    pwlength = 5;
  }

  // use of numbers in the password
  var numnumeric = pw.replace(/[0-9]/g, "");
  var numeric = pw.length - numnumeric.length;
  if (numeric > 3) {
    numeric = 3;
  }

  // use of symbols in the password
  var symbols = pw.replace(/\W/g, "");
  var numsymbols = pw.length - symbols.length;
  if (numsymbols > 3) {
    numsymbols = 3;
  }

  // use of uppercase in the password
  var numupper = pw.replace(/[A-Z]/g, "");
  var upper = pw.length - numupper.length;
  if (upper > 3) {
    upper = 3;
  }

  var pwstrength =
    pwlength * 10 - 20 + numeric * 10 + numsymbols * 15 + upper * 10;

  // make sure we're give a value between 0 and 100
  if (pwstrength < 0) {
    pwstrength = 0;
  }

  if (pwstrength > 100) {
    pwstrength = 100;
  }

  var mymeter = document.getElementById("pwmeter");
  mymeter.value = pwstrength;
}

function checkPasswords() {
  var pw1 = document.getElementById("pw1").value;
  var pw2 = document.getElementById("pw2").value;
  var ok = document.documentElement.getButton("accept");

  var oldpwbox = document.getElementById("oldpw");
  if (oldpwbox) {
    var initpw = oldpwbox.getAttribute("inited");

    if (initpw == "empty" && pw1 == "") {
      // The token has already been initialized, therefore this dialog
      // was called with the intention to change the password.
      // The token currently uses an empty password.
      // We will not allow changing the password from empty to empty.
      ok.setAttribute("disabled", "true");
      return;
    }
  }

  if (pw1 == pw2) {
    ok.setAttribute("disabled", "false");
  } else {
    ok.setAttribute("disabled", "true");
  }
}
