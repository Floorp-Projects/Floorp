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
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *  Justin Dolske <dolske@mozilla.com>
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

const EXPORTED_SYMBOLS = ['Crypto'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

Function.prototype.async = Async.sugar;

Utils.lazy(this, 'Crypto', CryptoSvc);

function CryptoSvc() {
  this._init();
}
CryptoSvc.prototype = {
  _logName: "Crypto",

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  __crypto: null,
  get _crypto() {
    if (!this.__crypto)
         this.__crypto = Cc["@labs.mozilla.com/Weave/Crypto;1"].
                         createInstance(Ci.IWeaveCrypto);
    return this.__crypto;
  },


  get defaultAlgorithm() {
    return Utils.prefs.getCharPref("encryption");
  },
  set defaultAlgorithm(value) {
    if (value != Utils.prefs.getCharPref("encryption"))
      Utils.prefs.setCharPref("encryption", value);
  },

  _init: function Crypto__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    this._log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.service.crypto")];
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch2);
    branch.addObserver("extensions.weave.encryption", this, false);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupports]),

  // nsIObserver

  observe: function Sync_observe(subject, topic, data) {
    switch (topic) {
    case "extensions.weave.encryption": {
      if (Utils.pref.getCharPref("encryption") == data)
        return;

      switch (data) {
        case "none":
          this._log.info("Encryption disabled");
          break;

        default:
          this._log.warn("Unknown encryption algorithm, resetting");
          branch.clearUserPref("extensions.weave.encryption");
          return; // otherwise we'll send the alg changed event twice
      }
      // FIXME: listen to this bad boy somewhere
      this._os.notifyObservers(null, "weave:encryption:algorithm-changed", "");
    } break;
    default:
      this._log.warn("Unknown encryption preference changed - ignoring");
    }
  },

  checkModule: function Crypto_checkModule() {
    let ok = false;

    try {
      let svc = Cc["@labs.mozilla.com/Weave/Crypto;1"].
        createInstance(Ci.IWeaveCrypto);
      let iv = svc.generateRandomIV();
      if (iv.length == 24)
        ok = true;

    } catch (e) {}

    return ok;
  },

  // Crypto

  encryptData: function Crypto_encryptData(data, identity) {
    let self = yield;
    let ret;

    this._log.trace("encrypt called. [id=" + identity.realm + "]");

    if ("none" == this.defaultAlgorithm) {
      this._log.debug("NOT encrypting data");
      ret = data;
    } else {
      let symkey = identity.bulkKey;
      let iv     = identity.bulkIV;
      ret = this._crypto.encrypt(data, symkey, iv);
    }

    self.done(ret);
  },

  decryptData: function Crypto_decryptData(data, identity) {
    let self = yield;
    let ret;

    this._log.trace("decrypt called. [id=" + identity.realm + "]");

    if ("none" == this.defaultAlgorithm) {
      this._log.debug("NOT decrypting data");
      ret = data;
    } else {
      let symkey = identity.bulkKey;
      let iv     = identity.bulkIV;
      ret = this._crypto.decrypt(data, symkey, iv);
    }

    self.done(ret);
  },

  /*
   * randomKeyGen
   *
   * Generates a random symmetric key and IV, and puts them in the specified
   * identity.
   */
  randomKeyGen: function Crypto_randomKeyGen(identity) {
    let self = yield;

    this._log.trace("randomKeyGen called. [id=" + identity.realm + "]");

    let symkey = this._crypto.generateRandomKey();
    let iv     = this._crypto.generateRandomIV();

    identity.bulkKey = symkey;
    identity.bulkIV  = iv;
  },

  /*
   * RSAkeygen
   *
   * Generates a new RSA keypair, as well as the salt/IV used for protecting
   * the private key, and puts them in the specified identity.
   */
  RSAkeygen: function Crypto_RSAkeygen(identity) {
    let self = yield;

    this._log.trace("RSAkeygen called. [id=" + identity.realm + "]");
    let privOut = {};
    let pubOut  = {};

    // Generate a blob of random data for salting the passphrase used to
    // encrypt the private key.
    let salt = this._crypto.generateRandomBytes(32);
    let iv   = this._crypto.generateRandomIV();

    this._crypto.generateKeypair(identity.password,
                                 salt, iv,
                                 pubOut, privOut);

    identity.keypairAlg     = "RSA";
    identity.pubkey         = pubOut.value;
    identity.privkey        = privOut.value;
    identity.passphraseSalt = salt;
    identity.privkeyWrapIV  = iv;
  },

  wrapKey : function Crypto_wrapKey(data, identity) {
    let self = yield;

    this._log.trace("wrapKey called. [id=" + identity.realm + "]");
    let ret = this._crypto.wrapSymmetricKey(data, identity.pubkey);

    self.done(ret);
  },

  unwrapKey: function Crypto_unwrapKey(data, identity) {
    let self = yield;

    this._log.trace("upwrapKey called. [id=" + identity.realm + "]");
    let ret = this._crypto.unwrapSymmetricKey(data,
                                              identity.privkey,
                                              identity.password,
                                              identity.passphraseSalt,
                                              identity.privkeyWrapIV);
    self.done(ret);
  },

  // This function tests to see if the passphrase which encrypts
  // the private key for the given identity is valid.
  isPassphraseValid: function Crypto_isPassphraseValid(identity) {
    var self = yield;

    // We do this in a somewhat roundabout way; an alternative is
    // to use a hard-coded symmetric key, but even that is still
    // roundabout--ideally, the IWeaveCrypto interface should
    // expose some sort of functionality to make this easier,
    // or perhaps it should just offer this very function. -AV

    // Generate a temporary fake identity.
    var idTemp = {realm: "Temporary passphrase validation"};

    // Generate a random symmetric key.
    this.randomKeyGen.async(Crypto, self.cb, idTemp);
    yield;

    // Encrypt the symmetric key with the user's public key.
    this.wrapKey.async(Crypto, self.cb, idTemp.bulkKey, identity);
    let wrappedKey = yield;
    let unwrappedKey;

    // Decrypt the symmetric key with the user's private key.
    try {
      this.unwrapKey.async(Crypto, self.cb, wrappedKey, identity);
      unwrappedKey = yield;
    } catch (e) {
      self.done(false);
      return;
    }

    // Ensure that the original symmetric key is identical to
    // the decrypted version.
    if (unwrappedKey != idTemp.bulkKey)
      throw new Error("Unwrapped key is not identical to original key.");

    self.done(true);
  }
};
