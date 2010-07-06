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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function Prompter() {
    // Note that EmbedPrompter clones this implementation.
}

Prompter.prototype = {
    classID          : Components.ID("{1c978d25-b37f-43a8-a2d6-0c7a239ead87}"),
    QueryInterface   : XPCOMUtils.generateQI([Ci.nsIPromptFactory, Ci.nsIPromptService, Ci.nsIPromptService2]),


    /* ----------  private memebers  ---------- */

    pickPrompter : function (domWin) {
        return new ModalPrompter(domWin);
    },


    /* ----------  nsIPromptFactory  ---------- */


    getPrompt : function (domWin, iid) {
        // This is still kind of dumb; the C++ code delegated to login manager
        // here, which in turn calls back into us via nsIPromptService2.
        if (iid.equals(Ci.nsIAuthPrompt2) || iid.equals(Ci.nsIAuthPrompt)) {
            try {
                let pwmgr = Cc["@mozilla.org/passwordmanager/authpromptfactory;1"].
                            getService(Ci.nsIPromptFactory);
                return pwmgr.getPrompt(domWin, iid);
            } catch (e) {
                Cu.reportError("nsPrompter: Delegation to password manager failed: " + e);
            }
        }

        let p = new ModalPrompter(domWin);
        p.QueryInterface(iid);
        return p;
    },


    /* ----------  nsIPromptService  ---------- */


    alert : function (domWin, title, text) {
        let p = this.pickPrompter(domWin);
        p.alert(title, text);
    },

    alertCheck : function (domWin, title, text, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        p.alertCheck(title, text, checkLabel, checkValue);
    },

    confirm : function (domWin, title, text) {
        let p = this.pickPrompter(domWin);
        return p.confirm(title, text);
    },

    confirmCheck : function (domWin, title, text, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.confirmCheck(title, text, checkLabel, checkValue);
    },

    confirmEx : function (domWin, title, text, flags, button0, button1, button2, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.confirmEx(title, text,  flags, button0, button1, button2, checkLabel, checkValue);
    },

    prompt : function (domWin, title, text, value, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.nsIPrompt_prompt(title, text, value, checkLabel, checkValue);
    },

    promptUsernameAndPassword : function (domWin, title, text, user, pass, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.nsIPrompt_promptUsernameAndPassword(title, text, user, pass, checkLabel, checkValue);
    },

    promptPassword : function (domWin, title, text, pass, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.nsIPrompt_promptPassword(title, text, pass, checkLabel, checkValue);
    },

    select : function (domWin, title, text, count, list, selected) {
        let p = this.pickPrompter(domWin);
        return p.select(title, text, count, list, selected);
    },


    /* ----------  nsIPromptService2  ---------- */


    promptAuth : function (domWin, channel, level, authInfo, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.promptAuth(channel, level, authInfo, checkLabel, checkValue);
    },

    asyncPromptAuth : function (domWin, channel, callback, context, level, authInfo, checkLabel, checkValue) {
        let p = this.pickPrompter(domWin);
        return p.asyncPromptAuth(channel, callback, context, level, authInfo, checkLabel, checkValue);
    },

};


