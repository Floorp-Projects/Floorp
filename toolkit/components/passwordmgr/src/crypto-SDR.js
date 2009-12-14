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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function LoginManagerCrypto_SDR() {
    this.init();
};

LoginManagerCrypto_SDR.prototype = {

    classDescription  : "LoginManagerCrypto_SDR",
    contractID : "@mozilla.org/login-manager/crypto/SDR;1",
    classID : Components.ID("{dc6c2976-0f73-4f1f-b9ff-3d72b4e28309}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerCrypto]),

    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"].
                                getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    __decoderRing : null,  // nsSecretDecoderRing service
    get _decoderRing() {
        if (!this.__decoderRing)
            this.__decoderRing = Cc["@mozilla.org/security/sdr;1"].
                                 getService(Ci.nsISecretDecoderRing);
        return this.__decoderRing;
    },

    __utfConverter : null, // UCS2 <--> UTF8 string conversion
    get _utfConverter() {
        if (!this.__utfConverter) {
            this.__utfConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                                  createInstance(Ci.nsIScriptableUnicodeConverter);
            this.__utfConverter.charset = "UTF-8";
        }
        return this.__utfConverter;
    },

    _utfConverterReset : function() {
        this.__utfConverter = null;
    },

    __observerService : null,
    get _observerService() {
        if (!this.__observerService)
            this.__observerService = Cc["@mozilla.org/observer-service;1"].
                                     getService(Ci.nsIObserverService);
        return this.__observerService;
    },

    _debug : false, // mirrors signon.debug


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console.
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("PwMgr cryptoSDR: " + message + "\n");
        this._logService.logStringMessage("PwMgr cryptoSDR: " + message);
    },


    init : function () {
        // Connect to the correct preferences branch.
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefService);
        this._prefBranch = this._prefBranch.getBranch("signon.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);

        this._debug = this._prefBranch.getBoolPref("debug");

        // Check to see if the internal PKCS#11 token has been initialized.
        // If not, set a blank password.
        let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].
                      getService(Ci.nsIPK11TokenDB);

        let token = tokenDB.getInternalKeyToken();
        if (token.needsUserInit) {
            this.log("Initializing key3.db with default blank password.");
            token.initPassword("");
        }
    },


    /*
     * encrypt
     *
     * Encrypts the specified string, using the SecretDecoderRing.
     *
     * Returns the encrypted string, or throws an exception if there was a
     * problem.
     */
    encrypt : function (plainText) {
        let cipherText = null;

        try {
            let plainOctet = this._utfConverter.ConvertFromUnicode(plainText);
            plainOctet += this._utfConverter.Finish();
            cipherText = this._decoderRing.encryptString(plainOctet);
        } catch (e) {
            this.log("Failed to encrypt string. (" + e.name + ")");
            // If the user clicks Cancel, we get NS_ERROR_FAILURE.
            // (unlike decrypting, which gets NS_ERROR_NOT_AVAILABLE).
            if (e.result == Cr.NS_ERROR_FAILURE)
                throw Components.Exception("User canceled master password entry", Cr.NS_ERROR_ABORT);
            else
                throw Components.Exception("Couldn't encrypt string", Cr.NS_ERROR_FAILURE);
        }
        return cipherText;
    },


    /*
     * decrypt
     *
     * Decrypts the specified string, using the SecretDecoderRing.
     *
     * Returns the decrypted string, or throws an exception if there was a
     * problem.
     */
    decrypt : function (cipherText) {
        let plainText = null;

        try {
            let plainOctet;
            if (cipherText.charAt(0) == '~') {
                // The old Wallet file format obscured entries by
                // base64-encoding them. These entries are signaled by a
                // leading '~' character.
                plainOctet = atob(cipherText.substring(1));
            } else {
                plainOctet = this._decoderRing.decryptString(cipherText);
            }
            plainText = this._utfConverter.ConvertToUnicode(plainOctet);
        } catch (e) {
            this.log("Failed to decrypt string: " + cipherText +
                " (" + e.name + ")");

            // In the unlikely event the converter threw, reset it.
            this._utfConverterReset();

            // If the user clicks Cancel, we get NS_ERROR_NOT_AVAILABLE.
            // If the cipherText is bad / wrong key, we get NS_ERROR_FAILURE
            // Wrong passwords are handled by the decoderRing reprompting;
            // we get no notification.
            if (e.result == Cr.NS_ERROR_NOT_AVAILABLE)
                throw Components.Exception("User canceled master password entry", Cr.NS_ERROR_ABORT);
            else
                throw Components.Exception("Couldn't decrypt string", Cr.NS_ERROR_FAILURE);
        }

        return plainText;
    }
}; // end of nsLoginManagerCrypto_SDR implementation

let component = [LoginManagerCrypto_SDR];
function NSGetModule(compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
