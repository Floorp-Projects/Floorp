/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const port = browser.runtime.connectNative("browser");

port.onMessage.addListener(async message => {
  switch (message.type) {
    case "SetPrefs":
      {
        const {
          id,
          args: { oldPrefs, newPrefs },
        } = message;
        sendResponse(id, browser.test.setPrefs(oldPrefs, newPrefs));
      }
      break;

    case "RestorePrefs":
      {
        const {
          id,
          args: { oldPrefs },
        } = message;
        sendResponse(id, browser.test.restorePrefs(oldPrefs));
      }
      break;

    case "GetPrefs":
      {
        const {
          id,
          args: { prefs },
        } = message;
        sendResponse(id, browser.test.getPrefs(prefs));
      }
      break;

    case "GetLinkColor":
      {
        const {
          id,
          args: { uri, selector },
        } = message;
        sendResponse(id, browser.test.getLinkColor(uri, selector));
      }
      break;

    case "GetRequestedLocales":
      {
        const { id } = message;
        sendResponse(id, browser.test.getRequestedLocales());
      }
      break;

    case "AddHistogram":
      {
        const {
          id,
          args: { id: histogramId, value },
        } = message;
        browser.test.addHistogram(histogramId, value);
        sendResponse(id, null);
      }
      break;
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