// Common utils not specific to a particular prompter style.
let PromptUtils = {

    getLocalizedString : function (key, formatArgs) {
        if (formatArgs)
            return this.strBundle.formatStringFromName(key, formatArgs, formatArgs.length);
        else
            return this.strBundle.GetStringFromName(key);
    },

    confirmExHelper : function (flags, button0, button1, button2) {
        const BUTTON_DEFAULT_MASK = 0x03000000;
        let defaultButtonNum = (flags & BUTTON_DEFAULT_MASK) >> 24;
        let isDelayEnabled = (flags & Ci.nsIPrompt.BUTTON_DELAY_ENABLE);

        // Flags can be used to select a specific pre-defined button label or
        // a caller-supplied string (button0/button1/button2). If no flags are
        // set for a button, then the button won't be shown.
        let argText = [button0, button1, button2];
        let buttonLabels = [null, null, null];
        for (let i = 0; i < 3; i++) {
            let buttonLabel;
            switch (flags & 0xff) {
              case Ci.nsIPrompt.BUTTON_TITLE_OK:
                buttonLabel = PromptUtils.getLocalizedString("OK");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_CANCEL:
                buttonLabel = PromptUtils.getLocalizedString("Cancel");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_YES:
                buttonLabel = PromptUtils.getLocalizedString("Yes");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_NO:
                buttonLabel = PromptUtils.getLocalizedString("No");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_SAVE:
                buttonLabel = PromptUtils.getLocalizedString("Save");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE:
                buttonLabel = PromptUtils.getLocalizedString("DontSave");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_REVERT:
                buttonLabel = PromptUtils.getLocalizedString("Revert");
                break;
              case Ci.nsIPrompt.BUTTON_TITLE_IS_STRING:
                buttonLabel = argText[i];
                break;
            }
            if (buttonLabel)
                buttonLabels[i] = buttonLabel;
            flags >>= 8;
        }

        return [buttonLabels[0], buttonLabels[1], buttonLabels[2], defaultButtonNum, isDelayEnabled];
    },

    getAuthInfo : function (authInfo) {
        let username, password;

        let flags = authInfo.flags;
        if (flags & Ci.nsIAuthInformation.NEED_DOMAIN && authInfo.domain)
            username = authInfo.domain + "\\" + authInfo.username;
        else
            username = authInfo.username;

        password = authInfo.password;

        return [username, password];
    },

    setAuthInfo : function (authInfo, username, password) {
        let flags = authInfo.flags;
        if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
            // Domain is separated from username by a backslash
            let idx = username.indexOf("\\");
            if (idx == -1) {
                authInfo.username = username;
            } else {
                authInfo.domain   =  username.substring(0, idx);
                authInfo.username =  username.substring(idx+1);
            }
        } else {
            authInfo.username = username;
        }
        authInfo.password = password;
    },

    // Copied from login manager
    getFormattedHostname : function (uri) {
        let scheme = uri.scheme;
        let hostname = scheme + "://" + uri.host;

        // If the URI explicitly specified a port, only include it when
        // it's not the default. (We never want "http://foo.com:80")
        port = uri.port;
        if (port != -1) {
            let handler = Services.io.getProtocolHandler(scheme);
            if (port != handler.defaultPort)
                hostname += ":" + port;
        }

        return hostname;
    },

    // Copied from login manager
    getAuthTarget : function (aChannel, aAuthInfo) {
        let hostname, realm;

        // If our proxy is demanding authentication, don't use the
        // channel's actual destination.
        if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
            if (!(aChannel instanceof Ci.nsIProxiedChannel))
                throw "proxy auth needs nsIProxiedChannel";

            let info = aChannel.proxyInfo;
            if (!info)
                throw "proxy auth needs nsIProxyInfo";

            // Proxies don't have a scheme, but we'll use "moz-proxy://"
            // so that it's more obvious what the login is for.
            let idnService = Cc["@mozilla.org/network/idn-service;1"].
                             getService(Ci.nsIIDNService);
            hostname = "moz-proxy://" +
                        idnService.convertUTF8toACE(info.host) +
                        ":" + info.port;
            realm = aAuthInfo.realm;
            if (!realm)
                realm = hostname;

            return [hostname, realm];
        }

        hostname = this.getFormattedHostname(aChannel.URI);

        // If a HTTP WWW-Authenticate header specified a realm, that value
        // will be available here. If it wasn't set or wasn't HTTP, we'll use
        // the formatted hostname instead.
        realm = aAuthInfo.realm;
        if (!realm)
            realm = hostname;

        return [hostname, realm];
    },


    makeAuthMessage : function (channel, authInfo) {
        let isProxy    = (authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY);
        let isPassOnly = (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD);

        let username = authInfo.username;
        let [displayHost, realm] = this.getAuthTarget(channel, authInfo);

        // Suppress "the site says: $realm" when we synthesized a missing realm.
        if (!authInfo.realm && !isProxy)
            realm = "";

        // Trim obnoxiously long realms.
        if (realm.length > 150) {
            realm = realm.substring(0, 150);
            // Append "..." (or localized equivalent).
            realm += this.ellipsis;
        }

        let text;
        if (isProxy)
            text = PromptUtils.getLocalizedString("EnterLoginForProxy", [realm, displayHost]);
        else if (isPassOnly)
            text = PromptUtils.getLocalizedString("EnterPasswordFor", [username, displayHost]);
        else if (!realm)
            text = PromptUtils.getLocalizedString("EnterUserPasswordFor", [displayHost]);
        else
            text = PromptUtils.getLocalizedString("EnterLoginForRealm", [realm, displayHost]);

        return text;
    },

};

