/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.readAsyncStream = function(stream) {
  return new Promise(function(resolve, reject) {
    let result = "";
    let source = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
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
};

var EXPORTED_SYMBOLS = ["readAsyncStream"];
