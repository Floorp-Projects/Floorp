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

const EXPORTED_SYMBOLS = ['PubKey', 'PrivKey',
                          'PubKeys', 'PrivKeys'];

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

Function.prototype.async = Async.sugar;

Utils.lazy(this, 'PubKeys', PubKeyManager);
Utils.lazy(this, 'PrivKeys', PrivKeyManager);

function PubKey(uri, authenticator) {
  this._PubKey_init(uri, authenticator);
}
PubKey.prototype = {
  __proto__: WBORecord.prototype,

  _PubKey_init: function PubKey_init(uri, authenticator) {
    this._WBORec_init(uri, authenticator);
  },

  get keyData() {
    if (!this.data)
      return null;
    return this.data.payload.key_data;
  },

  get privateKeyUri() {
    if (!this.data)
      return null;
    // resolve
    let uri = this.uri.resolve(this.data.payload.private_key);
    if (uri)
      return Utils.makeURI(uri);
    // does not resolve, return raw (this uri type might not be able to resolve)
    return Utils.makeURI(this.data.payload.private_key);
  }
};

function PrivKey(uri, authenticator) {
  this._PrivKey_init(uri, authenticator);
}
PrivKey.prototype = {
  __proto__: WBORecord.prototype,

  _PrivKey_init: function PrivKey_init(uri, authenticator) {
    this._WBORec_init(uri, authenticator);
  },

  get keyData() {
    if (!this.data)
      return null;
    return this.data.payload.key_data;
  },

  get privateKeyUri() {
    throw "attempted to get private key url from a private key!";
  },

  get publicKeyUri() {
    if (!this.data)
      return null;
    // resolve
    let uri = this.uri.resolve(this.data.payload.public_key);
    if (uri)
      return Utils.makeURI(uri);
    // does not resolve, return raw (this uri type might not be able to resolve)
    return Utils.makeURI(this.data.payload.public_key);
  }
};

function SymKey(keyData, wrapped) {
  this._init(keyData, wrapped);
}
SymKey.prototype = {
  get wrapped() {
    return this._wrapped;
  },

  _init: function SymKey__init(keyData, wrapped) {
    this._data = keyData;
    this._wrapped = wrapped;
  },

  unwrap: function SymKey_unwrap(privkey, passphrase, meta_record) {
    let svc = Cc["@labs.mozilla.com/Weave/Crypto;1"].
      createInstance(Ci.IWeaveCrypto);
    this._data = svc.unwrapSymmetricKey(this._data,
                                        privkey.keyData,
                                        passphrase,
                                        identity.passphraseSalt,
                                        identity.privkeyWrapIV);
  }
};

function PubKeyManager() {
  this._init();
}
PubKeyManager.prototype = {
  _keyType: PubKey,
  _logName: "PubKeyManager",

  get defaultKeyUri() this._defaultKeyUrl,
  set defaultKeyUri(value) { this._defaultKeyUri = value; },

  _getDefaultKey: function KeyMgr__getDefaultKey() {
    let self = yield;
    let ret = yield this.get(self.cb, this.defaultKeyUri);
    self.done(ret);
  },
  getDefaultKey: function KeyMgr_getDefaultKey(onComplete) {
    return this._getDefaultKey.async(this, onComplete);
  },

  _init: function KeyMgr__init() {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this._keys = {};
    this._aliases = {};
  },

  _import: function KeyMgr__import(url) {
    let self = yield;
    let ret = null;

    this._log.trace("Importing key: " + (url.spec? url.spec : url));

    try {
      let key = new this._keyType(url);
      let foo = yield key.get(self.cb);
      this.set(url, key);
      ret = key;
    } catch (e) {
      this._log.debug("Failed to import key: " + e);
      // don't do anything else, we'll just return null
    }

    self.done(ret);
  },
  import: function KeyMgr_import(onComplete, url) {
    this._import.async(this, onComplete, url);
  },

  _get: function KeyMgr__get(url) {
    let self = yield;

    let key = null;
    if (url in this._aliases)
      url = this._aliases[url];
    if (url in this._keys)
      key = this._keys[url];

    if (!key)
      key = yield this.import(self.cb, url);

    self.done(key);
  },
  get: function KeyMgr_get(onComplete, url) {
    this._get.async(this, onComplete, url);
  },

  set: function KeyMgr_set(url, key) {
    this._keys[url] = key;
    return key;
  },

  contains: function KeyMgr_contains(url) {
    let key = null;
    if (url in this._aliases)
      url = this._aliases[url];
    if (url in this._keys)
      return true;
    return false;
  },

  del: function KeyMgr_del(url) {
    delete this._keys[url];
  },
  getAlias: function KeyMgr_getAlias(alias) {
    return this._aliases[alias];
  },
  setAlias: function KeyMgr_setAlias(url, alias) {
    this._aliases[alias] = url;
  },
  delAlias: function KeyMgr_delAlias(alias) {
    delete this._aliases[alias];
  }
};

function PrivKeyManager() { this._init(); }
PrivKeyManager.prototype = {
  __proto__: PubKeyManager.prototype,
  _keyType: PrivKey,
  _logName: "PrivKeyManager"
};
