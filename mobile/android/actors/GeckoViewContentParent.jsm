/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContentParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { GeckoViewActorParent } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "SessionHistory",
  "resource://gre/modules/sessionstore/SessionHistory.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewContentParent");

class GeckoViewContentParent extends GeckoViewActorParent {
  async collectState() {
    return this.sendQuery("CollectSessionState");
  }

  restoreState({ history, switchId, formdata, scrolldata }) {
    if (Services.appinfo.sessionHistoryInParent) {
      const { browsingContext } = this.browser;
      lazy.SessionHistory.restoreFromParent(
        browsingContext.sessionHistory,
        history
      );

      // TODO Bug 1648158 this should include scroll, form history, etc
      return SessionStoreUtils.initializeRestore(
        browsingContext,
        SessionStoreUtils.constructSessionStoreRestoreData()
      );
    }

    // Restoring is made of two parts. First we need to restore the history
    // of the tab and navigating to the current page, after the page
    // navigates to the current page we need to restore the state of the
    // page (scroll position, form data, etc).
    //
    // We can't do everything in one step inside the child actor because
    // the actor is recreated when navigating, so we need to keep the state
    // on the parent side until we navigate.
    this.sendAsyncMessage("RestoreHistoryAndNavigate", {
      history,
      switchId,
    });

    if (!formdata && !scrolldata) {
      return null;
    }

    const progressFilter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);

    const { browser } = this;
    const progressListener = {
      QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener"]),

      onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
        if (!aWebProgress.isTopLevel) {
          return;
        }
        // The actor might get recreated between navigations so we need to
        // query it again for the up-to-date instance.
        browser.browsingContext.currentWindowGlobal
          .getActor("GeckoViewContent")
          .sendAsyncMessage("RestoreSessionState", { formdata, scrolldata });
        progressFilter.removeProgressListener(this);
        browser.removeProgressListener(progressFilter);
      },
    };

    const flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
    progressFilter.addProgressListener(progressListener, flags);
    browser.addProgressListener(progressFilter, flags);
    return null;
  }
}
