/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.TrackingDBService = function() {
  this._initialize();
};

TrackingDBService.prototype = Object.freeze({
  classID: Components.ID("{3c9c43b6-09eb-4ed2-9b87-e29f4221eef0}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsITrackingDBService]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(TrackingDBService),

  _initialize() {
    // Start up the database here
  },

  _readAsyncStream(stream) {
    return new Promise(function(resolve, reject) {
      let result = "";
      let source = Cc["@mozilla.org/binaryinputstream;1"]
        .createInstance(Ci.nsIBinaryInputStream);
      source.setInputStream(stream);
      function readData() {
        try {
          result += source.readBytes(source.available());
          stream.asyncWait(readData, 0, 0, Services.tm.currentThread);
        } catch (e) {
          if (e.result == Cr.NS_BASE_STREAM_CLOSED) {
            resolve(result);
          } else {
            reject(e);
          }
        }
      }
      stream.asyncWait(readData, 0, 0, Services.tm.currentThread);
    });
  },

  async recordContentBlockingLog(principal, inputStream) {
    let json = await this._readAsyncStream(inputStream);
    // Simulate parsing this for perf tests.
    let log = JSON.parse(json);
    void log;
  },
});

var EXPORTED_SYMBOLS = ["TrackingDBService"];
