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

/*
 * LoginManagerPromptFactory
 *
 * Implements nsIPromptFactory
 *
 * Invoked by NS_NewAuthPrompter2()
 * [embedding/components/windowwatcher/src/nsPrompt.cpp]
 */
function LoginManagerPromptFactory() {}

LoginManagerPromptFactory.prototype = {

    classDescription : "LoginManagerPromptFactory",
    contractID : "@mozilla.org/passwordmanager/authpromptfactory;1",
    classID : Components.ID("{447fc780-1d28-412a-91a1-466d48129c65}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIPromptFactory]),

    _promptService : null,
    _pwmgr         : null,

    _initialized : false,

    getPrompt : function (aWindow, aIID) {

        if (!this._initialized) {
            // Login manager service
            this._pwmgr = Cc["@mozilla.org/login-manager;1"]
                                .getService(Ci.nsILoginManager);

            // Prompt service for user interaction
            this._promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"]
                                    .getService(Ci.nsIPromptService2);

            this._initialized = true;
        }

        return new LoginManagerPrompter(this._pwmgr, this._promptService,
                                        aWindow).QueryInterface(aIID);
    }
}; // end of LoginManagerPromptFactory implementation




/* ==================== LoginManagerPrompter ==================== */




/*
 * LoginManagerPrompter
 *
 * Implements nsIAuthPrompt2.
 *
 * Invoked by a channel for protocol-based authentication (eg HTTP
 * Authenticate, FTP login)
 */
function LoginManagerPrompter(pwmgr, promptService, window) {
  this._pwmgr = pwmgr;
  this._promptService = promptService;
  this._window = window;

  this.log("===== initialized =====");
}
LoginManagerPrompter.prototype = {

    QueryInterface : XPCOMUtils.generateQI([Ci.nsIAuthPrompt2]),

    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"]
                                    .getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    _promptService : null,
    _pwmgr         : null,
    _window        : null,

    _debug         : false,


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console window.
     */
    log : function (message) {
        if (!this._debug)
            return;

        dump("Pwmgr Prompter: " + message + "\n");
        this._logService.logStringMessage("Pwmgr Prompter: " + message);
    },


    /*
     * promptAuth
     *
     * Implementation of nsIAuthPrompt2.
     *
     * nsIChannel aChannel
     * int        aLevel
     * nsIAuthInformation aAuthInfo
     * boolean    aConfirm
     */
    promptAuth : function (aChannel, aLevel, aAuthInfo, aConfirm) {
        var rememberLogin = false;
        var selectedLogin = null;
        var checkboxLabel = null;

        this.log("===== promptAuth called =====");

        var hostname, httpRealm;
        [hostname, httpRealm] = this._GetAuthKey(aChannel, aAuthInfo);

        if (this._pwmgr.getLoginSavingEnabled(hostname)) {
            checkboxLabel = this.getLocalizedString("rememberPassword");


            var foundLogins = this._pwmgr.findLogins({},
                                            hostname, null, httpRealm);

            // XXX Like the original code, we can't deal with multiple
            // account selection. (bug 227632)
            if (foundLogins.length > 0) {
                selectedLogin = foundLogins[0];
                this._SetAuthInfo(aAuthInfo, selectedLogin.username,
                                             selectedLogin.password);
                rememberLogin = true;
            }
        }

        // if checkboxLabel is null, the checkbox won't be shown at all.
        var checkbox = { value : rememberLogin };

        var ok = this._promptService.promptAuth(this._window, aChannel,
                                aLevel, aAuthInfo, checkboxLabel, checkbox);
        rememberLogin = checkbox.value;

        if (ok && rememberLogin) {
            var newLogin = Cc["@mozilla.org/login-manager/loginInfo;1"]
                                .createInstance(Ci.nsILoginInfo);
            newLogin.init(hostname, null, httpRealm,
                            aAuthInfo.username, aAuthInfo.password,
                            "", "");

            // If we didn't find an existing login, or if the username
            // changed, save as a new login.
            if (!selectedLogin ||
                aAuthInfo.username != selectedLogin.username) {

                // add as new
                this.log("Adding login for " + aAuthInfo.username +
                         " @ " + hostname + " (" + httpRealm + ")");
                this._pwmgr.addLogin(newLogin);

            } else if (selectedLogin &&
                       aAuthInfo.password != selectedLogin.password) {

                this.log("Updating password for " + aAuthInfo.username +
                         " @ " + hostname + " (" + httpRealm + ")");
                // update password
                this._pwmgr.modifyLogin(foundLogins[0], newLogin);

            } else {
                this.log("Login unchanged, no further action needed.");
                return ok;
            }
        }

        return ok;
    },

    asyncPromptAuth : function () {
        return NS_ERROR_NOT_IMPLEMENTED;
    },

    // From /netwerk/base/public/nsNetUtil.h....
    /**
     * This function is a helper to get a protocol's default port if the
     * URI does not specify a port explicitly. Returns -1 if protocol has no
     * concept of ports or if there was an error getting the port.
     */
    _GetRealPort : function (aURI) {
        var port = aURI.port;

        if (port != -1)
            return port; // explicitly specified

        // Otherwise, we have to get the default port from the protocol handler
        // Need the scheme first
        var scheme = aURI.scheme;

        var ioService = Cc["@mozilla.org/network/io-service;1"]
                            .getService(Ci.nsIIOService);

        var handler = ioService.getProtocolHandler(scheme);
        port = handler.defaultPort;

        return port;
    },


    // From: /embedding/components/windowwatcher/public/nsPromptUtils.h
    // Args: nsIChannel, nsIAuthInformation, boolean, string, int
    _GetAuthHostPort : function (aChannel, aAuthInfo) {

        // Have to distinguish proxy auth and host auth here...
        var flags = aAuthInfo.flags;

        if (flags & (Ci.nsIAuthInformation.AUTH_PROXY)) {
            // TODO: untested...
            var proxied = aChannel.QueryInterface(Ci.nsIProxiedChannel);
            if (!proxied)
                throw "proxy auth needs nsIProxiedChannel";

            var info = proxied.proxyInfo;
            if (!info)
                throw "proxy auth needs nsIProxyInfo";

            var idnhost = info.host;
            var port    = info.port;

            var idnService = Cc["@mozilla.org/network/idn-service;1"]
                                .getService(Ci.nsIIDNService);
            host = idnService.convertUTF8toACE(idnhost);
        } else {
            var host = aChannel.URI.host;
            var port = this._GetRealPort(aChannel.URI);
        }

        return [host, port];
    },


    // From: /embedding/components/windowwatcher/public/nsPromptUtils.h
    // Args: nsIChannel, nsIAuthInformation
    _GetAuthKey : function (aChannel, aAuthInfo) {
        var key = "";
        // HTTP does this differently from other protocols
        var http = aChannel.QueryInterface(Ci.nsIHttpChannel);
        if (!http) {
            key = aChannel.URI.prePath;
            this.log("_GetAuthKey: got http channel, key is: " + key);
            return key;
        }

        var [host, port] = this._GetAuthHostPort(aChannel, aAuthInfo);

        var realm = aAuthInfo.realm;

        key += host;
        key += ':';
        key += port;

        this.log("_GetAuthKey got host: " + key + " and realm: " + realm);

        return [key, realm];
    },


    // From: /embedding/components/windowwatcher/public/nsPromptUtils.h
    // Args: nsIAuthInformation, string, string
    /**
     * Given a username (possibly in DOMAIN\user form) and password, parses the
     * domain out of the username if necessary and sets domain, username and
     * password on the auth information object.
     */
    _SetAuthInfo : function (aAuthInfo, username, password) {
        var flags = aAuthInfo.flags;
        if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
            // Domain is separated from username by a backslash
            var idx = username.indexOf("\\");
            if (idx == -1) {
                aAuthInfo.username = username;
            } else {
                aAuthInfo.domain   =  username.substring(0, idx);
                aAuthInfo.username =  username.substring(idx+1);
            }
        } else {
            aAuthInfo.username = username;
        }
        aAuthInfo.password = password;
    },


    _bundle : null,
    getLocalizedString : function (key) {

        if (!this._bundle) {
            var bunService = Cc["@mozilla.org/intl/stringbundle;1"]
                                .getService(Ci.nsIStringBundleService);
            this._bundle = bunService.createBundle(
                        "chrome://passwordmgr/locale/passwordmgr.properties");

            if (!this._bundle)
                throw "String bundle not present!";
        }

        return this._bundle.GetStringFromName(key);
    }
}; // end of LoginManagerPrompter implementation

var component = [LoginManagerPromptFactory];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
