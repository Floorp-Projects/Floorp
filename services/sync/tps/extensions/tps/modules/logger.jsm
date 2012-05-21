/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* This is a JavaScript module (JSM) to be imported via
    Components.utils.import() and acts as a singleton.
    Only the following listed symbols will exposed on import, and only when
    and where imported. */

var EXPORTED_SYMBOLS = ["Logger"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

var Logger =
{
  _foStream: null,
  _converter: null,
  _potentialError: null,

  init: function (path) {
    if (this._converter != null) {
      // we're already open!
      return;
    }

    let prefs = CC["@mozilla.org/preferences-service;1"]
                .getService(CI.nsIPrefBranch);
    if (path) {
      prefs.setCharPref("tps.logfile", path);
    }
    else {
      path = prefs.getCharPref("tps.logfile");
    }

    this._file = CC["@mozilla.org/file/local;1"]
                 .createInstance(CI.nsILocalFile);
    this._file.initWithPath(path);
    var exists = this._file.exists();

    // Make a file output stream and converter to handle it.
    this._foStream = CC["@mozilla.org/network/file-output-stream;1"]
                     .createInstance(CI.nsIFileOutputStream);
    // If the file already exists, append it, otherwise create it.
    var fileflags = exists ? 0x02 | 0x08 | 0x10 : 0x02 | 0x08 | 0x20;

    this._foStream.init(this._file, fileflags, 0666, 0);
    this._converter = CC["@mozilla.org/intl/converter-output-stream;1"]
                      .createInstance(CI.nsIConverterOutputStream);
    this._converter.init(this._foStream, "UTF-8", 0, 0);
  },

  write: function (data) {
    if (this._converter == null) {
      CU.reportError(
          "TPS Logger.write called with _converter == null!");
      return;
    }
    this._converter.writeString(data);
  },

  close: function () {
    if (this._converter != null) {
      this._converter.close();
      this._converter = null;
      this._foStream = null;
    }
  },

  AssertTrue: function(bool, msg, showPotentialError) {
    if (bool) {
      return;
    }

    if (showPotentialError && this._potentialError) {
      msg += "; " + this._potentialError;
      this._potentialError = null;
    }
    throw("ASSERTION FAILED! " + msg);
  },

  AssertFalse: function(bool, msg, showPotentialError) {
    return this.AssertTrue(!bool, msg, showPotentialError);
  },

  AssertEqual: function(val1, val2, msg) {
    if (val1 != val2)
      throw("ASSERTION FAILED! " + msg + "; expected " +
            JSON.stringify(val2) + ", got " + JSON.stringify(val1));
  },

  log: function (msg, withoutPrefix) {
    dump(msg + "\n");
    if (withoutPrefix) {
      this.write(msg + "\n");
    }
    else {
      function pad(n, len) {
        let s = "0000" + n;
        return s.slice(-len);
      }

      let now = new Date();
      let year    = pad(now.getFullYear(),     4);
      let month   = pad(now.getMonth() + 1,    2);
      let day     = pad(now.getDate(),         2);
      let hour    = pad(now.getHours(),        2);
      let minutes = pad(now.getMinutes(),      2);
      let seconds = pad(now.getSeconds(),      2);
      let ms      = pad(now.getMilliseconds(), 3);

      this.write(year + "-" + month + "-" + day + " " +
                 hour + ":" + minutes + ":" + seconds + "." + ms + " " +
                 msg + "\n");
    }
  },

  clearPotentialError: function() {
    this._potentialError = null;
  },

  logPotentialError: function(msg) {
    this._potentialError = msg;
  },

  logLastPotentialError: function(msg) {
    var message = msg;
    if (this._potentialError) {
      message = this._poentialError;
      this._potentialError = null;
    }
    this.log("CROSSWEAVE ERROR: " + message);
  },

  logError: function (msg) {
    this.log("CROSSWEAVE ERROR: " + msg);
  },

  logInfo: function (msg, withoutPrefix) {
    if (withoutPrefix)
      this.log(msg, true);
    else
      this.log("CROSSWEAVE INFO: " + msg);
  },

  logPass: function (msg) {
    this.log("CROSSWEAVE TEST PASS: " + msg);
  },
};

