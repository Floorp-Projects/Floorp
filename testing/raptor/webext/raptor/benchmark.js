/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// receives result from benchmark and relays onto our background runner

function receiveMessage(event) {
  console.log("raptor benchmark received message");
  console.log(event.data);
  // raptor benchmark message data [0] is raptor tag, [1] is benchmark
  // name, and the rest is actual benchmark results that we want to fw
  if (event.data[0] == "raptor-benchmark") {
    sendResult(event.data[1], event.data.slice(2));
  }
}

function sendResult(_type, _value) {
  // send result back to background runner script
  console.log(`sending result back to runner: ${_type} ${_value}`);
  chrome.runtime.sendMessage({"type": _type, "value": _value}, function(response) {
    console.log(response.text);
  });
}

console.log("raptor benchmark content loaded");
window.addEventListener("message", receiveMessage);
