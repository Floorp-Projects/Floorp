/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

function UpdateSessionStore(
  aBrowser,
  aBrowsingContext,
  aEpoch,
  aData,
  aCollectSHistory
) {
  return SessionStoreFuncInternal.updateSessionStore(
    aBrowser,
    aBrowsingContext,
    aEpoch,
    aData,
    aCollectSHistory
  );
}

function UpdateSessionStoreForWindow(
  aBrowser,
  aBrowsingContext,
  aEpoch,
  aData
) {
  return SessionStoreFuncInternal.updateSessionStoreForWindow(
    aBrowser,
    aBrowsingContext,
    aEpoch,
    aData
  );
}

var EXPORTED_SYMBOLS = ["UpdateSessionStore", "UpdateSessionStoreForWindow"];

var SessionStoreFuncInternal = {
  updateStorage: function SSF_updateStorage(aOrigins, aKeys, aValues) {
    let data = {};
    for (let i = 0; i < aOrigins.length; i++) {
      // If the key isn't defined, then .clear() was called, and we send
      // up null for this domain to indicate that storage has been cleared
      // for it.
      if (aKeys[i] == "") {
        while (aOrigins[i + 1] == aOrigins[i]) {
          i++;
        }
        data[aOrigins[i]] = null;
      } else {
        let hostData = {};
        hostData[aKeys[i]] = aValues[i];
        while (aOrigins[i + 1] == aOrigins[i]) {
          i++;
          hostData[aKeys[i]] = aValues[i];
        }
        data[aOrigins[i]] = hostData;
      }
    }
    if (aOrigins.length) {
      return data;
    }

    return null;
  },

  updateSessionStore: function SSF_updateSessionStore(
    aBrowser,
    aBrowsingContext,
    aEpoch,
    aData,
    aCollectSHistory
  ) {
    let currentData = {};
    if (aData.docShellCaps != undefined) {
      currentData.disallow = aData.docShellCaps ? aData.docShellCaps : null;
    }
    if (aData.isPrivate != undefined) {
      currentData.isPrivate = aData.isPrivate;
    }

    if (aData.isFullStorage != undefined) {
      let storage = this.updateStorage(
        aData.storageOrigins,
        aData.storageKeys,
        aData.storageValues
      );
      if (aData.isFullStorage) {
        currentData.storage = storage;
      } else {
        currentData.storagechange = storage;
      }
    }

    SessionStore.updateSessionStoreFromTablistener(aBrowser, aBrowsingContext, {
      data: currentData,
      epoch: aEpoch,
      sHistoryNeeded: aCollectSHistory,
    });
  },
  updateSessionStoreForWindow: function SSF_updateSessionStoreForWindow(
    aBrowser,
    aBrowsingContext,
    aEpoch,
    aData
  ) {
    let windowstatechange = aData;

    SessionStore.updateSessionStoreFromTablistener(aBrowser, aBrowsingContext, {
      data: { windowstatechange },
      epoch: aEpoch,
    });
  },
};
