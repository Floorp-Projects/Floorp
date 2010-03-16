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

Cu.import("resource://weave/util.js");
Cu.import("resource://weave/base_records/wbo.js");
Cu.import("resource://weave/base_records/keys.js");

function CryptoWrapper(uri) {
  this.cleartext = {};
  WBORecord.call(this, uri);
  this.encryption = "";
  this.ciphertext = null;
}
CryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoWrapper",

  encrypt: function CryptoWrapper_encrypt(passphrase) {
    let pubkey = PubKeys.getDefaultKey();
    let privkey = PrivKeys.get(pubkey.privateKeyUri);

    let meta = CryptoMetas.get(this.encryption);
    let symkey = meta.getKey(privkey, passphrase);

    this.ciphertext = Svc.Crypto.encrypt(JSON.stringify(this.cleartext),
					 symkey, meta.bulkIV);
    this.cleartext = null;
  },

  decrypt: function CryptoWrapper_decrypt(passphrase) {
    let pubkey = PubKeys.getDefaultKey();
    let privkey = PrivKeys.get(pubkey.privateKeyUri);

    let meta = CryptoMetas.get(this.encryption);
    let symkey = meta.getKey(privkey, passphrase);

    this.cleartext = JSON.parse(Svc.Crypto.decrypt(this.ciphertext,
							symkey, meta.bulkIV));
    this.ciphertext = null;

    // Verify that the encrypted id matches the requested record's id
    if (this.cleartext.id != this.id)
      throw "Server attack?! Id mismatch: " + [this.cleartext.id, this.id];

    return this.cleartext;
  },

  toString: function CryptoWrap_toString() "{ " + [
      "id: " + this.id,
      "index: " + this.sortindex,
      "modified: " + this.modified,
      "payload: " + (this.deleted ? "DELETED" : JSON.stringify(this.cleartext))
    ].join("\n  ") + " }",

  // The custom setter below masks the parent's getter, so explicitly call it :(
  get id() WBORecord.prototype.__lookupGetter__("id").call(this),

  // Keep both plaintext and encrypted versions of the id to verify integrity
  set id(val) {
    WBORecord.prototype.__lookupSetter__("id").call(this, val);
    return this.cleartext.id = val;
  },
};

Utils.deferGetSet(CryptoWrapper, "payload", ["encryption", "ciphertext"]);
Utils.deferGetSet(CryptoWrapper, "cleartext", "deleted");

function CryptoMeta(uri) {
  WBORecord.call(this, uri);
  this.bulkIV = null;
  this.keyring = {};
}
CryptoMeta.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoMeta",

  generateIV: function CryptoMeta_generateIV() {
    this.bulkIV = Svc.Crypto.generateRandomIV();
  },

  getKey: function CryptoMeta_getKey(privkey, passphrase) {
    // We cache the unwrapped key, as it's expensive to generate
    if (this._unwrappedKey)
      return this._unwrappedKey;

    // get the uri to our public key
    let pubkeyUri = privkey.publicKeyUri.spec;

    // each hash key is a relative uri, resolve those and match against ours
    let wrapped_key;
    for (let relUri in this.keyring) {
      if (pubkeyUri == this.baseUri.resolve(relUri))
        wrapped_key = this.keyring[relUri];
    }
    if (!wrapped_key)
      throw "keyring doesn't contain a key for " + pubkeyUri;

    return this._unwrappedKey = Svc.Crypto.unwrapSymmetricKey(wrapped_key,
      privkey.keyData, passphrase.password, privkey.salt, privkey.iv);
  },

  addKey: function CryptoMeta_addKey(new_pubkey, privkey, passphrase) {
    let symkey = this.getKey(privkey, passphrase);
    this.addUnwrappedKey(new_pubkey, symkey);
  },

  addUnwrappedKey: function CryptoMeta_addUnwrappedKey(new_pubkey, symkey) {
    // get the new public key
    if (typeof new_pubkey == "string")
      new_pubkey = PubKeys.get(new_pubkey);

    // each hash key is a relative uri, resolve those and
    // if we find the one we're about to add, remove it
    for (let relUri in this.keyring) {
      if (pubkeyUri == this.uri.resolve(relUri))
        delete this.keyring[relUri];
    }

    this.keyring[new_pubkey.uri.spec] =
      Svc.Crypto.wrapSymmetricKey(symkey, new_pubkey.keyData);
  }
};

Utils.deferGetSet(CryptoMeta, "payload", ["bulkIV", "keyring"]);

Utils.lazy(this, 'CryptoMetas', CryptoRecordManager);

function CryptoRecordManager() {
  RecordManager.call(this);
}
CryptoRecordManager.prototype = {
  __proto__: RecordManager.prototype,
  _recordType: CryptoMeta
};
