/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "serviceWorkerManager",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

this.EXPORTED_SYMBOLS = ["ServiceWorkerCleanUp"];

function unregisterServiceWorker(aSW) {
  return new Promise(resolve => {
    let unregisterCallback = {
      unregisterSucceeded: resolve,
      unregisterFailed: resolve, // We don't care about failures.
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIServiceWorkerUnregisterCallback])
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
