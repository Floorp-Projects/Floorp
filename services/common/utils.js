/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["CommonUtils"];

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm")
Cu.import("resource://gre/modules/Log.jsm");

this.CommonUtils = {
  /*
   * Set manipulation methods. These should be lifted into toolkit, or added to
   * `Set` itself.
   */

  /**
   * Return elements of `a` or `b`.
   */
  union: function (a, b) {
    let out = new Set(a);
    for (let x of b) {
      out.add(x);
    }
    return out;
  },

  /**
   * Return elements of `a` that are not present in `b`.
   */
  difference: function (a, b) {
    let out = new Set(a);
    for (let x of b) {
      out.delete(x);
    }
    return out;
  },

  /**
   * Return elements of `a` that are also in `b`.
   */
  intersection: function (a, b) {
    let out = new Set();
    for (let x of a) {
      if (b.has(x)) {
        out.add(x);
      }
    }
    return out;
  },

  /**
   * Return true if `a` and `b` are the same size, and
   * every element of `a` is in `b`.
   */
  setEqual: function (a, b) {
    if (a.size != b.size) {
      return false;
    }
    for (let x of a) {
      if (!b.has(x)) {
        return false;
      }
    }
    return true;
  },

  /**
   * Encode byte string as base64URL (RFC 4648).
   *
   * @param bytes
   *        (string) Raw byte string to encode.
   * @param pad
   *        (bool) Whether to include padding characters (=). Defaults
   *        to true for historical reasons.
   */
  encodeBase64URL: function encodeBase64URL(bytes, pad=true) {
    let s = btoa(bytes).replace(/\+/g, "-").replace(/\//g, "_");

    if (!pad) {
      return s.replace(/=+$/, "");
    }

    return s;
  },

  /**
   * Create a nsIURI instance from a string.
   */
  makeURI: function makeURI(URIString) {
    if (!URIString)
      return null;
    try {
      return Services.io.newURI(URIString, null, null);
    } catch (e) {
      let log = Log.repository.getLogger("Common.Utils");
      log.debug("Could not create URI", e);
      return null;
    }
  },

  /**
   * Execute a function on the next event loop tick.
   *
   * @param callback
   *        Function to invoke.
   * @param thisObj [optional]
   *        Object to bind the callback to.
   */
  nextTick: function nextTick(callback, thisObj) {
    if (thisObj) {
      callback = callback.bind(thisObj);
    }
    Services.tm.currentThread.dispatch(callback, Ci.nsIThread.DISPATCH_NORMAL);
  },

  /**
   * Return a promise resolving on some later tick.
   *
   * This a wrapper around Promise.resolve() that prevents stack
   * accumulation and prevents callers from accidentally relying on
   * same-tick promise resolution.
   */
  laterTickResolvingPromise: function (value, prototype) {
    let deferred = Promise.defer(prototype);
    this.nextTick(deferred.resolve.bind(deferred, value));
    return deferred.promise;
  },

  /**
   * Spin the event loop and return once the next tick is executed.
   *
   * This is an evil function and should not be used in production code. It
   * exists in this module for ease-of-use.
   */
  waitForNextTick: function waitForNextTick() {
    let cb = Async.makeSyncCallback();
    this.nextTick(cb);
    Async.waitForSyncCallback(cb);

    return;
  },

  /**
   * Return a timer that is scheduled to call the callback after waiting the
   * provided time or as soon as possible. The timer will be set as a property
   * of the provided object with the given timer name.
   */
  namedTimer: function namedTimer(callback, wait, thisObj, name) {
    if (!thisObj || !name) {
      throw "You must provide both an object and a property name for the timer!";
    }

    // Delay an existing timer if it exists
    if (name in thisObj && thisObj[name] instanceof Ci.nsITimer) {
      thisObj[name].delay = wait;
      return;
    }

    // Create a special timer that we can add extra properties
    let timer = Object.create(Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer));

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

  encodeUTF8: function encodeUTF8(str) {
    try {
      str = this._utf8Converter.ConvertFromUnicode(str);
      return str + this._utf8Converter.Finish();
    } catch (ex) {
      return null;
    }
  },

  decodeUTF8: function decodeUTF8(str) {
    try {
      str = this._utf8Converter.ConvertToUnicode(str);
      return str + this._utf8Converter.Finish();
    } catch (ex) {
      return null;
    }
  },

  byteArrayToString: function byteArrayToString(bytes) {
    return bytes.map(byte => String.fromCharCode(byte)).join("");
  },

  stringToByteArray: function stringToByteArray(bytesString) {
    return Array.prototype.slice.call(bytesString).map(c => c.charCodeAt(0));
  },

  bytesAsHex: function bytesAsHex(bytes) {
    return Array.prototype.slice.call(bytes).map(c => ("0" + c.charCodeAt(0).toString(16)).slice(-2)).join("");
  },

  stringAsHex: function stringAsHex(str) {
    return CommonUtils.bytesAsHex(CommonUtils.encodeUTF8(str));
  },

  stringToBytes: function stringToBytes(str) {
    return CommonUtils.hexToBytes(CommonUtils.stringAsHex(str));
  },

  hexToBytes: function hexToBytes(str) {
    let bytes = [];
    for (let i = 0; i < str.length - 1; i += 2) {
      bytes.push(parseInt(str.substr(i, 2), 16));
    }
    return String.fromCharCode.apply(String, bytes);
  },

  hexAsString: function hexAsString(hex) {
    return CommonUtils.decodeUTF8(CommonUtils.hexToBytes(hex));
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
      let c = Array.prototype.slice.call(bytes.slice(i, i + 5)).map(byte => byte.charCodeAt(0));
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
      function left(octet, shift) {
        return (octet << shift) & 0xff;
      }

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
    return CommonUtils.byteArrayToString(ret.slice(0, bytes));
  },

  /**
   * Trim excess padding from a Base64 string and atob().
   *
   * See bug 562431 comment 4.
   */
  safeAtoB: function safeAtoB(b64) {
    let len = b64.length;
    let over = len % 4;
    return over ? atob(b64.substr(0, len - over)) : atob(b64);
  },

  /**
   * Parses a JSON file from disk using OS.File and promises.
   *
   * @param path the file to read. Will be passed to `OS.File.read()`.
   * @return a promise that resolves to the JSON contents of the named file.
   */
  readJSON: function(path) {
    return OS.File.read(path, { encoding: "utf-8" }).then((data) => {
      return JSON.parse(data);
    });
  },

  /**
   * Write a JSON object to the named file using OS.File and promises.
   *
   * @param contents a JS object. Will be serialized.
   * @param path the path of the file to write.
   * @return a promise, as produced by OS.File.writeAtomic.
   */
  writeJSON: function(contents, path) {
    let data = JSON.stringify(contents);
    return OS.File.writeAtomic(path, data, {encoding: "utf-8", tmpPath: path + ".tmp"});
  },


  /**
   * Ensure that the specified value is defined in integer milliseconds since
   * UNIX epoch.
   *
   * This throws an error if the value is not an integer, is negative, or looks
   * like seconds, not milliseconds.
   *
   * If the value is null or 0, no exception is raised.
   *
   * @param value
   *        Value to validate.
   */
  ensureMillisecondsTimestamp: function ensureMillisecondsTimestamp(value) {
    if (!value) {
      return;
    }

    if (!/^[0-9]+$/.test(value)) {
      throw new Error("Timestamp value is not a positive integer: " + value);
    }

    let intValue = parseInt(value, 10);

    if (!intValue) {
       return;
    }

    // Catch what looks like seconds, not milliseconds.
    if (intValue < 10000000000) {
      throw new Error("Timestamp appears to be in seconds: " + intValue);
    }
  },

  /**
   * Read bytes from an nsIInputStream into a string.
   *
   * @param stream
   *        (nsIInputStream) Stream to read from.
   * @param count
   *        (number) Integer number of bytes to read. If not defined, or
   *        0, all available input is read.
   */
  readBytesFromInputStream: function readBytesFromInputStream(stream, count) {
    let BinaryInputStream = Components.Constructor(
        "@mozilla.org/binaryinputstream;1",
        "nsIBinaryInputStream",
        "setInputStream");
    if (!count) {
      count = stream.available();
    }

    return new BinaryInputStream(stream).readBytes(count);
  },

  /**
   * Generate a new UUID using nsIUUIDGenerator.
   *
   * Example value: "1e00a2e2-1570-443e-bf5e-000354124234"
   *
   * @return string A hex-formatted UUID string.
   */
  generateUUID: function generateUUID() {
    let uuid = Cc["@mozilla.org/uuid-generator;1"]
                 .getService(Ci.nsIUUIDGenerator)
                 .generateUUID()
                 .toString();

    return uuid.substring(1, uuid.length - 1);
  },

  /**
   * Obtain an epoch value from a preference.
   *
   * This reads a string preference and returns an integer. The string
   * preference is expected to contain the integer milliseconds since epoch.
   * For best results, only read preferences that have been saved with
   * setDatePref().
   *
   * We need to store times as strings because integer preferences are only
   * 32 bits and likely overflow most dates.
   *
   * If the pref contains a non-integer value, the specified default value will
   * be returned.
   *
   * @param branch
   *        (Preferences) Branch from which to retrieve preference.
   * @param pref
   *        (string) The preference to read from.
   * @param def
   *        (Number) The default value to use if the preference is not defined.
   * @param log
   *        (Log.Logger) Logger to write warnings to.
   */
  getEpochPref: function getEpochPref(branch, pref, def=0, log=null) {
    if (!Number.isInteger(def)) {
      throw new Error("Default value is not a number: " + def);
    }

    let valueStr = branch.get(pref, null);

    if (valueStr !== null) {
      let valueInt = parseInt(valueStr, 10);
      if (Number.isNaN(valueInt)) {
        if (log) {
          log.warn("Preference value is not an integer. Using default. " +
                   pref + "=" + valueStr + " -> " + def);
        }

        return def;
      }

      return valueInt;
    }

    return def;
  },

  /**
   * Obtain a Date from a preference.
   *
   * This is a wrapper around getEpochPref. It converts the value to a Date
   * instance and performs simple range checking.
   *
   * The range checking ensures the date is newer than the oldestYear
   * parameter.
   *
   * @param branch
   *        (Preferences) Branch from which to read preference.
   * @param pref
   *        (string) The preference from which to read.
   * @param def
   *        (Number) The default value (in milliseconds) if the preference is
   *        not defined or invalid.
   * @param log
   *        (Log.Logger) Logger to write warnings to.
   * @param oldestYear
   *        (Number) Oldest year to accept in read values.
   */
  getDatePref: function getDatePref(branch, pref, def=0, log=null,
                                    oldestYear=2010) {

    let valueInt = this.getEpochPref(branch, pref, def, log);
    let date = new Date(valueInt);

    if (valueInt == def || date.getFullYear() >= oldestYear) {
      return date;
    }

    if (log) {
      log.warn("Unexpected old date seen in pref. Returning default: " +
               pref + "=" + date + " -> " + def);
    }

    return new Date(def);
  },

  /**
   * Store a Date in a preference.
   *
   * This is the opposite of getDatePref(). The same notes apply.
   *
   * If the range check fails, an Error will be thrown instead of a default
   * value silently being used.
   *
   * @param branch
   *        (Preference) Branch from which to read preference.
   * @param pref
   *        (string) Name of preference to write to.
   * @param date
   *        (Date) The value to save.
   * @param oldestYear
   *        (Number) The oldest year to accept for values.
   */
  setDatePref: function setDatePref(branch, pref, date, oldestYear=2010) {
    if (date.getFullYear() < oldestYear) {
      throw new Error("Trying to set " + pref + " to a very old time: " +
                      date + ". The current time is " + new Date() +
                      ". Is the system clock wrong?");
    }

    branch.set(pref, "" + date.getTime());
  },

  /**
   * Convert a string between two encodings.
   *
   * Output is only guaranteed if the input stream is composed of octets. If
   * the input string has characters with values larger than 255, data loss
   * will occur.
   *
   * The returned string is guaranteed to consist of character codes no greater
   * than 255.
   *
   * @param s
   *        (string) The source string to convert.
   * @param source
   *        (string) The current encoding of the string.
   * @param dest
   *        (string) The target encoding of the string.
   *
   * @return string
   */
  convertString: function convertString(s, source, dest) {
    if (!s) {
      throw new Error("Input string must be defined.");
    }

    let is = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
    is.setData(s, s.length);

    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                     .createInstance(Ci.nsIStreamLoader);

    let result;

    listener.init({
      onStreamComplete: function onStreamComplete(loader, context, status,
                                                  length, data) {
        result = String.fromCharCode.apply(this, data);
      },
    });

    let converter = this._converterService.asyncConvertData(source, dest,
                                                            listener, null);
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, is, 0, s.length);
    converter.onStopRequest(null, null, null);

    return result;
  },
};

XPCOMUtils.defineLazyGetter(CommonUtils, "_utf8Converter", function() {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
});

XPCOMUtils.defineLazyGetter(CommonUtils, "_converterService", function() {
  return Cc["@mozilla.org/streamConverters;1"]
           .getService(Ci.nsIStreamConverterService);
});
