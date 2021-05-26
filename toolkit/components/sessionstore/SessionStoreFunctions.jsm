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

function UpdateSessionStoreForStorage(
  aBrowser,
  aBrowsingContext,
  aEpoch,
  aData
) {
  return SessionStoreFuncInternal.updateSessionStoreForStorage(
    aBrowser,
    aBrowsingContext,
    aEpoch,
    aData
  );
}

var EXPORTED_SYMBOLS = [
  "UpdateSessionStore",
  "UpdateSessionStoreForWindow",
  "UpdateSessionStoreForStorage",
];

var SessionStoreFuncInternal = {
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

  updateSessionStoreForStorage: function SSF_updateSessionStoreForWindow(
    aBrowser,
    aBrowsingContext,
    aEpoch,
    aData
  ) {
    SessionStore.updateSessionStoreFromTablistener(aBrowser, aBrowsingContext, {
      data: { storage: aData },
      epoch: aEpoch,
    });
  },
};
