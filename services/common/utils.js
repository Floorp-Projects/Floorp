/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const EXPORTED_SYMBOLS = ["CommonUtils"];

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
};

XPCOMUtils.defineLazyGetter(CommonUtils, "_utf8Converter", function() {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
});