XPCOMUtils.defineLazyGetter(PromptUtils, "strBundle", function () {
    let bunService = Cc["@mozilla.org/intl/stringbundle;1"].
                     getService(Ci.nsIStringBundleService);
    let bundle = bunService.createBundle("chrome://global/locale/commonDialogs.properties");
    if (!bundle)
        throw "String bundle for Prompter not present!";
    return bundle;
});

XPCOMUtils.defineLazyGetter(PromptUtils, "ellipsis", function () {
    let ellipsis = "\u2026";
    try {
        ellipsis = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
    } catch (e) { }
    return ellipsis;
});



const COMMON_DIALOG = "chrome://global/content/commonDialog.xul";
const SELECT_DIALOG = "chrome://global/content/selectDialog.xul";

function ModalPrompter(domWin) {
    this.domWin = domWin;
}
ModalPrompter.prototype = {
    domWin : null,

    QueryInterface : XPCOMUtils.generateQI([Ci.nsIPrompt, Ci.nsIAuthPrompt, Ci.nsIAuthPrompt2]),


    /* ---------- internal methods ---------- */


    newPropBag : function () {
        let bag = Cc["@mozilla.org/hash-property-bag;1"].
                  createInstance(Ci.nsIWritablePropertyBag2);
        return bag.QueryInterface(Ci.nsIWritablePropertyBag);
    },

    openPrompt : function (uri, args) {
        // XXX do we want to do modal state if we fall back to .activeWindow?
        let domWin = this.domWin;
        if (!domWin)
            domWin = Services.ww.activeWindow;

        // XXX domWin may still be null here if there are _no_ windows open.

        // Note that we don't need to fire DOMWillOpenModalDialog and
        // DOMModalDialogClosed events here, wwatcher's OpenWindowJSInternal
        // will do that. Similarly for enterModalState / leaveModalState.

        Services.ww.openWindow(domWin, uri, "_blank", "centerscreen,chrome,modal,titlebar", args);
    },



    /*
     * ---------- interface disambiguation ----------
     *
     * nsIPrompt and nsIAuthPrompt share 3 method names with slightly
     * different arguments. All but prompt() have the same number of
     * arguments, so look at the arg types to figure out how we're being
     * called. :-(
     */
    prompt : function() {
        // also, the nsIPrompt flavor has 5 args instead of 6.
        if (typeof arguments[2] == "object")
            return this.nsIPrompt_prompt.apply(this, arguments);
        else
            return this.nsIAuthPrompt_prompt.apply(this, arguments);
    },

    promptUsernameAndPassword : function() {
        // Both have 6 args, so use types.
        if (typeof arguments[2] == "object")
            return this.nsIPrompt_promptUsernameAndPassword.apply(this, arguments);
        else
            return this.nsIAuthPrompt_promptUsernameAndPassword.apply(this, arguments);
    },

    promptPassword : function() {
        // Both have 5 args, so use types.
        if (typeof arguments[2] == "object")
            return this.nsIPrompt_promptPassword.apply(this, arguments);
        else
            return this.nsIAuthPrompt_promptPassword.apply(this, arguments);
    },


    /* ----------  nsIPrompt  ---------- */


    alert : function (title, text) {
        if (!title)
            title = PromptUtils.getLocalizedString("Alert");

        let args = this.newPropBag();
        args.setProperty("promptType", "alert");
        args.setProperty("title", title);
        args.setProperty("text",  text);

        this.openPrompt(COMMON_DIALOG, args);
    },

    alertCheck : function (title, text, checkLabel, checkValue) {
        if (!title)
            title = PromptUtils.getLocalizedString("Alert");

        let args = this.newPropBag();
        args.setProperty("promptType", "alertCheck");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("checkLabel", checkLabel);
        args.setProperty("checked",    checkValue.value);

        this.openPrompt(COMMON_DIALOG, args);

        // Checkbox state always returned, even if cancel clicked.
        checkValue.value = args.getProperty("checked");
    },

    confirm : function (title, text) {
        if (!title)
            title = PromptUtils.getLocalizedString("Confirm");

        let args = this.newPropBag();
        args.setProperty("promptType", "confirm");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("ok",         false);

        this.openPrompt(COMMON_DIALOG, args);

        // Did user click Ok or Cancel?
        return args.getProperty("ok");
    },

    confirmCheck : function (title, text, checkLabel, checkValue) {
        if (!title)
            title = PromptUtils.getLocalizedString("ConfirmCheck");

        let args = this.newPropBag();
        args.setProperty("promptType", "confirmCheck");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("checkLabel", checkLabel);
        args.setProperty("checked",    checkValue.value);
        args.setProperty("ok",         false);

        this.openPrompt(COMMON_DIALOG, args);

        // Checkbox state always returned, even if cancel clicked.
        checkValue.value = args.getProperty("checked");

        // Did user click Ok or Cancel?
        return args.getProperty("ok");
    },

    confirmEx : function (title, text, flags, button0, button1, button2,
                          checkLabel, checkValue) {

        if (!title)
            title = PromptUtils.getLocalizedString("Confirm");

        let args = this.newPropBag();
        args.setProperty("promptType", "confirmEx");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("checkLabel", checkLabel);
        args.setProperty("checked",    checkValue.value);
        args.setProperty("ok",         false);
        args.setProperty("buttonNumClicked", 1);

        let [label0, label1, label2, defaultButtonNum, isDelayEnabled] =
            PromptUtils.confirmExHelper(flags, button0, button1, button2);

        args.setProperty("defaultButtonNum", defaultButtonNum);
        args.setProperty("enableDelay", isDelayEnabled);

        if (label0) {
            args.setProperty("button0Label", label0);
            if (label1) {
                args.setProperty("button1Label", label1);
                if (label2) {
                    args.setProperty("button2Label", label2);
                }
            }
        }

        this.openPrompt(COMMON_DIALOG, args);

        // Checkbox state always returned, even if cancel clicked.
        checkValue.value = args.getProperty("checked");

        // Get the number of the button the user clicked.
        return args.getProperty("buttonNumClicked");
    },

    nsIPrompt_prompt : function (title, text, value, checkLabel, checkValue) {
        if (!title)
            title = PromptUtils.getLocalizedString("Prompt");

        let args = this.newPropBag();
        args.setProperty("promptType", "prompt");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("value",      value.value);
        args.setProperty("checkLabel", checkLabel);
        args.setProperty("checked",    checkValue.value);
        args.setProperty("ok",         false);

        this.openPrompt(COMMON_DIALOG, args);

        // Did user click Ok or Cancel?
        let ok  = args.getProperty("ok");
        if (ok) {
            checkValue.value = args.getProperty("checked");
            value.value      = args.getProperty("value");
        }

        return ok;
    },

    nsIPrompt_promptUsernameAndPassword : function (title, text, user, pass, checkLabel, checkValue) {
        if (!title)
            title = PromptUtils.getLocalizedString("PromptUsernameAndPassword2");

        let args = this.newPropBag();
        args.setProperty("promptType", "promptUserAndPass");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("user",       user.value);
        args.setProperty("pass",       pass.value);
        args.setProperty("checkLabel", checkLabel);
        args.setProperty("checked",    checkValue.value);
        args.setProperty("ok",         false);

        this.openPrompt(COMMON_DIALOG, args);

        // Did user click Ok or Cancel?
        let ok  = args.getProperty("ok");
        if (ok) {
            checkValue.value = args.getProperty("checked");
            user.value       = args.getProperty("user");
            pass.value       = args.getProperty("pass");
        }

        return ok;
    },

    nsIPrompt_promptPassword : function (title, text, pass, checkLabel, checkValue) {
        if (!title)
            title = PromptUtils.getLocalizedString("PromptPassword2");

        let args = this.newPropBag();
        args.setProperty("promptType", "promptPassword");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("pass",       pass.value);
        args.setProperty("checkLabel", checkLabel);
        args.setProperty("checked",    checkValue.value);
        args.setProperty("ok",         false);

        this.openPrompt(COMMON_DIALOG, args);

        // Did user click Ok or Cancel?
        let ok  = args.getProperty("ok");
        if (ok) {
            checkValue.value = args.getProperty("checked");
            pass.value       = args.getProperty("pass");
        }

        return ok;
    },

    select : function (title, text, count, list, selected) {
        if (!title)
            title = PromptUtils.getLocalizedString("Select");

        let args = this.newPropBag();
        args.setProperty("promptType", "select");
        args.setProperty("title",      title);
        args.setProperty("text",       text);
        args.setProperty("list",       list);
        args.setProperty("ok",         false);

        this.openPrompt(SELECT_DIALOG, args);

        // Did user click Ok or Cancel?
        let ok  = args.getProperty("ok");
        if (ok)
            selected.value = args.getProperty("selected");

        return ok;
    },


    /* ----------  nsIAuthPrompt  ---------- */


    nsIAuthPrompt_prompt : function (title, text, passwordRealm, savePassword, defaultText, result) {
        // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
        if (defaultText)
            result.value = defaultText;
        return this.nsIPrompt_prompt(title, text, result, null, {});
    },

    nsIAuthPrompt_promptUsernameAndPassword : function (title, text, passwordRealm, savePassword, user, pass) {
        // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
        return this.nsIPrompt_promptUsernameAndPassword(title, text, user, pass, null, {});
    },

    nsIAuthPrompt_promptPassword : function (title, text, passwordRealm, savePassword, pass) {
        // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
        return this.nsIPrompt_promptPassword(title, text, pass, null, {});
    },


    /* ----------  nsIAuthPrompt2  ---------- */


    promptAuth : function (channel, level, authInfo, checkLabel, checkValue) {
        let message = PromptUtils.makeAuthMessage(channel, authInfo);

        let [username, password] = PromptUtils.getAuthInfo(authInfo);

        let userParam = { value: username };
        let passParam = { value: password };

        let ok;
        if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD)
            ok = this.nsIPrompt_promptPassword(null, message, passParam, checkLabel, checkValue);
        else
            ok = this.nsIPrompt_promptUsernameAndPassword(null, message, userParam, passParam, checkLabel, checkValue);

        if (ok)
            PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
        return ok;
    },

    asyncPromptAuth : function (channel, callback, context, level, authInfo, checkLabel, checkValue) {
        // Nothing calls this directly; netwerk ends up going through
        // nsIPromptService::GetPrompt, which delegates to login manager.
        // Login manger handles the async bits itself, and only calls out
        // promptAuth, never asyncPromptAuth.
        //
        // Bug 565582 will change this.
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },
};


