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
Components.utils.import("resource://gre/modules/Services.jsm");

function LoginManagerCrypto_SDR() {
    this.init();
};

LoginManagerCrypto_SDR.prototype = {

    classID : Components.ID("{dc6c2976-0f73-4f1f-b9ff-3d72b4e28309}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsILoginManagerCrypto]),

    __sdrSlot : null, // PKCS#11 slot being used by the SDR.
    get _sdrSlot() {
        if (!this.__sdrSlot) {
            let modules = Cc["@mozilla.org/security/pkcs11moduledb;1"].
                          getService(Ci.nsIPKCS11ModuleDB);
            this.__sdrSlot = modules.findSlotByName("");
        }
        return this.__sdrSlot;
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

    _debug  : false, // mirrors signon.debug
    _uiBusy : false,


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console.
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("PwMgr cryptoSDR: " + message + "\n");
        Services.console.logStringMessage("PwMgr cryptoSDR: " + message);
    },


    init : function () {
        // Connect to the correct preferences branch.
        this._prefBranch = Services.prefs.getBranch("signon.");

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

        let wasLoggedIn = this.isLoggedIn;
        let canceledMP = false;

        this._uiBusy = true;
        try {
            let plainOctet = this._utfConverter.ConvertFromUnicode(plainText);
            plainOctet += this._utfConverter.Finish();
            cipherText = this._decoderRing.encryptString(plainOctet);
        } catch (e) {
            this.log("Failed to encrypt string. (" + e.name + ")");
            // If the user clicks Cancel, we get NS_ERROR_FAILURE.
            // (unlike decrypting, which gets NS_ERROR_NOT_AVAILABLE).
            if (e.result == Cr.NS_ERROR_FAILURE) {
                canceledMP = true;
                throw Components.Exception("User canceled master password entry", Cr.NS_ERROR_ABORT);
            } else {
                throw Components.Exception("Couldn't encrypt string", Cr.NS_ERROR_FAILURE);
            }
        } finally {
            this._uiBusy = false;
            // If we triggered a master password prompt, notify observers.
            if (!wasLoggedIn && this.isLoggedIn)
                this._notifyObservers("passwordmgr-crypto-login");
            else if (canceledMP)
                this._notifyObservers("passwordmgr-crypto-loginCanceled");
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

        let wasLoggedIn = this.isLoggedIn;
        let canceledMP = false;

        this._uiBusy = true;
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
            if (e.result == Cr.NS_ERROR_NOT_AVAILABLE) {
                canceledMP = true;
                throw Components.Exception("User canceled master password entry", Cr.NS_ERROR_ABORT);
            } else {
                throw Components.Exception("Couldn't decrypt string", Cr.NS_ERROR_FAILURE);
            }
        } finally {
            this._uiBusy = false;
            // If we triggered a master password prompt, notify observers.
            if (!wasLoggedIn && this.isLoggedIn)
                this._notifyObservers("passwordmgr-crypto-login");
            else if (canceledMP)
                this._notifyObservers("passwordmgr-crypto-loginCanceled");
        }

        return plainText;
    },


    /*
     * uiBusy
     */
    get uiBusy() {
        return this._uiBusy;
    },


    /*
     * isLoggedIn
     */
    get isLoggedIn() {
        let status = this._sdrSlot.status;
        this.log("SDR slot status is " + status);
        if (status == Ci.nsIPKCS11Slot.SLOT_READY ||
            status == Ci.nsIPKCS11Slot.SLOT_LOGGED_IN)
            return true;
        if (status == Ci.nsIPKCS11Slot.SLOT_NOT_LOGGED_IN)
            return false;
        throw Components.Exception("unexpected slot status: " + status, Cr.NS_ERROR_FAILURE);
    },


    /*
     * defaultEncType
     */
    get defaultEncType() {
        return Ci.nsILoginManagerCrypto.ENCTYPE_SDR;
    },


    /*
     * _notifyObservers
     */
    _notifyObservers : function(topic) {
        this.log("Prompted for a master password, notifying for " + topic);
        Services.obs.notifyObservers(null, topic, null);
     },
}; // end of nsLoginManagerCrypto_SDR implementation

let component = [LoginManagerCrypto_SDR];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
