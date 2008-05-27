# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gRemovePasswordDialog = {
  _token    : null,
  _bundle   : null,
  _prompt   : null,
  _okButton : null,
  _password : null,
  init: function ()
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
  
  validateInput: function ()
  {
    this._okButton.disabled = !this._token.checkPassword(this._password.value);    
  },
  
  removePassword: function ()
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

