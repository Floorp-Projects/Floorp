/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "serviceWorkerManager",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) {
  throw new Error("ServiceWorkerCleanUp.jsm can only be used in the parent process");
}

this.EXPORTED_SYMBOLS = ["ServiceWorkerCleanUp"];

function unregisterServiceWorker(aSW) {
  return new Promise(resolve => {
    let unregisterCallback = {
      unregisterSucceeded: resolve,
      unregisterFailed: resolve, // We don't care about failures.
      QueryInterface: ChromeUtils.generateQI([Ci.nsIServiceWorkerUnregisterCallback]),
    };
    serviceWorkerManager.propagateUnregister(aSW.principal, unregisterCallback, aSW.scope);
  });
}

this.ServiceWorkerCleanUp = {
  removeFromHost(aHost) {
    let promises = [];
    let serviceWorkers = serviceWorkerManager.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      if (sw.principal.URI.host == aHost) {
        promises.push(unregisterServiceWorker(sw));
      }
    }
    return Promise.all(promises);
  },

  removeFromPrincipal(aPrincipal) {
    let promises = [];
    let serviceWorkers = serviceWorkerManager.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      if (sw.principal.equals(aPrincipal)) {
        promises.push(unregisterServiceWorker(sw));
      }
    }
    return Promise.all(promises);
  },

  removeAll() {
    let promises = [];
    let serviceWorkers = serviceWorkerManager.getAllRegistrations();
    for (let i = 0; i < serviceWorkers.length; i++) {
      let sw = serviceWorkers.queryElementAt(i, Ci.nsIServiceWorkerRegistrationInfo);
      promises.push(unregisterServiceWorker(sw));
    }
    return Promise.all(promises);
  },
};
