<!-- -*- Mode: HTML -*-
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla Community QA Extension
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ben Hsieh <ben.hsieh@gmail.com>
 *
 *  Portions taken from David Hamp-Gonsalves' Buggybar (buggybar@gmail.com)
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

var accountCreate = {
    oldPassword: null,    //TODO: is this secure?
    oldUsername: null,
    updateFunction : null,
  loadSetup : function() {
    document.getElementById('qa-settings-createaccount-frame').src =
    litmus.baseURL+'extension.cgi?createAccount=1';
        accountCreate.updateFunction = window.arguments[0];

        accountCreate.oldPassword = qaPref.litmus.getPassword();
        accountCreate.oldUsername = qaPref.litmus.getUsername();
  },
    validateAccount : function() {

    var account = qaSetup.retrieveAccount("qa-settings-createaccount-frame");
    var uname = account.name;
    var passwd = account.password;


        if (uname == '' || passwd == '') {
            alert("No username or password has been registered.");
            return false;    //we need better error handling.
        }

    var location = document.getElementById('qa-settings-createaccount-frame').contentDocument.location + "";
    if (location.indexOf("createAccount") != -1) {
      alert("Account not created! You most likely need to click the 'Create Account' button");
      return false;
    }

        qaPref.litmus.setPassword(uname, passwd);
        accountCreate.updateFunction();

      // TODO: ideally we would validate against litmus, but...
    return true;
  },

    handleCancel : function() {
        qaPref.litmus.setPassword(oldUsername, oldPassword);
    }
}
