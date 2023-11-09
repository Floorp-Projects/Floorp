/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let backgroundPort = null;
let nativePort = null;

function connectNativePort() {
  if (nativePort) {
    return;
  }

  backgroundPort = browser.runtime.connect();
  nativePort = browser.runtime.connectNative("browser");

  nativePort.onMessage.addListener(message => {
    if (message.type) {
      // This is a session-specific webExtensionApiCall.
      // Forward to the background script.
      backgroundPort.postMessage(message);
    } else if (message.eval) {
      try {
        // Using eval here is the whole point of this WebExtension so we can
        // safely ignore the eslint warning.
        const response = window.eval(message.eval); // eslint-disable-line no-eval
        sendResponse(message.id, response);
      } catch (ex) {
        sendSyncResponse(message.id, null, ex);
      }
    }
  });

  function sendResponse(id, response, exception) {
    Promise.resolve(response).then(
      value => sendSyncResponse(id, value),
      reason => sendSyncResponse(id, null, reason)
    );
  }

  function sendSyncResponse(id, response, exception) {
    nativePort.postMessage({
      id,
      response: JSON.stringify(response),
      exception: exception && exception.toString(),
    });
  }
}

function disconnectNativePort() {
  backgroundPort?.disconnect();
  nativePort?.disconnect();
  backgroundPort = null;
  nativePort = null;
}

window.addEventListener("pageshow", connectNativePort);
window.addEventListener("pagehide", disconnectNativePort);

// If loading error page, pageshow mightn't fired.
connectNativePort();
