// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["NetworkInfoServiceAndroid"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AndroidLog.jsm");

var log = AndroidLog.d.bind(null, "NetworkInfoServiceAndroid");

const FAILURE_INTERNAL_ERROR = -65537;
const MSG_TAG = 'NetworkInfoService';

// Helper function for sending commands to Java.
function send(type, data, callback) {
  if (type[0] == ":") {
    type = MSG_TAG + type;
  }
  let msg = { type };

  for (let i in data) {
    try {
      msg[i] = data[i];
    } catch (e) {
    }
  }

  Messaging.sendRequestForResult(msg)
    .then(result => callback(result, null),
          err => callback(null, typeof err === "number" ? err : FAILURE_INTERNAL_ERROR));
}

class NetworkInfoServiceAndroid {
  constructor() {
  }

  listNetworkAddresses(aListener) {
    send(":ListNetworkAddresses", {}, (result, err) => {
      if (err) {
        log("ListNetworkAddresses Failed: (" + err + ")");
        aListener.onListNetworkAddressesFailed();
      } else {
        log("ListNetworkAddresses Succeeded: (" + JSON.stringify(result) + ")");
        aListener.onListedNetworkAddresses(result);
      }
    });
  }

  getHostname(aListener) {
    send(":GetHostname", {}, (result, err) => {
      if (err) {
        log("GetHostname Failed: (" + err + ")");
        aListener.onGetHostnameFailed();
      } else {
        log("GetHostname Succeeded: (" + JSON.stringify(result) + ")");
        aListener.onGotHostname(result);
      }
    });
  }
}
