/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import { GeckoViewSessionStore } from "resource://gre/modules/GeckoViewSessionStore.sys.mjs";

export class SessionStoreFunctions {
  UpdateSessionStore(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aEpoch,
    aCollectSHistory,
    aData
  ) {
    return GeckoViewSessionStoreFuncInternal.updateSessionStore(
      aBrowser,
      aBrowsingContext,
      aPermanentKey,
      aEpoch,
      aCollectSHistory,
      aData
    );
  }
}

var GeckoViewSessionStoreFuncInternal = {
  updateSessionStore: function SSF_updateSessionStore(
    aBrowser,
    aBrowsingContext,
    aPermanentKey,
    aEpoch,
    aCollectSHistory,
    aData
  ) {
    const { formdata, scroll } = aData;

    if (formdata) {
      aData.formdata = formdata.toJSON();
    }

    if (scroll) {
      aData.scroll = scroll.toJSON();
    }

    GeckoViewSessionStore.updateSessionStoreFromTabListener(
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
};

SessionStoreFunctions.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsISessionStoreFunctions",
]);
