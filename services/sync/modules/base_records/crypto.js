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

Cu.import("resource://services-sync/base_records/keys.js");
Cu.import("resource://services-sync/base_records/wbo.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/util.js");

function CryptoWrapper(uri) {
  this.cleartext = {};
  WBORecord.call(this, uri);
  this.ciphertext = null;
}
CryptoWrapper.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoWrapper",

  get encryption() {
    return this.uri.resolve(this.payload.encryption);
  },
  set encryption(value) {
    this.payload.encryption = this.uri.getRelativeSpec(Utils.makeURI(value));
  },

  encrypt: function CryptoWrapper_encrypt(passphrase) {
    let pubkey = PubKeys.getDefaultKey();
    let privkey = PrivKeys.get(pubkey.privateKeyUri);

    let meta = CryptoMetas.get(this.encryption);
    let symkey = meta.getKey(privkey, passphrase);

    this.IV = Svc.Crypto.generateRandomIV();
    this.ciphertext = Svc.Crypto.encrypt(JSON.stringify(this.cleartext),
                                         symkey, this.IV);
    this.hmac = Utils.sha256HMAC(this.ciphertext, symkey.hmacKey);
    this.cleartext = null;
  },

  decrypt: function CryptoWrapper_decrypt(passphrase, keyUri) {
    let pubkey = PubKeys.getDefaultKey();
    let privkey = PrivKeys.get(pubkey.privateKeyUri);

    let meta = CryptoMetas.get(keyUri);
    let symkey = meta.getKey(privkey, passphrase);

    // Authenticate the encrypted blob with the expected HMAC
    if (Utils.sha256HMAC(this.ciphertext, symkey.hmacKey) != this.hmac)
      throw "Record SHA256 HMAC mismatch: " + this.hmac;

    this.cleartext = JSON.parse(Svc.Crypto.decrypt(this.ciphertext, symkey,
                                                   this.IV));
    this.ciphertext = null;

    // Verify that the encrypted id matches the requested record's id
    if (this.cleartext.id != this.id)
      throw "Record id mismatch: " + [this.cleartext.id, this.id];

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

Utils.deferGetSet(CryptoWrapper, "payload", ["ciphertext", "IV", "hmac"]);
Utils.deferGetSet(CryptoWrapper, "cleartext", "deleted");

function CryptoMeta(uri) {
  WBORecord.call(this, uri);
  this.keyring = {};
}
CryptoMeta.prototype = {
  __proto__: WBORecord.prototype,
  _logName: "Record.CryptoMeta",

  getWrappedKey: function _getWrappedKey(privkey) {
    // get the uri to our public key
    let pubkeyUri = privkey.publicKeyUri.spec;

    // each hash key is a relative uri, resolve those and match against ours
    for (let relUri in this.keyring) {
      if (pubkeyUri == this.baseUri.resolve(relUri))
        return this.keyring[relUri];
    }
    return null;
  },

  getKey: function CryptoMeta_getKey(privkey, passphrase) {
    let wrapped_key = this.getWrappedKey(privkey);
    if (!wrapped_key)
      throw "keyring doesn't contain a key for " + privkey.publicKeyUri.spec;

    // Make sure the wrapped key hasn't been tampered with
    let localHMAC = Utils.sha256HMAC(wrapped_key.wrapped, this.hmacKey);
    if (localHMAC != wrapped_key.hmac)
      throw "Key SHA256 HMAC mismatch: " + wrapped_key.hmac;

    // Decrypt the symmetric key and make it a String object to add properties
    let unwrappedKey = new String(
      Svc.Crypto.unwrapSymmetricKey(
        wrapped_key.wrapped,
        privkey.keyData,
        passphrase.passwordUTF8,
        privkey.salt,
        privkey.iv
      )
    );

    unwrappedKey.hmacKey = Svc.KeyFactory.keyFromString(Ci.nsIKeyObject.HMAC,
      unwrappedKey);

    // Cache the result after the first get and just return it
    return (this.getKey = function() unwrappedKey)();
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
      if (new_pubkey.uri.spec == this.uri.resolve(relUri))
        delete this.keyring[relUri];
    }

    // Wrap the symmetric key and generate a HMAC for it
    let wrapped = Svc.Crypto.wrapSymmetricKey(symkey, new_pubkey.keyData);
    this.keyring[this.uri.getRelativeSpec(new_pubkey.uri)] = {
      wrapped: wrapped,
      hmac: Utils.sha256HMAC(wrapped, this.hmacKey)
    };
  },

  get hmacKey() {
    let passphrase = ID.get("WeaveCryptoID").passwordUTF8;
    return Svc.KeyFactory.keyFromString(Ci.nsIKeyObject.HMAC, passphrase);
  }
};

Utils.deferGetSet(CryptoMeta, "payload", "keyring");

Utils.lazy(this, 'CryptoMetas', CryptoRecordManager);

function CryptoRecordManager() {
  RecordManager.call(this);
}
CryptoRecordManager.prototype = {
  __proto__: RecordManager.prototype,
  _recordType: CryptoMeta
};
