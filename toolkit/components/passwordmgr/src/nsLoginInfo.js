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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
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


const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function nsLoginInfo() {}

nsLoginInfo.prototype = {

    classDescription  : "LoginInfo",
    contractID : "@mozilla.org/login-manager/loginInfo;1",
    classID : Components.ID("{0f2f347c-1e4f-40cc-8efd-792dea70a85e}"),
    QueryInterface: XPCOMUtils.generateQI([Ci.nsILoginInfo]), 

    // Allow storage-Legacy.js to get at the JS object so it can
    // slap on a few extra properties for internal use.
    get wrappedJSObject() {
        return this;
    },

    hostname      : null,
    formSubmitURL : null,
    httpRealm     : null,
    username      : null,
    password      : null,
    usernameField : null,
    passwordField : null,

    init : function (aHostname, aFormSubmitURL, aHttpRealm,
                     aUsername,      aPassword,
                     aUsernameField, aPasswordField) {
        this.hostname      = aHostname;
        this.formSubmitURL = aFormSubmitURL;
        this.httpRealm     = aHttpRealm;
        this.username      = aUsername;
        this.password      = aPassword;
        this.usernameField = aUsernameField;
        this.passwordField = aPasswordField;
    },

    matches : function (aLogin, ignorePassword) {
        if (this.hostname      != aLogin.hostname      ||
            this.httpRealm     != aLogin.httpRealm     ||
            this.username      != aLogin.username)
            return false;

        if (!ignorePassword && this.password != aLogin.password)
            return false;

        // If either formSubmitURL is blank (but not null), then match.
        if (this.formSubmitURL != "" && aLogin.formSubmitURL != "" &&
            this.formSubmitURL != aLogin.formSubmitURL)
            return false;

        // The .usernameField and .passwordField values are ignored.

        return true;
    },

    equals : function (aLogin) {
        if (this.hostname      != aLogin.hostname      ||
            this.formSubmitURL != aLogin.formSubmitURL ||
            this.httpRealm     != aLogin.httpRealm     ||
            this.username      != aLogin.username      ||
            this.password      != aLogin.password      ||
            this.usernameField != aLogin.usernameField ||
            this.passwordField != aLogin.passwordField)
            return false;

        return true;
    }

}; // end of nsLoginInfo implementation

var component = [nsLoginInfo];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
