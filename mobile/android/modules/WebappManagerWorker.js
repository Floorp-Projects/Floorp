/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

importScripts("resource://gre/modules/osfile.jsm");
importScripts("resource://gre/modules/workers/require.js");
let Log = require("resource://gre/modules/AndroidLog.jsm");

// Define the "log" function as a binding of the Log.d function so it specifies
// the "debug" priority and a log tag.
let log = Log.d.bind(null, "WebappManagerWorker");

onmessage = function(event) {
  let { url, path } = event.data;

  let file = OS.File.open(path, { truncate: true });
  let request = new XMLHttpRequest({ mozSystem: true });

  request.open("GET", url, true);
  request.responseType = "moz-chunked-arraybuffer";

  request.onprogress = function(event) {
    log("onprogress: received " + request.response.byteLength + " bytes");
    let bytesWritten = file.write(new Uint8Array(request.response));
    log("onprogress: wrote " + bytesWritten + " bytes");
  };

  request.onreadystatechange = function(event) {
    log("onreadystatechange: " + request.readyState);

    if (request.readyState !== 4) {
      return;
    }

    file.close();

    if (request.status === 200) {
      postMessage({ type: "success" });
    } else {
      try {
        OS.File.remove(path);
      } catch(ex) {
        log("error removing " + path + ": " + ex);
      }
      let statusMessage = request.status + " - " + request.statusText;
      postMessage({ type: "failure", message: statusMessage });
    }
  };

  request.send(null);
}
