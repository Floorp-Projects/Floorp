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

function nsLoginInfo() {}

nsLoginInfo.prototype = {

    QueryInterface : function (iid) {
        var interfaces = [Ci.nsILoginInfo, Ci.nsISupports];
        if (!interfaces.some( function(v) { return iid.equals(v) } ))
            throw Components.results.NS_ERROR_NO_INTERFACE;
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

    equalsIgnorePassword : function (aLogin) {
        if (this.hostname != aLogin.hostname)
            return false;

        // If either formSubmitURL is blank (but not null), then match.
        if (this.formSubmitURL != "" && aLogin.formSubmitURL != "" &&
            this.formSubmitURL != aLogin.formSubmitURL)
            return false;

        if (this.httpRealm != aLogin.httpRealm)
            return false;

        if (this.username != aLogin.username)
            return false;

        if (this.usernameField != aLogin.usernameField)
            return false;

        // The .password and .passwordField values are ignored.

        return true;
    },

    equals : function (aLogin) {
        if (!this.equalsIgnorePassword(aLogin) ||
            this.password      != aLogin.password   ||
            this.passwordField != aLogin.passwordField)
            return false;

        return true;
    }

}; // end of nsLoginInfo implementation




// Boilerplate code for component registration...
var gModule = {
    registerSelf: function(componentManager, fileSpec, location, type) {
        componentManager = componentManager.QueryInterface(
                                                Ci.nsIComponentRegistrar);
        for each (var obj in this._objects) 
            componentManager.registerFactoryLocation(obj.CID,
                    obj.className, obj.contractID,
                    fileSpec, location, type);
    },

    unregisterSelf: function (componentManager, location, type) {
        for each (var obj in this._objects) 
            componentManager.unregisterFactoryLocation(obj.CID, location);
    },

    getClassObject: function(componentManager, cid, iid) {
        if (!iid.equals(Ci.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        for (var key in this._objects) {
            if (cid.equals(this._objects[key].CID))
                return this._objects[key].factory;
        }

        throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    _objects: {
        service: {
            CID : Components.ID("{0f2f347c-1e4f-40cc-8efd-792dea70a85e}"),
            contractID : "@mozilla.org/login-manager/loginInfo;1",
            className  : "LoginInfo",
            factory    : LoginInfoFactory = {
                createInstance: function(aOuter, aIID) {
                    if (aOuter != null)
                        throw Components.results.NS_ERROR_NO_AGGREGATION;
                    var svc = new nsLoginInfo();
                    return svc.QueryInterface(aIID);
                }
            }
        }
    },

    canUnload: function(componentManager) {
        return true;
    }
};

function NSGetModule(compMgr, fileSpec) {
    return gModule;
}
