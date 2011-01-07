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
 *  Richard Newman <rnewman@mozilla.com>
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

const EXPORTED_SYMBOLS = ['Utils', 'Svc', 'Str'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/ext/Observers.js");
Cu.import("resource://services-sync/ext/Preferences.js");
Cu.import("resource://services-sync/ext/StringBundle.js");
Cu.import("resource://services-sync/ext/Sync.js");
Cu.import("resource://services-sync/log4moz.js");

/*
 * Utility functions
 */

let Utils = {
  /**
   * Execute an arbitrary number of asynchronous functions one after the
   * other, passing the callback arguments on to the next one.  All functions
   * must take a callback function as their last argument.  The 'this' object
   * will be whatever asyncChain's is.
   * 
   * @usage this._chain = Utils.asyncChain;
   *        this._chain(this.foo, this.bar, this.baz)(args, for, foo)
   * 
   * This is equivalent to:
   *
   *   let self = this;
   *   self.foo(args, for, foo, function (bars, args) {
   *     self.bar(bars, args, function (baz, params) {
   *       self.baz(baz, params);
   *     });
   *   });
   */
  asyncChain: function asyncChain() {
    let funcs = Array.slice(arguments);
    let thisObj = this;
    return function callback() {
      if (funcs.length) {
        let args = Array.slice(arguments).concat(callback);
        let f = funcs.shift();
        f.apply(thisObj, args);
      }
    };
  },

  /**
   * Wrap a function to catch all exceptions and log them
   *
   * @usage MyObj._catch = Utils.catch;
   *        MyObj.foo = function() { this._catch(func)(); }
   */
  catch: function Utils_catch(func) {
    let thisArg = this;
    return function WrappedCatch() {
      try {
        return func.call(thisArg);
      }
      catch(ex) {
        thisArg._log.debug("Exception: " + Utils.exceptionStr(ex));
      }
    };
  },

  /**
   * Wrap a function to call lock before calling the function then unlock.
   *
   * @usage MyObj._lock = Utils.lock;
   *        MyObj.foo = function() { this._lock(func)(); }
   */
  lock: function lock(label, func) {
    let thisArg = this;
    return function WrappedLock() {
      if (!thisArg.lock()) {
        throw "Could not acquire lock. Label: \"" + label + "\".";
      }

      try {
        return func.call(thisArg);
      }
      finally {
        thisArg.unlock();
      }
    };
  },
  
  isLockException: function isLockException(ex) {
    return ex && (ex.indexOf("Could not acquire lock.") == 0);
  },

  /**
   * Wrap functions to notify when it starts and finishes executing or if it got
   * an error. The message is a combination of a provided prefix and local name
   * with the current state and the subject is the provided subject.
   *
   * @usage function MyObj() { this._notify = Utils.notify("prefix:"); }
   *        MyObj.foo = function() { this._notify(name, subject, func)(); }
   */
  notify: function Utils_notify(prefix) {
    return function NotifyMaker(name, subject, func) {
      let thisArg = this;
      let notify = function(state) {
        let mesg = prefix + name + ":" + state;
        thisArg._log.trace("Event: " + mesg);
        Observers.notify(mesg, subject);
      };

      return function WrappedNotify() {
        try {
          notify("start");
          let ret = func.call(thisArg);
          notify("finish");
          return ret;
        }
        catch(ex) {
          notify("error");
          throw ex;
        }
      };
    };
  },

  batchSync: function batchSync(service, engineType) {
    return function batchedSync() {
      let engine = this;
      let batchEx = null;

      // Try running sync in batch mode
      Svc[service].runInBatchMode({
        runBatched: function wrappedSync() {
          try {
            engineType.prototype._sync.call(engine);
          }
          catch(ex) {
            batchEx = ex;
          }
        }
      }, null);

      // Expose the exception if something inside the batch failed
      if (batchEx!= null)
        throw batchEx;
    };
  },
  
  createStatement: function createStatement(db, query) {
    // Gecko 2.0
    if (db.createAsyncStatement)
      return db.createAsyncStatement(query);

    // Gecko <2.0
    return db.createStatement(query);
  },

  queryAsync: function(query, names) {
    // Allow array of names, single name, and no name
    if (!Utils.isArray(names))
      names = names == null ? [] : [names];

    // Synchronously asyncExecute fetching all results by name
    let [exec, execCb] = Sync.withCb(query.executeAsync, query);
    return exec({
      items: [],
      handleResult: function handleResult(results) {
        let row;
        while ((row = results.getNextRow()) != null) {
          this.items.push(names.reduce(function(item, name) {
            item[name] = row.getResultByName(name);
            return item;
          }, {}));
        }
      },
      handleError: function handleError(error) {
        execCb.throw(error);
      },
      handleCompletion: function handleCompletion(reason) {
        execCb(this.items);
      }
    });
  },

  byteArrayToString: function byteArrayToString(bytes) {
    return [String.fromCharCode(byte) for each (byte in bytes)].join("");
  },

  /**
   * Generate a string of random bytes.
   */
  generateRandomBytes: function generateRandomBytes(length) {
    let rng = Cc["@mozilla.org/security/random-generator;1"]
                .createInstance(Ci.nsIRandomGenerator);
    let bytes = rng.generateRandomBytes(length);
    return Utils.byteArrayToString(bytes);
  },

  /**
   * Encode byte string as base64url (RFC 4648).
   */
  encodeBase64url: function encodeBase64url(bytes) {
    return btoa(bytes).replace('+', '-', 'g').replace('/', '_', 'g');
  },

  /**
   * GUIDs are 9 random bytes encoded with base64url (RFC 4648).
   * That makes them 12 characters long with 72 bits of entropy.
   */
  makeGUID: function makeGUID() {
    return Utils.encodeBase64url(Utils.generateRandomBytes(9));
  },

  anno: function anno(id, anno, val, expire) {
    // Figure out if we have a bookmark or page
    let annoFunc = (typeof id == "number" ? "Item" : "Page") + "Annotation";

    // Convert to a nsIURI if necessary
    if (typeof id == "string")
      id = Utils.makeURI(id);

    if (id == null)
      throw "Null id for anno! (invalid uri)";

    switch (arguments.length) {
      case 2:
        // Get the annotation with 2 args
        return Svc.Annos["get" + annoFunc](id, anno);
      case 3:
        expire = "NEVER";
        // Fallthrough!
      case 4:
        // Convert to actual EXPIRE value
        expire = Svc.Annos["EXPIRE_" + expire];

        // Set the annotation with 3 or 4 args
        return Svc.Annos["set" + annoFunc](id, anno, val, 0, expire);
    }
  },

  ensureOneOpen: let (windows = {}) function ensureOneOpen(window) {
    // Close the other window if it exists
    let url = window.location.href;
    let other = windows[url];
    if (other != null)
      other.close();

    // Save the new window for future closure
    windows[url] = window;

    // Actively clean up when the window is closed
    window.addEventListener("unload", function() windows[url] = null, false);
  },

  // Returns a nsILocalFile representing a file relative to the
  // current user's profile directory.  If the argument is a string,
  // it should be a string with unix-style slashes for directory names
  // (these slashes are automatically converted to platform-specific
  // path separators).
  //
  // Alternatively, if the argument is an object, it should contain
  // the following attributes:
  //
  //   path: the path to the file, relative to the current user's
  //   profile dir.
  //
  //   autoCreate: whether or not the file should be created if it
  //   doesn't already exist.
  getProfileFile: function getProfileFile(arg) {
    if (typeof arg == "string")
      arg = {path: arg};

    let pathParts = arg.path.split("/");
    let file = Svc.Directory.get("ProfD", Ci.nsIFile);
    file.QueryInterface(Ci.nsILocalFile);
    for (let i = 0; i < pathParts.length; i++)
      file.append(pathParts[i]);
    if (arg.autoCreate && !file.exists())
      file.create(file.NORMAL_FILE_TYPE, PERMS_FILE);
    return file;
  },

  /**
   * Add a simple getter/setter to an object that defers access of a property
   * to an inner property.
   *
   * @param obj
   *        Object to add properties to defer in its prototype
   * @param defer
   *        Hash property of obj to defer to (dot split each level)
   * @param prop
   *        Property name to defer (or an array of property names)
   */
  deferGetSet: function Utils_deferGetSet(obj, defer, prop) {
    if (Utils.isArray(prop))
      return prop.map(function(prop) Utils.deferGetSet(obj, defer, prop));

    // Split the defer into each dot part for each level to dereference
    let parts = defer.split(".");
    let deref = function(base) Utils.deref(base, parts);

    let prot = obj.prototype;

    // Create a getter if it doesn't exist yet
    if (!prot.__lookupGetter__(prop)) {
      // Yes, this should be a one-liner, but there are errors if it's not
      // broken out. *sigh*
      // Errors are these:
      // JavaScript strict warning: resource://services-sync/util.js, line 304: reference to undefined property deref(this)[prop]
      // JavaScript strict warning: resource://services-sync/util.js, line 304: reference to undefined property deref(this)[prop]
      let f = function() {
        let d = deref(this);
        if (!d)
          return undefined;
        let out = d[prop];
        return out;
      }
      prot.__defineGetter__(prop, f);
    }

    // Create a setter if it doesn't exist yet
    if (!prot.__lookupSetter__(prop))
      prot.__defineSetter__(prop, function(val) deref(this)[prop] = val);
  },

  /**
   * Dereference an array of properties starting from a base object
   *
   * @param base
   *        Base object to start dereferencing
   * @param props
   *        Array of properties to dereference (one for each level)
   */
  deref: function Utils_deref(base, props) props.reduce(function(curr, prop)
    curr[prop], base),

  /**
   * Determine if some value is an array
   *
   * @param val
   *        Value to check (can be null, undefined, etc.)
   * @return True if it's an array; false otherwise
   */
  isArray: function Utils_isArray(val) val != null && typeof val == "object" &&
    val.constructor.name == "Array",

  // lazy load objects from a constructor on first access.  It will
  // work with the global object ('this' in the global context).
  lazy: function Weave_lazy(dest, prop, ctr) {
    delete dest[prop];
    dest.__defineGetter__(prop, Utils.lazyCb(dest, prop, ctr));
  },
  lazyCb: function Weave_lazyCb(dest, prop, ctr) {
    return function() {
      delete dest[prop];
      dest[prop] = new ctr();
      return dest[prop];
    };
  },

  // like lazy, but rather than new'ing the 3rd arg we use its return value
  lazy2: function Weave_lazy2(dest, prop, fn) {
    delete dest[prop];
    dest.__defineGetter__(prop, Utils.lazyCb2(dest, prop, fn));
  },
  lazyCb2: function Weave_lazyCb2(dest, prop, fn) {
    return function() {
      delete dest[prop];
      return dest[prop] = fn();
    };
  },

  lazySvc: function Weave_lazySvc(dest, prop, cid, iface) {
    let getter = function() {
      delete dest[prop];
      let svc = null;

      // Use the platform's service if it exists
      if (cid in Cc && iface in Ci)
        svc = Cc[cid].getService(Ci[iface]);
      else {
        svc = FakeSvc[cid];

        let log = Log4Moz.repository.getLogger("Service.Util");
        if (svc == null)
          log.warn("Component " + cid + " doesn't exist on this platform.");
        else
          log.debug("Using a fake svc object for " + cid);
      }

      return dest[prop] = svc;
    };
    dest.__defineGetter__(prop, getter);
  },

  lazyStrings: function Weave_lazyStrings(name) {
    let bundle = "chrome://weave/locale/services/" + name + ".properties";
    return function() new StringBundle(bundle);
  },

  deepEquals: function eq(a, b) {
    // If they're triple equals, then it must be equals!
    if (a === b)
      return true;

    // If they weren't equal, they must be objects to be different
    if (typeof a != "object" || typeof b != "object")
      return false;

    // But null objects won't have properties to compare
    if (a === null || b === null)
      return false;

    // Make sure all of a's keys have a matching value in b
    for (let k in a)
      if (!eq(a[k], b[k]))
        return false;

    // Do the same for b's keys but skip those that we already checked
    for (let k in b)
      if (!(k in a) && !eq(a[k], b[k]))
        return false;

    return true;
  },

  deepCopy: function Weave_deepCopy(thing, noSort) {
    if (typeof(thing) != "object" || thing == null)
      return thing;
    let ret;

    if (Utils.isArray(thing)) {
      ret = [];
      for (let i = 0; i < thing.length; i++)
        ret.push(Utils.deepCopy(thing[i], noSort));

    } else {
      ret = {};
      let props = [p for (p in thing)];
      if (!noSort)
        props = props.sort();
      props.forEach(function(k) ret[k] = Utils.deepCopy(thing[k], noSort));
    }

    return ret;
  },

  // Works on frames or exceptions, munges file:// URIs to shorten the paths
  // FIXME: filename munging is sort of hackish, might be confusing if
  // there are multiple extensions with similar filenames
  formatFrame: function Utils_formatFrame(frame) {
    let tmp = "<file:unknown>";

    let file = frame.filename || frame.fileName;
    if (file)
      tmp = file.replace(/^(?:chrome|file):.*?([^\/\.]+\.\w+)$/, "$1");

    if (frame.lineNumber)
      tmp += ":" + frame.lineNumber;
    if (frame.name)
      tmp = frame.name + "()@" + tmp;

    return tmp;
  },

  exceptionStr: function Weave_exceptionStr(e) {
    let message = e.message ? e.message : e;
    return message + " " + Utils.stackTrace(e);
  },

  stackTraceFromFrame: function Weave_stackTraceFromFrame(frame) {
    let output = [];
    while (frame) {
      let str = Utils.formatFrame(frame);
      if (str)
        output.push(str);
      frame = frame.caller;
    }
    return output.join(" < ");
  },

  stackTrace: function Weave_stackTrace(e) {
    // Wrapped nsIException
    if (e.location)
      return "Stack trace: " + Utils.stackTraceFromFrame(e.location);

    // Standard JS exception
    if (e.stack)
      return "JS Stack trace: " + e.stack.trim().replace(/\n/g, " < ").
        replace(/@[^@]*?([^\/\.]+\.\w+:)/g, "@$1");

    return "No traceback available";
  },
  
  // Generator and discriminator for HMAC exceptions.
  // Split these out in case we want to make them richer in future, and to 
  // avoid inevitable confusion if the message changes.
  throwHMACMismatch: function throwHMACMismatch(shouldBe, is) {
    throw "Record SHA256 HMAC mismatch: should be " + shouldBe + ", is " + is;
  },
  
  isHMACMismatch: function isHMACMismatch(ex) {
    const hmacFail = "Record SHA256 HMAC mismatch: ";
    return ex && ex.indexOf && (ex.indexOf(hmacFail) == 0);
  },
  
  checkStatus: function Weave_checkStatus(code, msg, ranges) {
    if (!ranges)
      ranges = [[200,300]];

    for (let i = 0; i < ranges.length; i++) {
      var rng = ranges[i];
      if (typeof(rng) == "object" && code >= rng[0] && code < rng[1])
        return true;
      else if (typeof(rng) == "number" && code == rng) {
        return true;
      }
    }

    if (msg) {
      let log = Log4Moz.repository.getLogger("Service.Util");
      log.error(msg + " Error code: " + code);
    }

    return false;
  },

  ensureStatus: function Weave_ensureStatus(args) {
    if (!Utils.checkStatus.apply(Utils, arguments))
      throw 'checkStatus failed';
  },

  digest: function digest(message, hasher) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
      createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let data = converter.convertToByteArray(message, {});
    hasher.update(data, data.length);
    return hasher.finish(false);
  },

  bytesAsHex: function bytesAsHex(bytes) {
    // Convert each hashed byte into 2-hex strings then combine them
    return [("0" + byte.charCodeAt().toString(16)).slice(-2)
            for each (byte in bytes)].join("");
  },

  _sha256: function _sha256(message) {
    let hasher = Cc["@mozilla.org/security/hash;1"].
      createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA256);
    return Utils.digest(message, hasher);
  },

  sha256: function sha256(message) {
    return Utils.bytesAsHex(Utils._sha256(message));
  },

  sha256Base64: function (message) {
    return btoa(Utils._sha256(message));
  },

  _sha1: function _sha1(message) {
    let hasher = Cc["@mozilla.org/security/hash;1"].
      createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);
    return Utils.digest(message, hasher);
  },

  sha1: function sha1(message) {
    return Utils.bytesAsHex(Utils._sha1(message));
  },

  sha1Base32: function sha1Base32(message) {
    return Utils.encodeBase32(Utils._sha1(message));
  },
  
  /**
   * Produce an HMAC key object from a key string.
   */
  makeHMACKey: function makeHMACKey(str) {
    return Svc.KeyFactory.keyFromString(Ci.nsIKeyObject.HMAC, str);
  },
    
  /**
   * Produce an HMAC hasher.
   */
  makeHMACHasher: function makeHMACHasher() {
    return Cc["@mozilla.org/security/hmac;1"]
             .createInstance(Ci.nsICryptoHMAC);
  },

  sha1Base64: function (message) {
    return btoa(Utils._sha1(message));
  },

  /**
   * Generate a sha1 HMAC for a message, not UTF-8 encoded,
   * and a given nsIKeyObject.
   * Optionally provide an existing hasher, which will be 
   * initialized and reused.
   */
  sha1HMACBytes: function sha1HMACBytes(message, key, hasher) {
    let h = hasher || this.makeHMACHasher();
    h.init(h.SHA1, key);
    
    // No UTF-8 encoding for you, sunshine.
    let bytes = [b.charCodeAt() for each (b in message)];
    h.update(bytes, bytes.length);
    return h.finish(false);
  },
  
  /**
   * Generate a sha256 HMAC for a string message and a given nsIKeyObject.
   * Optionally provide an existing hasher, which will be
   * initialized and reused.
   *
   * Returns hex output.
   */
  sha256HMAC: function sha256HMAC(message, key, hasher) {
    let h = hasher || this.makeHMACHasher();
    h.init(h.SHA256, key);
    return Utils.bytesAsHex(Utils.digest(message, h));
  },
  
  
  /**
   * Generate a sha256 HMAC for a string message, not UTF-8 encoded,
   * and a given nsIKeyObject.
   * Optionally provide an existing hasher, which will be
   * initialized and reused.
   */
  sha256HMACBytes: function sha256HMACBytes(message, key, hasher) {
    let h = hasher || this.makeHMACHasher();
    h.init(h.SHA256, key);

    // No UTF-8 encoding for you, sunshine.
    let bytes = [b.charCodeAt() for each (b in message)];
    h.update(bytes, bytes.length);
    return h.finish(false);
  },

  /**
   * HMAC-based Key Derivation Step 2 according to RFC 5869.
   */
  hkdfExpand: function hkdfExpand(prk, info, len) {
    const BLOCKSIZE = 256 / 8;
    let h = Utils.makeHMACHasher();
    let T = "";
    let Tn = "";
    let iterations = Math.ceil(len/BLOCKSIZE);
    for (let i = 0; i < iterations; i++) {
      Tn = Utils.sha256HMACBytes(Tn + info + String.fromCharCode(i + 1),
                                 Utils.makeHMACKey(prk), h);
      T += Tn;
    }
    return T.slice(0, len);
  },

  byteArrayToString: function byteArrayToString(bytes) {
    return [String.fromCharCode(byte) for each (byte in bytes)].join("");
  },
  
  /**
   * PBKDF2 implementation in Javascript.
   * 
   * The arguments to this function correspond to items in 
   * PKCS #5, v2.0 pp. 9-10 
   * 
   * P: the passphrase, an octet string:              e.g., "secret phrase"
   * S: the salt, an octet string:                    e.g., "DNXPzPpiwn"
   * c: the number of iterations, a positive integer: e.g., 4096
   * dkLen: the length in octets of the destination 
   *        key, a positive integer:                  e.g., 16
   *        
   * The output is an octet string of length dkLen, which you
   * can encode as you wish.
   */
  pbkdf2Generate : function pbkdf2Generate(P, S, c, dkLen) {
    
    // We don't have a default in the algo itself, as NSS does.
    // Use the constant.
    if (!dkLen)
      dkLen = SYNC_KEY_DECODED_LENGTH;
    
    /* For HMAC-SHA-1 */
    const HLEN = 20;
    
    function F(PK, S, c, i, h) {
    
      function XOR(a, b, isA) {
        if (a.length != b.length) {
          return false;
        }

        let val = [];
        for (let i = 0; i < a.length; i++) {
          if (isA) {
            val[i] = a[i] ^ b[i];
          } else {
            val[i] = a.charCodeAt(i) ^ b.charCodeAt(i);
          }
        }

        return val;
      }
    
      let ret;
      let U = [];

      /* Encode i into 4 octets: _INT */
      let I = [];
      I[0] = String.fromCharCode((i >> 24) & 0xff);
      I[1] = String.fromCharCode((i >> 16) & 0xff);
      I[2] = String.fromCharCode((i >> 8) & 0xff);
      I[3] = String.fromCharCode(i & 0xff);

      U[0] = Utils.sha1HMACBytes(S + I.join(''), PK, h);
      for (let j = 1; j < c; j++) {
        U[j] = Utils.sha1HMACBytes(U[j - 1], PK, h);
      }

      ret = U[0];
      for (j = 1; j < c; j++) {
        ret = Utils.byteArrayToString(XOR(ret, U[j]));
      }

      return ret;
    }
    
    let l = Math.ceil(dkLen / HLEN);
    let r = dkLen - ((l - 1) * HLEN);

    // Reuse the key and the hasher. Remaking them 4096 times is 'spensive.
    let PK = Utils.makeHMACKey(P);
    let h = Utils.makeHMACHasher();
    
    T = [];
    for (let i = 0; i < l;) {
      T[i] = F(PK, S, c, ++i, h);
    }

    let ret = '';
    for (i = 0; i < l-1;) {
      ret += T[i++];
    }
    ret += T[l - 1].substr(0, r);

    return ret;
  },


  /**
   * Base32 decode (RFC 4648) a string.
   */
  decodeBase32: function decodeBase32(str) {
    const key = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    let padChar = str.indexOf("=");
    let chars = (padChar == -1) ? str.length : padChar;
    let bytes = Math.floor(chars * 5 / 8);
    let blocks = Math.ceil(chars / 8);

    // Process a chunk of 5 bytes / 8 characters.
    // The processing of this is known in advance,
    // so avoid arithmetic!
    function processBlock(ret, cOffset, rOffset) {
      let c, val;

      // N.B., this relies on
      //   undefined | foo == foo.
      function accumulate(val) {
        ret[rOffset] |= val;
      }

      function advance() {
        c  = str[cOffset++];
        if (!c || c == "" || c == "=") // Easier than range checking.
          throw "Done";                // Will be caught far away.
        val = key.indexOf(c);
        if (val == -1)
          throw "Unknown character in base32: " + c;
      }

      // Handle a left shift, restricted to bytes.
      function left(octet, shift)
        (octet << shift) & 0xff;

      advance();
      accumulate(left(val, 3));
      advance();
      accumulate(val >> 2);
      ++rOffset;
      accumulate(left(val, 6));
      advance();
      accumulate(left(val, 1));
      advance();
      accumulate(val >> 4);
      ++rOffset;
      accumulate(left(val, 4));
      advance();
      accumulate(val >> 1);
      ++rOffset;
      accumulate(left(val, 7));
      advance();
      accumulate(left(val, 2));
      advance();
      accumulate(val >> 3);
      ++rOffset;
      accumulate(left(val, 5));
      advance();
      accumulate(val);
      ++rOffset;
    }

    // Our output. Define to be explicit (and maybe the compiler will be smart).
    let ret  = new Array(bytes);
    let i    = 0;
    let cOff = 0;
    let rOff = 0;

    for (; i < blocks; ++i) {
      try {
        processBlock(ret, cOff, rOff);
      } catch (ex) {
        // Handle the detection of padding.
        if (ex == "Done")
          break;
        throw ex;
      }
      cOff += 8;
      rOff += 5;
    }

    // Slice in case our shift overflowed to the right.
    return Utils.byteArrayToString(ret.slice(0, bytes));
  },

  /**
   * Base32 encode (RFC 4648) a string
   */
  encodeBase32: function encodeBase32(bytes) {
    const key = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    let quanta = Math.floor(bytes.length / 5);
    let leftover = bytes.length % 5;

    // Pad the last quantum with zeros so the length is a multiple of 5.
    if (leftover) {
      quanta += 1;
      for (let i = leftover; i < 5; i++)
        bytes += "\0";
    }

    // Chop the string into quanta of 5 bytes (40 bits). Each quantum
    // is turned into 8 characters from the 32 character base.
    let ret = "";
    for (let i = 0; i < bytes.length; i += 5) {
      let c = [byte.charCodeAt() for each (byte in bytes.slice(i, i + 5))];
      ret += key[c[0] >> 3]
           + key[((c[0] << 2) & 0x1f) | (c[1] >> 6)]
           + key[(c[1] >> 1) & 0x1f]
           + key[((c[1] << 4) & 0x1f) | (c[2] >> 4)]
           + key[((c[2] << 1) & 0x1f) | (c[3] >> 7)]
           + key[(c[3] >> 2) & 0x1f]
           + key[((c[3] << 3) & 0x1f) | (c[4] >> 5)]
           + key[c[4] & 0x1f];
    }

    switch (leftover) {
      case 1:
        return ret.slice(0, -6) + "======";
      case 2:
        return ret.slice(0, -4) + "====";
      case 3:
        return ret.slice(0, -3) + "===";
      case 4:
        return ret.slice(0, -1) + "=";
      default:
        return ret;
    }
  },

  /**
   * Turn RFC 4648 base32 into our own user-friendly version.
   *   ABCDEFGHIJKLMNOPQRSTUVWXYZ234567
   * becomes
   *   abcdefghijk8mn9pqrstuvwxyz234567
   */
  base32ToFriendly: function base32ToFriendly(input) {
    return input.toLowerCase()
                .replace("l", '8', "g")
                .replace("o", '9', "g");
  },

  base32FromFriendly: function base32FromFriendly(input) {
    return input.toUpperCase()
                .replace("8", 'L', "g")
                .replace("9", 'O', "g");
  },


  /**
   * Key manipulation.
   */

  // Return an octet string in friendly base32 *with no trailing =*.
  encodeKeyBase32: function encodeKeyBase32(keyData) {
    return Utils.base32ToFriendly(
             Utils.encodeBase32(keyData))
           .slice(0, SYNC_KEY_ENCODED_LENGTH);
  },

  decodeKeyBase32: function decodeKeyBase32(encoded) {
    return Utils.decodeBase32(
             Utils.base32FromFriendly(
               Utils.normalizePassphrase(encoded)))
           .slice(0, SYNC_KEY_DECODED_LENGTH);
  },

  base64Key: function base64Key(keyData) {
    return btoa(keyData);
  },

  deriveKeyFromPassphrase: function deriveKeyFromPassphrase(passphrase, salt, keyLength, forceJS) {
    if (Svc.Crypto.deriveKeyFromPassphrase && !forceJS) {
      return Svc.Crypto.deriveKeyFromPassphrase(passphrase, salt, keyLength);
    }
    else {
      // Fall back to JS implementation.
      // 4096 is hardcoded in WeaveCrypto, so do so here.
      return Utils.pbkdf2Generate(passphrase, atob(salt), 4096, keyLength);
    }
  },

  /**
   * N.B., salt should be base64 encoded, even though we have to decode
   * it later!
   */
  derivePresentableKeyFromPassphrase : function derivePresentableKeyFromPassphrase(passphrase, salt, keyLength, forceJS) {
    let k = Utils.deriveKeyFromPassphrase(passphrase, salt, keyLength, forceJS);
    return Utils.encodeKeyBase32(k);
  },

  /**
   * N.B., salt should be base64 encoded, even though we have to decode
   * it later!
   */
  deriveEncodedKeyFromPassphrase : function deriveEncodedKeyFromPassphrase(passphrase, salt, keyLength, forceJS) {
    let k = Utils.deriveKeyFromPassphrase(passphrase, salt, keyLength, forceJS);
    return Utils.base64Key(k);
  },

  /**
   * Take a base64-encoded 128-bit AES key, returning it as five groups of five
   * uppercase alphanumeric characters, separated by hyphens.
   * A.K.A. base64-to-base32 encoding.
   */
  presentEncodedKeyAsSyncKey : function presentEncodedKeyAsSyncKey(encodedKey) {
    return Utils.encodeKeyBase32(atob(encodedKey));
  },

  makeURI: function Weave_makeURI(URIString) {
    if (!URIString)
      return null;
    try {
      return Svc.IO.newURI(URIString, null, null);
    } catch (e) {
      let log = Log4Moz.repository.getLogger("Service.Util");
      log.debug("Could not create URI: " + Utils.exceptionStr(e));
      return null;
    }
  },

  makeURL: function Weave_makeURL(URIString) {
    let url = Utils.makeURI(URIString);
    url.QueryInterface(Ci.nsIURL);
    return url;
  },

  xpath: function Weave_xpath(xmlDoc, xpathString) {
    let root = xmlDoc.ownerDocument == null ?
      xmlDoc.documentElement : xmlDoc.ownerDocument.documentElement;
    let nsResolver = xmlDoc.createNSResolver(root);

    return xmlDoc.evaluate(xpathString, xmlDoc, nsResolver,
                           Ci.nsIDOMXPathResult.ANY_TYPE, null);
  },

  getTmp: function Weave_getTmp(name) {
    let tmp = Svc.Directory.get("ProfD", Ci.nsIFile);
    tmp.QueryInterface(Ci.nsILocalFile);

    tmp.append("weave");
    tmp.append("tmp");
    if (!tmp.exists())
      tmp.create(tmp.DIRECTORY_TYPE, PERMS_DIRECTORY);

    if (name)
      tmp.append(name);

    return tmp;
  },

  /**
   * Load a json object from disk
   *
   * @param filePath
   *        Json file path load from weave/[filePath].json
   * @param that
   *        Object to use for logging and "this" for callback
   * @param callback
   *        Function to process json object as its first parameter
   */
  jsonLoad: function Utils_jsonLoad(filePath, that, callback) {
    filePath = "weave/" + filePath + ".json";
    if (that._log)
      that._log.trace("Loading json from disk: " + filePath);

    let file = Utils.getProfileFile(filePath);
    if (!file.exists())
      return;

    try {
      let [is] = Utils.open(file, "<");
      let json = Utils.readStream(is);
      is.close();
      callback.call(that, JSON.parse(json));
    }
    catch (ex) {
      if (that._log)
        that._log.debug("Failed to load json: " + Utils.exceptionStr(ex));
    }
  },

  /**
   * Save a json-able object to disk
   *
   * @param filePath
   *        Json file path save to weave/[filePath].json
   * @param that
   *        Object to use for logging and "this" for callback
   * @param callback
   *        Function to provide json-able object to save. If this isn't a
   *        function, it'll be used as the object to make a json string.
   */
  jsonSave: function Utils_jsonSave(filePath, that, callback) {
    filePath = "weave/" + filePath + ".json";
    if (that._log)
      that._log.trace("Saving json to disk: " + filePath);

    let file = Utils.getProfileFile({ autoCreate: true, path: filePath });
    let json = typeof callback == "function" ? callback.call(that) : callback;
    let out = JSON.stringify(json);
    let [fos] = Utils.open(file, ">");
    fos.writeString(out);
    fos.close();
  },

  /**
   * Return a timer that is scheduled to call the callback after waiting the
   * provided time or as soon as possible. The timer will be set as a property
   * of the provided object with the given timer name.
   */
  delay: function delay(callback, wait, thisObj, name) {
    // Default to running right away
    wait = wait || 0;

    // Use a dummy object if one wasn't provided
    thisObj = thisObj || {};

    // Delay an existing timer if it exists
    if (name in thisObj && thisObj[name] instanceof Ci.nsITimer) {
      thisObj[name].delay = wait;
      return;
    }

    // Create a special timer that we can add extra properties
    let timer = {};
    timer.__proto__ = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

    // Provide an easy way to clear out the timer
    timer.clear = function() {
      thisObj[name] = null;
      timer.cancel();
    };

    // Initialize the timer with a smart callback
    timer.initWithCallback({
      notify: function notify() {
        // Clear out the timer once it's been triggered
        timer.clear();
        callback.call(thisObj, timer);
      }
    }, wait, timer.TYPE_ONE_SHOT);

    return thisObj[name] = timer;
  },

  open: function open(pathOrFile, mode, perms) {
    let stream, file;

    if (pathOrFile instanceof Ci.nsIFile) {
      file = pathOrFile;
    } else {
      file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      dump("PATH IS" + pathOrFile + "\n");
      file.initWithPath(pathOrFile);
    }

    if (!perms)
      perms = PERMS_FILE;

    switch(mode) {
    case "<": {
      if (!file.exists())
        throw "Cannot open file for reading, file does not exist";
      let fis = Cc["@mozilla.org/network/file-input-stream;1"].
        createInstance(Ci.nsIFileInputStream);
      fis.init(file, MODE_RDONLY, perms, 0);
      stream = Cc["@mozilla.org/intl/converter-input-stream;1"].
        createInstance(Ci.nsIConverterInputStream);
      stream.init(fis, "UTF-8", 4096,
                  Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
    } break;

    case ">": {
      let fos = Cc["@mozilla.org/network/file-output-stream;1"].
        createInstance(Ci.nsIFileOutputStream);
      fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, perms, 0);
      stream = Cc["@mozilla.org/intl/converter-output-stream;1"]
        .createInstance(Ci.nsIConverterOutputStream);
      stream.init(fos, "UTF-8", 4096, 0x0000);
    } break;

    case ">>": {
      let fos = Cc["@mozilla.org/network/file-output-stream;1"].
        createInstance(Ci.nsIFileOutputStream);
      fos.init(file, MODE_WRONLY | MODE_CREATE | MODE_APPEND, perms, 0);
      stream = Cc["@mozilla.org/intl/converter-output-stream;1"]
        .createInstance(Ci.nsIConverterOutputStream);
      stream.init(fos, "UTF-8", 4096, 0x0000);
    } break;

    default:
      throw "Illegal mode to open(): " + mode;
    }

    return [stream, file];
  },

  getIcon: function(iconUri, defaultIcon) {
    try {
      let iconURI = Utils.makeURI(iconUri);
      return Svc.Favicon.getFaviconLinkForIcon(iconURI).spec;
    }
    catch(ex) {}

    // Just give the provided default icon or the system's default
    return defaultIcon || Svc.Favicon.defaultFavicon.spec;
  },

  getErrorString: function Utils_getErrorString(error, args) {
    try {
      return Str.errors.get(error, args || null);
    } catch (e) {}

    // basically returns "Unknown Error"
    return Str.errors.get("error.reason.unknown");
  },

  // assumes an nsIConverterInputStream
  readStream: function Weave_readStream(is) {
    let ret = "", str = {};
    while (is.readString(4096, str) != 0) {
      ret += str.value;
    }
    return ret;
  },

  encodeUTF8: function(str) {
    try {
      var unicodeConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                             .createInstance(Ci.nsIScriptableUnicodeConverter);
      unicodeConverter.charset = "UTF-8";
      str = unicodeConverter.ConvertFromUnicode(str);
      return str + unicodeConverter.Finish();
    } catch(ex) {
      return null;
    }
  },

  decodeUTF8: function(str) {
    try {
      var unicodeConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                             .createInstance(Ci.nsIScriptableUnicodeConverter);
      unicodeConverter.charset = "UTF-8";
      str = unicodeConverter.ConvertToUnicode(str);
      return str + unicodeConverter.Finish();
    } catch(ex) {
      return null;
    }
  },

  /**
   * Generate 26 characters.
   */
  generatePassphrase: function generatePassphrase() {
    // Note that this is a different base32 alphabet to the one we use for
    // other tasks. It's lowercase, uses different letters, and needs to be
    // decoded with decodeKeyBase32, not just decodeBase32.
    return Utils.encodeKeyBase32(Utils.generateRandomBytes(16));
  },

  /**
   * The following are the methods supported for UI use:
   *
   * * isPassphrase:
   *     determines whether a string is either a normalized or presentable
   *     passphrase.
   * * hyphenatePassphrase:
   *     present a normalized passphrase for display. This might actually
   *     perform work beyond just hyphenation; sorry.
   * * hyphenatePartialPassphrase:
   *     present a fragment of a normalized passphrase for display.
   * * normalizePassphrase:
   *     take a presentable passphrase and reduce it to a normalized
   *     representation for storage. normalizePassphrase can safely be called
   *     on normalized input.
   * * normalizeAccount:
   *     take user input for account/username, cleaning up appropriately.
   */

  isPassphrase: function(s) {
    if (s) {
      return /^[abcdefghijkmnpqrstuvwxyz23456789]{26}$/.test(Utils.normalizePassphrase(s));
    }
    return false;
  },

  /**
   * Hyphenate a passphrase (26 characters) into groups.
   * abbbbccccddddeeeeffffggggh
   * =>
   * a-bbbbc-cccdd-ddeee-effff-ggggh
   */
  hyphenatePassphrase: function hyphenatePassphrase(passphrase) {
    // For now, these are the same.
    return Utils.hyphenatePartialPassphrase(passphrase, true);
  },

  hyphenatePartialPassphrase: function hyphenatePartialPassphrase(passphrase, omitTrailingDash) {
    if (!passphrase)
      return null;

    // Get the raw data input. Just base32.
    let data = passphrase.toLowerCase().replace(/[^abcdefghijkmnpqrstuvwxyz23456789]/g, "");

    // This is the neatest way to do this.
    if ((data.length == 1) && !omitTrailingDash)
      return data + "-";

    // Hyphenate it.
    let y = data.substr(0,1);
    let z = data.substr(1).replace(/(.{1,5})/g, "-$1");

    // Correct length? We're done.
    if ((z.length == 30) || omitTrailingDash)
      return y + z;

    // Add a trailing dash if appropriate.
    return (y + z.replace(/([^-]{5})$/, "$1-")).substr(0, SYNC_KEY_HYPHENATED_LENGTH);
  },

  normalizePassphrase: function normalizePassphrase(pp) {
    // Short var name... have you seen the lines below?!
    // Allow leading and trailing whitespace.
    pp = pp.trim().toLowerCase();

    // 20-char sync key.
    if (pp.length == 23 &&
        [5, 11, 17].every(function(i) pp[i] == '-')) {

      return pp.slice(0, 5) + pp.slice(6, 11)
             + pp.slice(12, 17) + pp.slice(18, 23);
    }

    // "Modern" 26-char key.
    if (pp.length == 31 &&
        [1, 7, 13, 19, 25].every(function(i) pp[i] == '-')) {

      return pp.slice(0, 1) + pp.slice(2, 7)
             + pp.slice(8, 13) + pp.slice(14, 19)
             + pp.slice(20, 25) + pp.slice(26, 31);
    }

    // Something else -- just return.
    return pp;
  },
  
  normalizeAccount: function normalizeAccount(acc) {
    return acc.trim();
  },

  // WeaveCrypto returns bad base64 strings. Truncate excess padding
  // and decode.
  // See Bug 562431, comment 4.
  safeAtoB: function safeAtoB(b64) {
    let len = b64.length;
    let over = len % 4;
    return over ? atob(b64.substr(0, len - over)) : atob(b64);
  },

  /*
   * Calculate the strength of a passphrase provided by the user
   * according to the NIST algorithm (NIST 800-63 Appendix A.1).
   */
  passphraseStrength: function passphraseStrength(value) {
    let bits = 0;

    // The entropy of the first character is taken to be 4 bits.
    if (value.length)
      bits = 4;

    // The entropy of the next 7 characters are 2 bits per character.
    if (value.length > 1)
      bits += Math.min(value.length - 1, 7) * 2;

    // For the 9th through the 20th character the entropy is taken to
    // be 1.5 bits per character.
    if (value.length > 8)
      bits += Math.min(value.length - 8, 12) * 1.5;

    // For characters 21 and above the entropy is taken to be 1 bit per character.
    if (value.length > 20)
      bits += value.length - 20;

    // Bonus of 6 bits if we find non-alphabetic characters
    if ([char.charCodeAt() for each (char in value.toLowerCase())]
        .some(function(chr) chr < 97 || chr > 122))
      bits += 6;
      
    return bits;
  },

  /**
   * Create an array like the first but without elements of the second
   */
  arraySub: function arraySub(minuend, subtrahend) {
    return minuend.filter(function(i) subtrahend.indexOf(i) == -1);
  },

  bind2: function Async_bind2(object, method) {
    return function innerBind() { return method.apply(object, arguments); };
  },

  mpLocked: function mpLocked() {
    let modules = Cc["@mozilla.org/security/pkcs11moduledb;1"].
                  getService(Ci.nsIPKCS11ModuleDB);
    let sdrSlot = modules.findSlotByName("");
    let status  = sdrSlot.status;
    let slots = Ci.nsIPKCS11Slot;

    if (status == slots.SLOT_READY || status == slots.SLOT_LOGGED_IN
                                   || status == slots.SLOT_UNINITIALIZED)
      return false;

    if (status == slots.SLOT_NOT_LOGGED_IN)
      return true;
    
    // something wacky happened, pretend MP is locked
    return true;
  },

  __prefs: null,
  get prefs() {
    if (!this.__prefs) {
      this.__prefs = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefService);
      this.__prefs = this.__prefs.getBranch(PREFS_BRANCH);
      this.__prefs.QueryInterface(Ci.nsIPrefBranch2);
    }
    return this.__prefs;
  }
};

