/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "serviceWorkerManager",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) {
  throw new Error(
    "ServiceWorkerCleanUp.sys.mjs can only be used in the parent process"
  );
}

function unregisterServiceWorker(aSW) {
  return new Promise(resolve => {
    let unregisterCallback = {
      unregisterSucceeded: resolve,
      unregisterFailed: resolve, // We don't care about failures.
      QueryInterface: ChromeUtils.generateQI([
        "nsIServiceWorkerUnregisterCallback",
      ]),
    };
    lazy.serviceWorkerManager.propagateUnregister(
      aSW.principal,
      unregisterCallback,
      aSW.scope
    );
  });
}

function unregisterServiceWorkersMatching(filterFn) {
  let promises = [];
  let serviceWorkers = lazy.serviceWorkerManager.getAllRegistrations();
  for (let i = 0; i < serviceWorkers.length; i++) {
    let sw = serviceWorkers.queryElementAt(
      i,
      Ci.nsIServiceWorkerRegistrationInfo
    );
    if (filterFn(sw)) {
      promises.push(unregisterServiceWorker(sw));
    }
  }
  return Promise.all(promises);
}

export const ServiceWorkerCleanUp = {
  removeFromHost(aHost) {
    return unregisterServiceWorkersMatching(sw =>
      Services.eTLD.hasRootDomain(sw.principal.host, aHost)
    );
  },

  removeFromBaseDomain(aBaseDomain) {
    // Service workers are disabled in partitioned contexts. This means we don't
    // have to check for a partitionKey, but only look at the top level base
    // domain. If this ever changes we need to update this method to account for
    // partitions. See Bug 1495241.
    return unregisterServiceWorkersMatching(
      sw => sw.principal.baseDomain == aBaseDomain
    );
  },

  removeFromPrincipal(aPrincipal) {
    return unregisterServiceWorkersMatching(sw =>
      sw.principal.equals(aPrincipal)
    );
  },

  removeFromOriginAttributes(aOriginAttributesString) {
    lazy.serviceWorkerManager.removeRegistrationsByOriginAttributes(
      aOriginAttributesString
    );
    return Promise.resolve();
  },

  removeAll() {
    return unregisterServiceWorkersMatching(() => true);
  },
};