function AuthPromptAdapterFactory() {
}
AuthPromptAdapterFactory.prototype = {
    classID          : Components.ID("{6e134924-6c3a-4d86-81ac-69432dd971dc}"),
    QueryInterface   : XPCOMUtils.generateQI([Ci.nsIAuthPromptAdapterFactory]),

    /* ----------  nsIAuthPromptAdapterFactory ---------- */

    createAdapter : function (oldPrompter) {
        return new AuthPromptAdapter(oldPrompter);
    }
};


// Takes an nsIAuthPrompt implementation, wraps it with a nsIAuthPrompt2 shell.
function AuthPromptAdapter(oldPrompter) {
    this.oldPrompter = oldPrompter;
}
AuthPromptAdapter.prototype = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIAuthPrompt2]),
    oldPrompter    : null,

    /* ----------  nsIAuthPrompt2 ---------- */

    promptAuth : function (channel, level, authInfo, checkLabel, checkValue) {
        let message = PromptUtils.makeAuthMessage(channel, authInfo);

        let [username, password] = PromptUtils.getAuthInfo(authInfo);
        let userParam = { value: username };
        let passParam = { value: password };

        let [host, realm]  = PromptUtils.getAuthTarget(channel, authInfo);
        let authTarget = host + " (" + realm + ")";

        let ok;
        if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD)
            ok = this.oldPrompter.promptPassword(null, message, authTarget, Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY, passParam);
        else
            ok = this.oldPrompter.promptUsernameAndPassword(null, message, authTarget, Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY, userParam, passParam);

        if (ok)
            PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
        return ok;
    },

    asyncPromptAuth : function (channel, callback, context, level, authInfo, checkLabel, checkValue) {
        throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
};


// Wrapper using the old embedding contractID, since it's already common in
// the addon ecosystem.
function EmbedPrompter() {
}
EmbedPrompter.prototype = new Prompter();
EmbedPrompter.prototype.classID          = Components.ID("{7ad1b327-6dfa-46ec-9234-f2a620ea7e00}");

var component = [Prompter, EmbedPrompter, AuthPromptAdapterFactory];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