let FakeSvc = {
  // Private Browsing
  "@mozilla.org/privatebrowsing;1": {
    autoStarted: false,
    privateBrowsingEnabled: false
  },
  // Session Restore
  "@mozilla.org/browser/sessionstore;1": {
    setTabValue: function(tab, key, value) {
      if (!tab.__SS_extdata)
        tab.__SS_extdata = {};
      tab.__SS_extData[key] = value;
    },
    getBrowserState: function() {
      // Fennec should have only one window. Not more, not less.
      let state = { windows: [{ tabs: [] }] };
      let window = Svc.WinMediator.getMostRecentWindow("navigator:browser");

      // Extract various pieces of tab data
      window.Browser._tabs.forEach(function(tab) {
        let tabState = { entries: [{}] };
        let browser = tab.browser;

        // Cases when we want to skip the tab. Could come up if we get
        // state as a tab is opening or closing.
        if (!browser || !browser.currentURI || !browser.sessionHistory)
          return;

        let history = browser.sessionHistory;
        if (history.count > 0) {
          // We're only grabbing the current history entry for now.
          let entry = history.getEntryAtIndex(history.index, false);
          tabState.entries[0].url = entry.URI.spec;
          // Like SessionStore really does it...
          if (entry.title && entry.title != entry.url)
            tabState.entries[0].title = entry.title;
        }
        // index is 1-based
        tabState.index = 1;

        // Get the image for the tab. Fennec doesn't quite work the same
        // way as Firefox, so we'll just get this from the browser object.
        tabState.attributes = { image: browser.mIconURL };

        // Collect the extdata
        if (tab.__SS_extdata) {
          tabState.extData = {};
          for (let key in tab.__SS_extdata)
            tabState.extData[key] = tab.__SS_extdata[key];
        }

        // Add the tab to the window
        state.windows[0].tabs.push(tabState);
      });
      return JSON.stringify(state);
    }
  },
  // A fake service only used for testing
  "@labs.mozilla.com/Fake/Thing;1": {
    isFake: true
  }
};

