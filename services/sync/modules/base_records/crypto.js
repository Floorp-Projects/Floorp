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

Utils.lazy(this, 'CryptoMetas', RecordManager);

// fixme: global, ugh
let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
let crypto = Cc["@labs.mozilla.com/Weave/Crypto;1"].createInstance(Ci.IWeaveCrypto);

function CryptoWrapper(uri, authenticator) {
  this._CryptoWrap_init(uri, authenticator);
}
CryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoWrapper",

  _CryptoWrap_init: function CryptoWrap_init(uri, authenticator) {
    // FIXME: this will add a json filter, meaning our payloads will be json
    //        encoded, even though they are already a string
    this._WBORec_init(uri, authenticator);
    this.data.payload = {
      encryption: "",
      cleartext: null,
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

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);

    let meta = yield CryptoMetas.get(self.cb, this.encryption);
    let symkey = yield meta.getKey(self.cb, privkey, passphrase);

    this.ciphertext = crypto.encrypt(json.encode([this.cleartext]), symkey, meta.bulkIV);
    this.cleartext = null;

    self.done();
  },
  encrypt: function CryptoWrap_encrypt(onComplete, passphrase) {
    this._encrypt.async(this, onComplete, passphrase);
  },

  _decrypt: function CryptoWrap__decrypt(passphrase) {
    let self = yield;

    let pubkey = yield PubKeys.getDefaultKey(self.cb);
    let privkey = yield PrivKeys.get(self.cb, pubkey.privateKeyUri);

    let meta = yield CryptoMetas.get(self.cb, this.encryption);
    let symkey = yield meta.getKey(self.cb, privkey, passphrase);

    // note: payload is wrapped in an array, see _encrypt
    this.cleartext = json.decode(crypto.decrypt(this.ciphertext, symkey, meta.bulkIV))[0];
    this.ciphertext = null;

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
      bulkIV: null,
      keyring: {}
    };
  },

  generateIV: function CryptoMeta_generateIV() {
    this.bulkIV = crypto.generateRandomIV();
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

    let ret = crypto.unwrapSymmetricKey(wrapped_key, privkey.keyData,
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
      crypto.wrapSymmetricKey(symkey, new_pubkey.keyData);
  },
  addUnwrappedKey: function CryptoMeta_addUnwrappedKey(onComplete, new_pubkey, symkey) {
    this._addUnwrappedKey.async(this, onComplete, new_pubkey, symkey);
  }
};

function RecordManager() {
  this._init();
}
RecordManager.prototype = {
  _recordType: CryptoMeta,
  _logName: "RecordMgr",

  _init: function RegordMgr__init() {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this._records = {};
    this._aliases = {};
  },

  _import: function RegordMgr__import(url) {
    let self = yield;
    let rec;

    this._log.trace("Importing record: " + (url.spec? url.spec : url));

    try {
      rec = new this._recordType(url);
      yield rec.get(self.cb);
      this.set(url, rec);
    } catch (e) {
      this._log.debug("Failed to import record: " + e);
      rec = null;
    }
    self.done(rec);
  },
  import: function RegordMgr_import(onComplete, url) {
    this._import.async(this, onComplete, url);
  },

  _get: function RegordMgr__get(url) {
    let self = yield;

    let rec = null;
    if (url in this._aliases)
      url = this._aliases[url];
    if (url in this._records)
      rec = this._records[url];

    if (!rec)
      rec = yield this.import(self.cb, url);

    self.done(rec);
  },
  get: function RegordMgr_get(onComplete, url) {
    this._get.async(this, onComplete, url);
  },

  set: function RegordMgr_set(url, record) {
    this._records[url] = record;
  },

  contains: function RegordMgr_contains(url) {
    let record = null;
    if (url in this._aliases)
      url = this._aliases[url];
    if (url in this._records)
      return true;
    return false;
  },

  del: function RegordMgr_del(url) {
    delete this._records[url];
  },
  getAlias: function RegordMgr_getAlias(alias) {
    return this._aliases[alias];
  },
  setAlias: function RegordMgr_setAlias(url, alias) {
    this._aliases[alias] = url;
  },
  delAlias: function RegordMgr_delAlias(alias) {
    delete this._aliases[alias];
  }
};
