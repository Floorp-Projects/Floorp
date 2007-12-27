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

const EXPORTED_SYMBOLS = ['WeaveCrypto'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");

Function.prototype.async = generatorAsync;

function WeaveCrypto() {
  this._init();
}
WeaveCrypto.prototype = {
  _logName: "Crypto",

  __os: null,
  get _os() {
    if (!this.__os)
      this.__os = Cc["@mozilla.org/observer-service;1"]
        .getService(Ci.nsIObserverService);
    return this.__os;
  },

  __xxtea: {},
  __xxteaLoaded: false,
  get _xxtea() {
    if (!this.__xxteaLoaded) {
      Cu.import("resource://weave/xxtea.js", this.__xxtea);
      this.__xxteaLoaded = true;
    }
    return this.__xxtea;
  },

  get defaultAlgorithm() {
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    return branch.getCharPref("extensions.weave.encryption");
  },
  set defaultAlgorithm(value) {
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch);
    let cur = branch.getCharPref("extensions.weave.encryption");
    if (value != cur)
      branch.setCharPref("extensions.weave.encryption", value);
  },

  _init: function Crypto__init() {
    this._log = Log4Moz.Service.getLogger("Service." + this._logName);
    let branch = Cc["@mozilla.org/preferences-service;1"]
      .getService(Ci.nsIPrefBranch2);
    branch.addObserver("extensions.weave.encryption", this, false);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupports]),

  // nsIObserver

  observe: function Sync_observe(subject, topic, data) {
    switch (topic) {
    case "extensions.weave.encryption":
      let branch = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch);

      let cur = branch.getCharPref("extensions.weave.encryption");
      if (cur == data)
	return;

      switch (data) {
      case "none":
        this._log.info("Encryption disabled");
        break;
      case "XXTEA":
      case "XXXTEA": // Weave 0.1 had this typo
        this._log.info("Using encryption algorithm: " + data);
        break;
      default:
	this._log.warn("Unknown encryption algorithm, resetting");
	branch.setCharPref("extensions.weave.encryption", "XXTEA");
	return; // otherwise we'll send the alg changed event twice
      }
      // FIXME: listen to this bad boy somewhere
      this._os.notifyObservers(null, "weave:encryption:algorithm-changed", "");
      break;
    default:
      this._log.warn("Unknown encryption preference changed - ignoring");
    }
  },

  // Crypto

  PBEencrypt: function Crypto_PBEencrypt(onComplete, data, identity, algorithm) {
    let [self, cont] = yield;
    let listener = new EventListener(cont);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let ret;

    try {
      if (!algorithm)
        algorithm = this.defaultAlgorithm;

      switch (algorithm) {
      case "none":
        ret = data;
      case "XXTEA":
      case "XXXTEA": // Weave 0.1.12.10 and below had this typo
        this._log.debug("Encrypting data");
        let gen = this._xxtea.encrypt(data, identity.password);
        ret = gen.next();
        while (typeof(ret) == "object") {
          timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
          yield; // Yield to main loop
          ret = gen.next();
        }
        gen.close();
        this._log.debug("Done encrypting data");
        break;
      default:
        throw "Unknown encryption algorithm: " + algorithm;
      }

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      timer = null;
      generatorDone(this, self, onComplete, ret);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  },

  PBEdecrypt: function Crypto_PBEdecrypt(onComplete, data, identity, algorithm) {
    let [self, cont] = yield;
    let listener = new EventListener(cont);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let ret;

    try {
      if (!algorithm)
        algorithm = this.defaultAlgorithm;

      switch (algorithm) {
      case "none":
        ret = data;
      case "XXTEA":
      case "XXXTEA": // Weave 0.1.12.10 and below had this typo
        this._log.debug("Decrypting data");
        let gen = this._xxtea.decrypt(data, identity.password);
        ret = gen.next();
        while (typeof(ret) == "object") {
          timer.initWithCallback(listener, 0, timer.TYPE_ONE_SHOT);
          yield; // Yield to main loop
          ret = gen.next();
        }
        gen.close();
        this._log.debug("Done decrypting data");
        break;
      default:
        throw "Unknown encryption algorithm: " + algorithm;
      }

    } catch (e) {
      this._log.error("Exception caught: " + (e.message? e.message : e));

    } finally {
      timer = null;
      generatorDone(this, self, onComplete, ret);
      yield; // onComplete is responsible for closing the generator
    }
    this._log.warn("generator not properly closed");
  }
};
