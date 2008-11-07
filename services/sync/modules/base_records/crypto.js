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
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ['CryptoWrapper'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/Observers.js");
Cu.import("resource://weave/Preferences.js");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/crypto.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/keys.js");

Function.prototype.async = Async.sugar;

function CryptoWrapper(uri, authenticator) {
  this._CryptoWrap_init(uri, authenticator);
}
CryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoWrapper",

  _CryptoWrap_init: function CryptoWrap_init(uri, authenticator) {
    this._WBORec_init(uri, authenticator);
    this.data.payload = "";
  },

  _encrypt: function CryptoWrap__encrypt(passphrase) {
    let self = yield;

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);

    let meta = new CryptoMeta(this.data.encryption); // FIXME: cache!
    yield meta.get(self.cb);
    let symkey = meta.getKey(pubkey, privkey, passphrase);

    let crypto = Cc["@labs.mozilla.com/Weave/Crypto;1"].
      createInstance(Ci.IWeaveCrypto);
    this.data.payload = crypto.encrypt(this.cleartext, symkey, meta.iv);

    self.done();
  },
  encrypt: function CryptoWrap_encrypt(onComplete, passphrase) {
    this._encrypt.async(this, onComplete, passphrase);
  },

  _decrypt: function CryptoWrap__decrypt(passphrase) {
    let self = yield;

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);

    let meta = new CryptoMeta(this.data.encryption); // FIXME: cache!
    yield meta.get(self.cb);
    let symkey = meta.getKey(pubkey, privkey, passphrase);

    let crypto = Cc["@labs.mozilla.com/Weave/Crypto;1"].
      createInstance(Ci.IWeaveCrypto);
    this.cleartext = crypto.decrypt(this.data.payload, symkey, meta.iv);

    self.done(this.cleartext);
  },
  decrypt: function CryptoWrap_decrypt(onComplete, passphrase) {
    this._decrypt.async(this, onComplete, passphrase);
  }
};

function CryptoMeta(uri, authenticator) {
  this._CryptoMeta_init(uri, authenticator);
}
CryptoMeta.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoMeta",

  _CryptoMeta_init: function CryptoMeta_init(uri, authenticator) {
    this._WBORec_init(uri, authenticator);
    this.data.payload = {
      salt: null,
      iv: null,
      keyring: {}
    };
  },

  get salt() this.data.payload.salt,
  set salt(value) { this.data.payload.salt = value; },

  get iv() this.data.payload.iv,
  set iv(value) { this.data.payload.iv = value; },

  getKey: function getKey(pubKey, privKey, passphrase) {
    let wrappedKey;

    // get the full uri to our public key
    let pubKeyUri;
    if (typeof pubKey == 'string')
      pubKeyUri = this.uri.resolve(pubKey);
    else
      pubKeyUri = pubKey.spec;

    // each hash key is a relative uri, resolve those and match against ours
    for (let relUri in this.data.payload.keyring) {
      if (pubKeyUri == this.uri.resolve(relUri))
        wrappedKey = this.data.payload.keyring[relUri];
    }
    if (!wrappedKey)
      throw "keyring doesn't contain a key for " + pubKeyUri;

    let crypto = Cc["@labs.mozilla.com/Weave/Crypto;1"].
      createInstance(Ci.IWeaveCrypto);
    return crypto.unwrapSymmetricKey(wrappedKey, privKey.keyData,
                                     passphrase, this.salt, this.iv);
  }
};