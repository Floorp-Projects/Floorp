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

  _openssl: function Crypto__openssl() {
    let extMgr = Cc["@mozilla.org/extensions/manager;1"]
                 .getService(Ci.nsIExtensionManager);
    let loc = extMgr.getInstallLocation("{340c2bbc-ce74-4362-90b5-7c26312808ef}");

    let wrap = loc.getItemLocation("{340c2bbc-ce74-4362-90b5-7c26312808ef}");
    wrap.append("openssl");
    let bin;

    let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
    switch(os) {
    case "WINNT":
      wrap.append("win32");
      wrap.append("exec.bat");
      bin = wrap.parent.path + "\\openssl.exe";
      break;
    case "Linux":
    case "Darwin":
      wrap.append("unix");
      wrap.append("exec.sh");
      bin = "openssl";
      break;
    default:
      throw "encryption not supported on this platform: " + os;
    }

    let args = Array.prototype.slice.call(arguments);
    args.unshift(wrap, Utils.getTmp().path, bin);

    let rv = Utils.runCmd.apply(null, args);
    if (rv != 0)
      throw "openssl did not run successfully, error code " + rv;
  },

  _opensslPBE: function Crypto__openssl(op, algorithm, input, password) {
    let inputFile = Utils.getTmp("input");
    let [inputFOS] = Utils.open(inputFile, ">");
    inputFOS.writeString(input);
    inputFOS.close();

    // nsIProcess doesn't support stdin, so we write a file instead
    let passFile = Utils.getTmp("pass");
    let [passFOS] = Utils.open(passFile, ">", PERMS_PASSFILE);
    passFOS.writeString(password);
    passFOS.close();

    try {
      this._openssl(algorithm, op, "-a", "-salt", "-in", "input",
                    "-out", "output", "-pass", "file:pass");

    } catch (e) {
      throw e;

    } finally {
      passFile.remove(false);
      inputFile.remove(false);
    }

    let outputFile = Utils.getTmp("output");
    let [outputFIS] = Utils.open(outputFile, "<");
    let ret = Utils.readStream(outputFIS);
    outputFIS.close();
    outputFile.remove(false);

    return ret;
  },

  // generates a random string that can be used as a passphrase
  _opensslRand: function Crypto__opensslRand(length) {
    if (!length)
      length = 128;

    let outputFile = Utils.getTmp("output");
    if (outputFile.exists())
      outputFile.remove(false);

    this._openssl("rand", "-base64", "-out", "output", length);

    let [outputFIS] = Utils.open(outputFile, "<");
    let ret = Utils.readStream(outputFIS);
    outputFIS.close();
    outputFile.remove(false);

    return ret;
  },

  // generates an rsa public/private key pair, with the private key encrypted
  _opensslRSAKeyGen: function Crypto__opensslRSAKeyGen(identity, algorithm, bits) {
    if (!algorithm)
      algorithm = "aes-256-cbc";
    if (!bits)
      bits = "2048";

    let privKeyF = Utils.getTmp("privkey.pem");
    if (privKeyF.exists())
      privKeyF.remove(false);

    this._openssl("genrsa", "-out", "privkey.pem", bits);

    let pubKeyF = Utils.getTmp("pubkey.pem");
    if (pubKeyF.exists())
      pubKeyF.remove(false);

    this._openssl("rsa", "-in", "privkey.pem", "-out", "pubkey.pem",
                  "-outform", "PEM", "-pubout");

    let cryptedKeyF = Utils.getTmp("enckey.pem");
    if (cryptedKeyF.exists())
      cryptedKeyF.remove(false);

    // nsIProcess doesn't support stdin, so we write a file instead
    let passFile = Utils.getTmp("pass");
    let [passFOS] = Utils.open(passFile, ">", PERMS_PASSFILE);
    passFOS.writeString(identity.password);
    passFOS.close();

    try {
      this._openssl("pkcs8", "-in", "privkey.pem", "-out", "enckey.pem",
                    "-topk8", "-v2", algorithm, "-passout", "file:pass");

    } catch (e) {
      throw e;

    } finally {
      passFile.remove(false);
      privKeyF.remove(false);
    }

    let [cryptedKeyFIS] = Utils.open(cryptedKeyF, "<");
    let cryptedKey = Utils.readStream(cryptedKeyFIS);
    cryptedKeyFIS.close();
    cryptedKeyF.remove(false);

    let [pubKeyFIS] = Utils.open(pubKeyF, "<");
    let pubKey = Utils.readStream(pubKeyFIS);
    pubKeyFIS.close();
    pubKeyF.remove(false);

    return [cryptedKey, pubKey];
  },

  // returns 'input' encrypted with the 'pubkey' public RSA key
  _opensslRSAencrypt: function Crypto__opensslRSAencrypt(input, identity) {
    let inputFile = Utils.getTmp("input");
    let [inputFOS] = Utils.open(inputFile, ">");
    inputFOS.writeString(input);
    inputFOS.close();

    let keyFile = Utils.getTmp("key");
    let [keyFOS] = Utils.open(keyFile, ">");
    keyFOS.writeString(identity.pubkey);
    keyFOS.close();

    let tmpFile = Utils.getTmp("tmp-output");
    if (tmpFile.exists())
      tmpFile.remove(false);

    let outputFile = Utils.getTmp("output");
    if (outputFile.exists())
      outputFile.remove(false);

    this._openssl("rsautl", "-encrypt", "-pubin", "-inkey", "key",
                  "-in", "input", "-out", "tmp-output");
    this._openssl("base64", "-in", "tmp-output", "-out", "output");

    let [outputFIS] = Utils.open(outputFile, "<");
    let output = Utils.readStream(outputFIS);
    outputFIS.close();
    outputFile.remove(false);

    return output;
  },

  // returns 'input' decrypted with the 'privkey' private RSA key and password
  _opensslRSAdecrypt: function Crypto__opensslRSAdecrypt(input, identity) {
    let inputFile = Utils.getTmp("input");
    let [inputFOS] = Utils.open(inputFile, ">");
    inputFOS.writeString(input);
    inputFOS.close();

    let keyFile = Utils.getTmp("key");
    let [keyFOS] = Utils.open(keyFile, ">");
    keyFOS.writeString(identity.privkey);
    keyFOS.close();

    let tmpKeyFile = Utils.getTmp("tmp-key");
    if (tmpKeyFile.exists())
      tmpKeyFile.remove(false);

    let tmpFile = Utils.getTmp("tmp-output");
    if (tmpFile.exists())
      tmpFile.remove(false);

    let outputFile = Utils.getTmp("output");
    if (outputFile.exists())
      outputFile.remove(false);

    // nsIProcess doesn't support stdin, so we write a file instead
    let passFile = Utils.getTmp("pass");
    let [passFOS] = Utils.open(passFile, ">", PERMS_PASSFILE);
    passFOS.writeString(identity.password);
    passFOS.close();

    try {
      this._openssl("base64", "-d", "-in", "input", "-out", "tmp-output");
      // FIXME: this is because openssl.exe (in windows only) doesn't
      // seem to support -passin for rsautl, but it works for rsa.
      this._openssl("rsa", "-in", "key", "-out", "tmp-key", "-passin", "file:pass");
      this._openssl("rsautl", "-decrypt", "-inkey", "tmp-key",
                    "-in", "tmp-output", "-out", "output");

    } catch(e) {
      throw e;

    } finally {
      passFile.remove(false);
      tmpKeyFile.remove(false);
      tmpFile.remove(false);
      keyFile.remove(false);
    }

    let [outputFIS] = Utils.open(outputFile, "<");
    let output = Utils.readStream(outputFIS);
    outputFIS.close();
    outputFile.remove(false);

    return output;
  },

  // returns the public key from the private key
  _opensslRSAkeydecrypt: function Crypto__opensslRSAkeydecrypt(identity) {
    let keyFile = Utils.getTmp("key");
    let [keyFOS] = Utils.open(keyFile, ">");
    keyFOS.writeString(identity.privkey);
    keyFOS.close();

    let outputFile = Utils.getTmp("output");
    if (outputFile.exists())
      outputFile.remove(false);

    // nsIProcess doesn't support stdin, so we write a file instead
    let passFile = Utils.getTmp("pass");
    let [passFOS] = Utils.open(passFile, ">", PERMS_PASSFILE);
    passFOS.writeString(identity.password);
    passFOS.close();

    try {
      this._openssl("rsa", "-in", "key", "-pubout", "-out", "output",
                    "-passin", "file:pass");

    } catch(e) {
      throw e;

    } finally {
      passFile.remove(false);
    }

    let [outputFIS] = Utils.open(outputFile, "<");
    let output = Utils.readStream(outputFIS);
    outputFIS.close();
    outputFile.remove(false);

    return output;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupports]),

  // nsIObserver

  observe: function Sync_observe(subject, topic, data) {
    switch (topic) {
    case "extensions.weave.encryption": {
      let branch = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch);

      let cur = branch.getCharPref("extensions.weave.encryption");
      if (cur == data)
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

  // Crypto

  PBEencrypt: function Crypto_PBEencrypt(data, identity, algorithm) {
    let self = yield;
    let ret;

    if (!algorithm)
      algorithm = this.defaultAlgorithm;

    if (algorithm != "none")
      this._log.debug("Encrypting data");

    switch (algorithm) {
    case "none":
      ret = data;
      break;

    case "aes-128-cbc":
    case "aes-192-cbc":
    case "aes-256-cbc":
    case "bf-cbc":
    case "des-ede3-cbc":
      ret = this._opensslPBE("-e", algorithm, data, identity.password);
      break;

    default:
      throw "Unknown encryption algorithm: " + algorithm;
    }

    if (algorithm != "none")
      this._log.debug("Done encrypting data");

    self.done(ret);
  },

  PBEdecrypt: function Crypto_PBEdecrypt(data, identity, algorithm) {
    let self = yield;
    let ret;

    if (!algorithm)
      algorithm = this.defaultAlgorithm;

    if (algorithm != "none")
      this._log.debug("Decrypting data");

    switch (algorithm) {
    case "none":
      ret = data;
      break;

    case "aes-128-cbc":
    case "aes-192-cbc":
    case "aes-256-cbc":
    case "bf-cbc":
    case "des-ede3-cbc":
      ret = this._opensslPBE("-d", algorithm, data, identity.password);
      break;

    default:
      throw "Unknown encryption algorithm: " + algorithm;
    }

    if (algorithm != "none")
      this._log.debug("Done decrypting data");

    self.done(ret);
  },

  PBEkeygen: function Crypto_PBEkeygen() {
    let self = yield;
    let ret = this._opensslRand();
    self.done(ret);
  },

  RSAkeygen: function Crypto_RSAkeygen(identity) {
    let self = yield;
    let ret = this._opensslRSAKeyGen(identity);
    self.done(ret);
  },

  RSAencrypt: function Crypto_RSAencrypt(data, identity) {
    let self = yield;
    let ret = this._opensslRSAencrypt(data, identity);
    self.done(ret);
  },

  RSAdecrypt: function Crypto_RSAdecrypt(data, identity) {
    let self = yield;
    let ret = this._opensslRSAdecrypt(data, identity);
    self.done(ret);
  },

  RSAkeydecrypt: function Crypto_RSAkeydecrypt(identity) {
    let self = yield;
    let ret = this._opensslRSAkeydecrypt(identity);
    self.done(ret);
  }
};
