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

const EXPORTED_SYMBOLS = ['CryptoWrapper', 'CryptoMeta', 'CryptoMetas'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/keys.js");

Function.prototype.async = Async.sugar;

function CryptoWrapper(uri) {
  this._CryptoWrap_init(uri);
}
CryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoWrapper",

  _CryptoWrap_init: function CryptoWrap_init(uri) {
    // FIXME: this will add a json filter, meaning our payloads will be json
    //        encoded, even though they are already a string
    this._WBORec_init(uri);
    this.data.payload = {
      encryption: "",
      ciphertext: null
    };
  },

  // FIXME: we make no attempt to ensure cleartext is in sync
  //        with the encrypted payload
  cleartext: null,

  get encryption() this.payload.encryption,
  set encryption(value) {
    this.payload.encryption = value;
  },

  get ciphertext() this.payload.ciphertext,
  set ciphertext(value) {
    this.payload.ciphertext = value;
  },

  _encrypt: function CryptoWrap__encrypt(passphrase) {
    let self = yield;

    // Don't encrypt empty payloads
    if (!this.cleartext) {
      this.ciphertext = this.cleartext;
      self.done();
      return;
    }

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);

    let meta = yield CryptoMetas.get(self.cb, this.encryption);
    let symkey = yield meta.getKey(self.cb, privkey, passphrase);

    this.ciphertext = Svc.Crypto.encrypt(Svc.Json.encode([this.cleartext]),
					 symkey, meta.bulkIV);
    this.cleartext = null;

    self.done();
  },
  encrypt: function CryptoWrap_encrypt(onComplete, passphrase) {
    this._encrypt.async(this, onComplete, passphrase);
  },

  _decrypt: function CryptoWrap__decrypt(passphrase) {
    let self = yield;

    // Empty payloads aren't encrypted
    if (!this.ciphertext) {
      this.cleartext = this.ciphertext;
      self.done();
      return;
    }

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);

    let meta = yield CryptoMetas.get(self.cb, this.encryption);
    let symkey = yield meta.getKey(self.cb, privkey, passphrase);

    // note: payload is wrapped in an array, see _encrypt
    this.cleartext = Svc.Json.decode(Svc.Crypto.decrypt(this.ciphertext,
							symkey, meta.bulkIV))[0];
    this.ciphertext = null;

    self.done(this.cleartext);
  },
  decrypt: function CryptoWrap_decrypt(onComplete, passphrase) {
    this._decrypt.async(this, onComplete, passphrase);
  },

  toString: function WBORec_toString() {
    return "{ id: " + this.id + "\n" +
      "  parent: " + this.parentid + "\n" +
      "  depth: " + this.depth + ", index: " + this.sortindex + "\n" +
      "  modified: " + this.modified + "\n" +
      "  payload: " + Svc.Json.encode(this.cleartext) + " }";
  }
};

function CryptoMeta(uri) {
  this._CryptoMeta_init(uri);
}
CryptoMeta.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoMeta",

  _CryptoMeta_init: function CryptoMeta_init(uri) {
    this._WBORec_init(uri);
    this.data.payload = {
      bulkIV: null,
      keyring: {}
    };
  },

  generateIV: function CryptoMeta_generateIV() {
    this.bulkIV = Svc.Crypto.generateRandomIV();
  },

  get bulkIV() this.data.payload.bulkIV,
  set bulkIV(value) { this.data.payload.bulkIV = value; },

  _getKey: function CryptoMeta__getKey(privkey, passphrase) {
    let self = yield;
    let wrapped_key;

    // get the uri to our public key
    let pubkeyUri = privkey.publicKeyUri.spec;

    // each hash key is a relative uri, resolve those and match against ours
    for (let relUri in this.payload.keyring) {
      if (pubkeyUri == this.uri.resolve(relUri))
        wrapped_key = this.payload.keyring[relUri];
    }
    if (!wrapped_key)
      throw "keyring doesn't contain a key for " + pubkeyUri;

    let ret = Svc.Crypto.unwrapSymmetricKey(wrapped_key, privkey.keyData,
                                        passphrase, privkey.salt, privkey.iv);
    self.done(ret);
  },
  getKey: function CryptoMeta_getKey(onComplete, privkey, passphrase) {
    this._getKey.async(this, onComplete, privkey, passphrase);
  },

  _addKey: function CryptoMeta__addKey(new_pubkey, privkey, passphrase) {
    let self = yield;
    let symkey = yield this.getKey(self.cb, privkey, passphrase);
    yield this.addUnwrappedKey(self.cb, new_pubkey, symkey);
  },
  addKey: function CryptoMeta_addKey(onComplete, new_pubkey, privkey, passphrase) {
    this._addKey.async(this, onComplete, new_pubkey, privkey, passphrase);
  },

  _addUnwrappedKey: function CryptoMeta__addUnwrappedKey(new_pubkey, symkey) {
    let self = yield;

    // get the new public key
    if (typeof new_pubkey == 'string')
      new_pubkey = PubKeys.get(self.cb, new_pubkey);

    // each hash key is a relative uri, resolve those and
    // if we find the one we're about to add, remove it
    for (let relUri in this.payload.keyring) {
      if (pubkeyUri == this.uri.resolve(relUri))
        delete this.payload.keyring[relUri];
    }

    this.payload.keyring[new_pubkey.uri.spec] =
      Svc.Crypto.wrapSymmetricKey(symkey, new_pubkey.keyData);
  },
  addUnwrappedKey: function CryptoMeta_addUnwrappedKey(onComplete, new_pubkey, symkey) {
    this._addUnwrappedKey.async(this, onComplete, new_pubkey, symkey);
  }
};

Utils.lazy(this, 'CryptoMetas', CryptoRecordManager);

function CryptoRecordManager() { this._init(); }
CryptoRecordManager.prototype = {
  __proto__: RecordManager.prototype,
  _recordType: CryptoMeta
};