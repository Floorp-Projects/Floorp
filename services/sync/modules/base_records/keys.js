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

Cu.import("resource://services-sync/base_records/wbo.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");

function PubKey(uri) {
  WBORecord.call(this, uri);
  this.type = "pubkey";
  this.keyData = null;
}
PubKey.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.PubKey",

  get privateKeyUri() {
    if (!this.data)
      return null;

    // Use the uri if it resolves, otherwise return raw (uri type unresolvable)
    let key = this.payload.privateKeyUri;
    return Utils.makeURI(this.uri.resolve(key) || key);
  },
  set privateKeyUri(value) {
    this.payload.privateKeyUri = this.uri.getRelativeSpec(Utils.makeURI(value));
  },

  get publicKeyUri() {
    throw "attempted to get public key url from a public key!";
  }
};

Utils.deferGetSet(PubKey, "payload", ["keyData", "type"]);

function PrivKey(uri) {
  WBORecord.call(this, uri);
  this.type = "privkey";
  this.salt = null;
  this.iv = null;
  this.keyData = null;
}
PrivKey.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.PrivKey",

  get publicKeyUri() {
    if (!this.data)
      return null;

    // Use the uri if it resolves, otherwise return raw (uri type unresolvable)
    let key = this.payload.publicKeyUri;
    return Utils.makeURI(this.uri.resolve(key) || key);
  },
  set publicKeyUri(value) {
    this.payload.publicKeyUri = this.uri.getRelativeSpec(Utils.makeURI(value));
  },

  get privateKeyUri() {
    throw "attempted to get private key url from a private key!";
  }
};

Utils.deferGetSet(PrivKey, "payload", ["salt", "iv", "keyData", "type"]);

// XXX unused/unfinished
function SymKey(keyData, wrapped) {
  this._data = keyData;
  this._wrapped = wrapped;
}
SymKey.prototype = {
  get wrapped() {
    return this._wrapped;
  },

  unwrap: function SymKey_unwrap(privkey, passphrase, meta_record) {
    this._data =
      Svc.Crypto.unwrapSymmetricKey(this._data, privkey.keyData, passphrase,
                                    privkey.salt, privkey.iv);
  }
};

Utils.lazy(this, 'PubKeys', PubKeyManager);

function PubKeyManager() {
  RecordManager.call(this);
}
PubKeyManager.prototype = {
  __proto__: RecordManager.prototype,
  _recordType: PubKey,
  _logName: "PubKeyManager",

  get defaultKeyUri() this._defaultKeyUri,
  set defaultKeyUri(value) { this._defaultKeyUri = value; },

  getDefaultKey: function PubKeyManager_getDefaultKey() {
    return this.get(this.defaultKeyUri);
  },

  createKeypair: function KeyMgr_createKeypair(passphrase, pubkeyUri, privkeyUri) {
    if (!pubkeyUri)
      throw "Missing or null parameter 'pubkeyUri'.";
    if (!privkeyUri)
      throw "Missing or null parameter 'privkeyUri'.";

    this._log.debug("Generating RSA keypair");
    let pubkey = new PubKey(pubkeyUri);
    let privkey = new PrivKey(privkeyUri);
    privkey.salt = Svc.Crypto.generateRandomBytes(16);
    privkey.iv = Svc.Crypto.generateRandomIV();

    let pub = {}, priv = {};
    Svc.Crypto.generateKeypair(passphrase.passwordUTF8, privkey.salt,
                               privkey.iv, pub, priv);
    [pubkey.keyData, privkey.keyData] = [pub.value, priv.value];

    pubkey.privateKeyUri = privkeyUri;
    privkey.publicKeyUri = pubkeyUri;

    this._log.debug("Generating RSA keypair... done");
    return {pubkey: pubkey, privkey: privkey};
  },

  uploadKeypair: function PubKeyManager_uploadKeypair(keys) {
    for each (let key in keys)
      new Resource(key.uri).put(key);
  }
};

Utils.lazy(this, 'PrivKeys', PrivKeyManager);

function PrivKeyManager() {
  PubKeyManager.call(this);
}
PrivKeyManager.prototype = {
  __proto__: PubKeyManager.prototype,
  _recordType: PrivKey,
  _logName: "PrivKeyManager"
};