/*
 * Commonly-used services
 */
let Svc = {};
Svc.Prefs = new Preferences(PREFS_BRANCH);
Svc.DefaultPrefs = new Preferences({branch: PREFS_BRANCH, defaultBranch: true});
Svc.Obs = Observers;

this.__defineGetter__("_sessionCID", function() {
  //sets session CID based on browser type
  let appInfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
  return appInfo.ID == SEAMONKEY_ID ? "@mozilla.org/suite/sessionstore;1"
                                    : "@mozilla.org/browser/sessionstore;1";
});
[["Annos", "@mozilla.org/browser/annotation-service;1", "nsIAnnotationService"],
 ["AppInfo", "@mozilla.org/xre/app-info;1", "nsIXULAppInfo"],
 ["Bookmark", "@mozilla.org/browser/nav-bookmarks-service;1", "nsINavBookmarksService"],
 ["Directory", "@mozilla.org/file/directory_service;1", "nsIProperties"],
 ["Env", "@mozilla.org/process/environment;1", "nsIEnvironment"],
 ["Favicon", "@mozilla.org/browser/favicon-service;1", "nsIFaviconService"],
 ["Form", "@mozilla.org/satchel/form-history;1", "nsIFormHistory2"],
 ["History", "@mozilla.org/browser/nav-history-service;1", "nsPIPlacesDatabase"],
 ["Idle", "@mozilla.org/widget/idleservice;1", "nsIIdleService"],
 ["IO", "@mozilla.org/network/io-service;1", "nsIIOService"],
 ["KeyFactory", "@mozilla.org/security/keyobjectfactory;1", "nsIKeyObjectFactory"],
 ["Login", "@mozilla.org/login-manager;1", "nsILoginManager"],
 ["Memory", "@mozilla.org/xpcom/memory-service;1", "nsIMemory"],
 ["Private", "@mozilla.org/privatebrowsing;1", "nsIPrivateBrowsingService"],
 ["Profiles", "@mozilla.org/toolkit/profile-service;1", "nsIToolkitProfileService"],
 ["Prompt", "@mozilla.org/embedcomp/prompt-service;1", "nsIPromptService"],
 ["Script", "@mozilla.org/moz/jssubscript-loader;1", "mozIJSSubScriptLoader"],
 ["SysInfo", "@mozilla.org/system-info;1", "nsIPropertyBag2"],
 ["Version", "@mozilla.org/xpcom/version-comparator;1", "nsIVersionComparator"],
 ["WinMediator", "@mozilla.org/appshell/window-mediator;1", "nsIWindowMediator"],
 ["WinWatcher", "@mozilla.org/embedcomp/window-watcher;1", "nsIWindowWatcher"],
 ["Session", this._sessionCID, "nsISessionStore"],
].forEach(function(lazy) Utils.lazySvc(Svc, lazy[0], lazy[1], lazy[2]));

Svc.__defineGetter__("Crypto", function() {
  let cryptoSvc;
  try {
    let ns = {};
    Cu.import("resource://services-crypto/WeaveCrypto.js", ns);
    cryptoSvc = new ns.WeaveCrypto();
  } catch (ex) {
    // Fallback to binary WeaveCrypto
    cryptoSvc = Cc["@labs.mozilla.com/Weave/Crypto;1"].
                getService(Ci.IWeaveCrypto);
  }
  delete Svc.Crypto;
  return Svc.Crypto = cryptoSvc;
});

let Str = {};
["errors", "sync"]
  .forEach(function(lazy) Utils.lazy2(Str, lazy, Utils.lazyStrings(lazy)));

Svc.Obs.add("xpcom-shutdown", function () {
  for (let name in Svc)
    delete Svc[name];
});
