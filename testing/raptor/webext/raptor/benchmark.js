/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// receives result from benchmark and relays onto our background runner

function receiveMessage(event) {
  raptorLog("raptor benchmark received message");
  raptorLog(event.data);
  // raptor benchmark message data [0] is raptor tag, [1] is benchmark
  // name, and the rest is actual benchmark results that we want to fw
  if (event.data[0] == "raptor-benchmark") {
    sendResult(event.data[1], event.data.slice(2));
  }
}

function sendResult(_type, _value) {
  // send result back to background runner script
  raptorLog(`sending result back to runner: ${_type} ${_value}`);
  chrome.runtime.sendMessage({ type: _type, value: _value }, function(
    response
  ) {
    raptorLog(response.text);
  });
}

function raptorLog(text, level = "info") {
  let prefix = "";

  if (level == "error") {
    prefix = "ERROR: ";
  }

  console[level](`${prefix}[raptor-benchmarkjs] ${text}`);
}

raptorLog("raptor benchmark content loaded");
window.addEventListener("message", receiveMessage);
