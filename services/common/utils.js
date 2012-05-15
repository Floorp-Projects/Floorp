/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const EXPORTED_SYMBOLS = ["CommonUtils"];

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/log4moz.js");

let CommonUtils = {
  exceptionStr: function exceptionStr(e) {
    let message = e.message ? e.message : e;
    return message + " " + CommonUtils.stackTrace(e);
  },

  stackTrace: function stackTrace(e) {
    // Wrapped nsIException
    if (e.location) {
      let frame = e.location;
      let output = [];
      while (frame) {
        // Works on frames or exceptions, munges file:// URIs to shorten the paths
        // FIXME: filename munging is sort of hackish, might be confusing if
        // there are multiple extensions with similar filenames
        let str = "<file:unknown>";

        let file = frame.filename || frame.fileName;
        if (file){
          str = file.replace(/^(?:chrome|file):.*?([^\/\.]+\.\w+)$/, "$1");
        }

        if (frame.lineNumber){
          str += ":" + frame.lineNumber;
        }
        if (frame.name){
          str = frame.name + "()@" + str;
        }

        if (str){
          output.push(str);
        }
        frame = frame.caller;
      }
      return "Stack trace: " + output.join(" < ");
    }
    // Standard JS exception
    if (e.stack){
      return "JS Stack trace: " + e.stack.trim().replace(/\n/g, " < ").
        replace(/@[^@]*?([^\/\.]+\.\w+:)/g, "@$1");
    }

    return "No traceback available";
  },

  /**
   * Encode byte string as base64URL (RFC 4648).
   */
  encodeBase64URL: function encodeBase64URL(bytes) {
    return btoa(bytes).replace("+", "-", "g").replace("/", "_", "g");
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
      let log = Log4Moz.repository.getLogger("Common.Utils");
      log.debug("Could not create URI: " + CommonUtils.exceptionStr(e));
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
    return [String.fromCharCode(byte) for each (byte in bytes)].join("");
  },

  bytesAsHex: function bytesAsHex(bytes) {
    let hex = "";
    for (let i = 0; i < bytes.length; i++) {
      hex += ("0" + bytes[i].charCodeAt().toString(16)).slice(-2);
    }
    return hex;
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
   * Load a JSON file from disk in the profile directory.
   *
   * @param filePath
   *        JSON file path load from profile. Loaded file will be
   *        <profile>/<filePath>.json. i.e. Do not specify the ".json"
   *        extension.
   * @param that
   *        Object to use for logging and "this" for callback.
   * @param callback
   *        Function to process json object as its first argument. If the file
   *        could not be loaded, the first argument will be undefined.
   */
  jsonLoad: function jsonLoad(filePath, that, callback) {
    let path = filePath + ".json";

    if (that._log) {
      that._log.trace("Loading json from disk: " + filePath);
    }

    let file = FileUtils.getFile("ProfD", path.split("/"), true);
    if (!file.exists()) {
      callback.call(that);
      return;
    }

    let channel = NetUtil.newChannel(file);
    channel.contentType = "application/json";

    NetUtil.asyncFetch(channel, function (is, result) {
      if (!Components.isSuccessCode(result)) {
        callback.call(that);
        return;
      }
      let string = NetUtil.readInputStreamToString(is, is.available());
      is.close();
      let json;
      try {
        json = JSON.parse(string);
      } catch (ex) {
        if (that._log) {
          that._log.debug("Failed to load json: " +
                          CommonUtils.exceptionStr(ex));
        }
      }
      callback.call(that, json);
    });
  },

  /**
   * Save a json-able object to disk in the profile directory.
   *
   * @param filePath
   *        JSON file path save to <filePath>.json
   * @param that
   *        Object to use for logging and "this" for callback
   * @param obj
   *        Function to provide json-able object to save. If this isn't a
   *        function, it'll be used as the object to make a json string.
   * @param callback
   *        Function called when the write has been performed. Optional.
   */
  jsonSave: function jsonSave(filePath, that, obj, callback) {
    let path = filePath + ".json";
    if (that._log) {
      that._log.trace("Saving json to disk: " + path);
    }

    let file = FileUtils.getFile("ProfD", path.split("/"), true);
    let json = typeof obj == "function" ? obj.call(that) : obj;
    let out = JSON.stringify(json);

    let fos = FileUtils.openSafeFileOutputStream(file);
    let is = this._utf8Converter.convertToInputStream(out);
    NetUtil.asyncCopy(is, fos, function (result) {
      if (typeof callback == "function") {
        callback.call(that);
      }
    });
  },
};

XPCOMUtils.defineLazyGetter(CommonUtils, "_utf8Converter", function() {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
});
