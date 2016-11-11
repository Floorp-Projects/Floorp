// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gRemovePasswordDialog = {
  _token    : null,
  _bundle   : null,
  _prompt   : null,
  _okButton : null,
  _password : null,
  init: function()
  {
    this._prompt = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                             .getService(Components.interfaces.nsIPromptService);
    this._bundle = document.getElementById("bundlePreferences");

    this._okButton = document.documentElement.getButton("accept");
    this._okButton.label = this._bundle.getString("pw_remove_button");

    this._password = document.getElementById("password");

    var pk11db = Components.classes["@mozilla.org/security/pk11tokendb;1"]
                           .getService(Components.interfaces.nsIPK11TokenDB);
    this._token = pk11db.getInternalKeyToken();

    // Initialize the enabled state of the Remove button by checking the
    // initial value of the password ("" should be incorrect).
    this.validateInput();
  },

  validateInput: function()
  {
    this._okButton.disabled = !this._token.checkPassword(this._password.value);
  },

  removePassword: function()
  {
    if (this._token.checkPassword(this._password.value)) {
      this._token.changePassword(this._password.value, "");
      this._prompt.alert(window,
                         this._bundle.getString("pw_change_success_title"),
                         this._bundle.getString("pw_erased_ok")
                         + " " + this._bundle.getString("pw_empty_warning"));
    }
    else {
      this._password.value = "";
      this._password.focus();
      this._prompt.alert(window,
                         this._bundle.getString("pw_change_failed_title"),
                         this._bundle.getString("incorrect_pw"));
    }
  },
};

