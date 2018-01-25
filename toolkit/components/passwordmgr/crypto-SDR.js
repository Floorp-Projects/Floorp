/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
                                  "resource://gre/modules/LoginHelper.jsm");

function LoginManagerCrypto_SDR() {
  this.init();
}

LoginManagerCrypto_SDR.prototype = {

  classID: Components.ID("{dc6c2976-0f73-4f1f-b9ff-3d72b4e28309}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsILoginManagerCrypto]),

  __decoderRing: null,  // nsSecretDecoderRing service
  get _decoderRing() {
    if (!this.__decoderRing)
      this.__decoderRing = Cc["@mozilla.org/security/sdr;1"].
                           getService(Ci.nsISecretDecoderRing);
    return this.__decoderRing;
  },

  __utfConverter: null, // UCS2 <--> UTF8 string conversion
  get _utfConverter() {
    if (!this.__utfConverter) {
      this.__utfConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                            createInstance(Ci.nsIScriptableUnicodeConverter);
      this.__utfConverter.charset = "UTF-8";
    }
    return this.__utfConverter;
  },

  _utfConverterReset() {
    this.__utfConverter = null;
  },

  _uiBusy: false,


  init() {
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
  encrypt(plainText) {
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
  decrypt(cipherText) {
    let plainText = null;

    let wasLoggedIn = this.isLoggedIn;
    let canceledMP = false;

    this._uiBusy = true;
    try {
      let plainOctet;
      plainOctet = this._decoderRing.decryptString(cipherText);
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
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].
                  getService(Ci.nsIPK11TokenDB);
    let token = tokenDB.getInternalKeyToken();
    return !token.hasPassword || token.isLoggedIn();
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
  _notifyObservers(topic) {
    this.log("Prompted for a master password, notifying for " + topic);
    Services.obs.notifyObservers(null, topic);
  },
}; // end of nsLoginManagerCrypto_SDR implementation

XPCOMUtils.defineLazyGetter(this.LoginManagerCrypto_SDR.prototype, "log", () => {
  let logger = LoginHelper.createLogger("Login crypto");
  return logger.log.bind(logger);
});

var component = [LoginManagerCrypto_SDR];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
