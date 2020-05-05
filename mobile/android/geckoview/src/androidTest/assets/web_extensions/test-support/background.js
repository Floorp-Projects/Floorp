/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const port = browser.runtime.connectNative("browser");

const APIS = {
  AddHistogram({ id, value }) {
    browser.test.addHistogram(id, value);
  },
  SetScalar({ id, value }) {
    browser.test.setScalar(id, value);
  },
  GetRequestedLocales() {
    return browser.test.getRequestedLocales();
  },
  GetLinkColor({ uri, selector }) {
    return browser.test.getLinkColor(uri, selector);
  },
  GetPidForTab({ tab }) {
    return browser.test.getPidForTab(tab.id);
  },
  GetPrefs({ prefs }) {
    return browser.test.getPrefs(prefs);
  },
  RemoveCertOverride({ host, port }) {
    browser.test.removeCertOverride(host, port);
  },
  RestorePrefs({ oldPrefs }) {
    return browser.test.restorePrefs(oldPrefs);
  },
  SetPrefs({ oldPrefs, newPrefs }) {
    return browser.test.setPrefs(oldPrefs, newPrefs);
  },
  SetResolutionAndScaleTo({ resolution }) {
    return browser.test.setResolutionAndScaleTo(resolution);
  },
};

port.onMessage.addListener(async message => {
  const impl = APIS[message.type];
  apiCall(message, impl);
});

browser.runtime.onConnect.addListener(contentPort => {
  contentPort.onMessage.addListener(message => {
    message.args.tab = contentPort.sender.tab;

    const impl = APIS[message.type];
    apiCall(message, impl);
  });
});

function apiCall(message, impl) {
  const { id, args } = message;
  sendResponse(id, impl(args));
}

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
