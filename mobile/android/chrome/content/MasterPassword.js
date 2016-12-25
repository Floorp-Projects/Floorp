/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");

var MasterPassword = {
  pref: "privacy.masterpassword.enabled",

  get _pk11DB() {
    delete this._pk11DB;
    return this._pk11DB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(Ci.nsIPK11TokenDB);
  },

  get enabled() {
    let token = this._pk11DB.getInternalKeyToken();
    if (token) {
      return token.hasPassword;
    }
    return false;
  },

  setPassword: function setPassword(aPassword) {
    try {
      let token = this._pk11DB.getInternalKeyToken();
      if (token.needsUserInit) {
        token.initPassword(aPassword);
      } else if (!token.needsLogin()) {
        token.changePassword("", aPassword);
      }

      return true;
    } catch(e) {
      dump("MasterPassword.setPassword: " + e);
    }
    return false;
  },

  removePassword: function removePassword(aOldPassword) {
    try {
      let token = this._pk11DB.getInternalKeyToken();
      if (token.checkPassword(aOldPassword)) {
        token.changePassword(aOldPassword, "");
        return true;
      }
    } catch(e) {
      dump("MasterPassword.removePassword: " + e + "\n");
    }
    Snackbars.show(Strings.browser.GetStringFromName("masterPassword.incorrect"), Snackbars.LENGTH_LONG);
    return false;
  }
};
