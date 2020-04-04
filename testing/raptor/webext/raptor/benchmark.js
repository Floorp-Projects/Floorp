/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// receives result from benchmark and relays onto our background runner

async function receiveMessage(event) {
  raptorLog("raptor benchmark received message");
  raptorLog(event.data);

  // raptor benchmark message data [0] is raptor tag, [1] is benchmark
  // name, and the rest is actual benchmark results that we want to fw
  if (event.data[0] == "raptor-benchmark") {
    await sendResult(event.data[1], event.data.slice(2));
  }
}

/**
 * Send result back to background runner script
 */
async function sendResult(type, value) {
  raptorLog(`sending result back to runner: ${type} ${value}`);

  let response;
  if (typeof browser !== "undefined") {
    response = await browser.runtime.sendMessage({ type, value });
  } else {
    response = await new Promise(resolve => {
      chrome.runtime.sendMessage({ type, value }, resolve);
    });
  }

  if (response) {
    raptorLog(`Response: ${response.text}`);
  }
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
