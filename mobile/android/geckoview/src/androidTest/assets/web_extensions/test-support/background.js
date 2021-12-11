/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const port = browser.runtime.connectNative("browser");

const APIS = {
  AddHistogram({ id, value }) {
    browser.test.addHistogram(id, value);
  },
  Eval({ code }) {
    // eslint-disable-next-line no-eval
    return eval(`(async () => {
      ${code}
    })()`);
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
  GetProfilePath() {
    return browser.test.getProfilePath();
  },
  GetAllBrowserPids() {
    return browser.test.getAllBrowserPids();
  },
  KillContentProcess({ pid }) {
    return browser.test.killContentProcess(pid);
  },
  GetPrefs({ prefs }) {
    return browser.test.getPrefs(prefs);
  },
  GetActive({ tab }) {
    return browser.test.getActive(tab.id);
  },
  RemoveAllCertOverrides() {
    browser.test.removeAllCertOverrides();
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
  FlushApzRepaints({ tab }) {
    return browser.test.flushApzRepaints(tab.id);
  },
  PromiseAllPaintsDone({ tab }) {
    return browser.test.promiseAllPaintsDone(tab.id);
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
  try {
    sendResponse(id, impl(args));
  } catch (error) {
    sendResponse(id, Promise.reject(error));
  }
}

function sendResponse(id, response) {
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
