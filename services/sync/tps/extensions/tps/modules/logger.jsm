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
 * The Original Code is Crossweave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
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
    if (!bool) {
      let message = msg;
      if (showPotentialError && this._potentialError) {
        message += "; " + this._potentialError;
        this._potentialError = null;
      }
      throw("ASSERTION FAILED! " + message);
    }
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
      var now = new Date()
      this.write(now.getFullYear() + "-" + (now.getMonth() < 9 ? '0' : '') + 
          (now.getMonth() + 1) + "-" + 
          (now.getDay() < 9 ? '0' : '') + (now.getDay() + 1) + " " +
          (now.getHours() < 10 ? '0' : '') + now.getHours() + ":" +
          (now.getMinutes() < 10 ? '0' : '') + now.getMinutes() + ":" +
          (now.getSeconds() < 10 ? '0' : '') + now.getSeconds() + " " + 
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

