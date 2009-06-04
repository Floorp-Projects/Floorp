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

Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/resource.js");
Cu.import("resource://weave/base_records/wbo.js");

Function.prototype.async = Async.sugar;

function PubKey(uri) {
  this._PubKey_init(uri);
}
PubKey.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.PubKey",

  _PubKey_init: function PubKey_init(uri) {
    this._WBORec_init(uri);
    this.payload = {
      type: "pubkey",
      keyData: null,
      privateKeyUri: null
    };
  },

  get privateKeyUri() {
    if (!this.data)
      return null;

    // Use the uri if it resolves, otherwise return raw (uri type unresolvable)
    let key = this.payload.privateKeyUri;
    return Utils.makeURI(this.uri.resolve(key) || key);
  },

  get publicKeyUri() {
    throw "attempted to get public key url from a public key!";
  }
};

Utils.deferGetSet(PubKey, "payload", ["keyData", "privateKeyUri"]);

function PrivKey(uri) {
  this._PrivKey_init(uri);
}
PrivKey.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.PrivKey",

  _PrivKey_init: function PrivKey_init(uri) {
    this._WBORec_init(uri);
    this.payload = {
      type: "privkey",
      salt: null,
      iv: null,
      keyData: null,
      publicKeyUri: null
    };
  },

  get publicKeyUri() {
    if (!this.data)
      return null;

    // Use the uri if it resolves, otherwise return raw (uri type unresolvable)
    let key = this.payload.publicKeyUri;
    return Utils.makeURI(this.uri.resolve(key) || key);
  },

  get privateKeyUri() {
    throw "attempted to get private key url from a private key!";
  }
};

Utils.deferGetSet(PrivKey, "payload", ["salt", "iv", "keyData", "publicKeyUri"]);

// XXX unused/unfinished
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
    this._data =
      Svc.Crypto.unwrapSymmetricKey(this._data, privkey.keyData, passphrase,
                                    privkey.salt, privkey.iv);
  }
};

Utils.lazy(this, 'PubKeys', PubKeyManager);

function PubKeyManager() { this._init(); }
PubKeyManager.prototype = {
  __proto__: RecordManager.prototype,
  _recordType: PubKey,
  _logName: "PubKeyManager",

  get defaultKeyUri() this._defaultKeyUri,
  set defaultKeyUri(value) { this._defaultKeyUri = value; },

  getDefaultKey: function KeyMgr_getDefaultKey(onComplete) {
    let fn = function KeyMgr__getDefaultKey() {
      let self = yield;
      let ret = this.get(this.defaultKeyUri);
      self.done(ret);
    };
    fn.async(this, onComplete);
  },

  createKeypair: function KeyMgr_createKeypair(passphrase, pubkeyUri, privkeyUri) {
    this._log.debug("Generating RSA keypair");
    let pubkey = new PubKey();
    let privkey = new PrivKey();
    privkey.salt = Svc.Crypto.generateRandomBytes(16);
    privkey.iv = Svc.Crypto.generateRandomIV();

    let pub = {}, priv = {};
    Svc.Crypto.generateKeypair(passphrase, privkey.salt, privkey.iv, pub, priv);
    [pubkey.keyData, privkey.keyData] = [pub.value, priv.value];

    if (pubkeyUri) {
      pubkey.uri = pubkeyUri;
      privkey.publicKeyUri = pubkeyUri;
    }
    if (privkeyUri) {
      privkey.uri = privkeyUri;
      pubkey.privateKeyUri = privkeyUri;
    }

    this._log.debug("Generating RSA keypair... done");
    return {pubkey: pubkey, privkey: privkey};
  },

  uploadKeypair: function KeyMgr_uploadKeypair(onComplete, keys) {
    let fn = function KeyMgr__uploadKeypair(keys) {
      let self = yield;
      for each (let key in keys) {
	let res = new Resource(key.uri);
	res.put(key.serialize());
      }
    };
    fn.async(this, onComplete, keys);
  }
};

Utils.lazy(this, 'PrivKeys', PrivKeyManager);

function PrivKeyManager() { this._init(); }
PrivKeyManager.prototype = {
  __proto__: PubKeyManager.prototype,
  _recordType: PrivKey,
  _logName: "PrivKeyManager"
};
