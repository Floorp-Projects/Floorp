/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
const { SessionStore } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionStore.jsm"
);

function UpdateSessionStore(
  aBrowser,
  aBrowsingContext,
  aPermanentKey,
  aEpoch,
  aCollectSHistory,
  aData
) {
  return SessionStoreFuncInternal.updateSessionStore(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aEpoch,
    aCollectSHistory,
    aData
  );
}

function UpdateSessionStoreForStorage(
  aBrowser,
  aBrowsingContext,
  aPermanentKey,
  aEpoch,
  aData
) {
  return SessionStoreFuncInternal.updateSessionStoreForStorage(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aEpoch,
    aData
  );
}

var EXPORTED_SYMBOLS = ["UpdateSessionStore", "UpdateSessionStoreForStorage"];

var SessionStoreFuncInternal = {
  updateSessionStore: function SSF_updateSessionStore(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aEpoch,
    aCollectSHistory,
    aData
  ) {
    let { formdata, scroll } = aData;

    if (formdata) {
      aData.formdata = formdata.toJSON();
    }

    if (scroll) {
      aData.scroll = scroll.toJSON();
    }

    SessionStore.updateSessionStoreFromTablistener(
      aBrowser,
      aBrowsingContext,
      aPermanentKey,
      {
        data: aData,
        epoch: aEpoch,
        sHistoryNeeded: aCollectSHistory,
      }
    );
  },

  updateSessionStoreForStorage: function SSF_updateSessionStoreForWindow(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aEpoch,
    aData
  ) {
    SessionStore.updateSessionStoreFromTablistener(
      aBrowser,
      aBrowsingContext,
      aPermanentKey,
      { data: { storage: aData }, epoch: aEpoch }
    );
  },
};
