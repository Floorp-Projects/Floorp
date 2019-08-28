/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let port = null;

window.addEventListener("pageshow", () => {
  port = browser.runtime.connectNative("browser");

  port.onMessage.addListener(message => {
    if (message.eval) {
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
    port.postMessage({
      id,
      response: JSON.stringify(response),
      exception: exception && exception.toString(),
    });
  }
});

window.addEventListener("pagehide", () => {
  port.disconnect();
});
