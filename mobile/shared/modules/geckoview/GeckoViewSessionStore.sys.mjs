/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.sys.mjs",
});

const { debug, warn } = GeckoViewUtils.initLogging("SessionStore");
const kNoIndex = Number.MAX_SAFE_INTEGER;
const kLastIndex = Number.MAX_SAFE_INTEGER - 1;

class SHistoryListener {
  constructor(browsingContext) {
    this.QueryInterface = ChromeUtils.generateQI([
      "nsISHistoryListener",
      "nsISupportsWeakReference",
    ]);

    this._browserId = browsingContext.browserId;
    this._fromIndex = kNoIndex;
  }

  unregister(permanentKey) {
    const bc = BrowsingContext.getCurrentTopByBrowserId(this._browserId);
    bc?.sessionHistory?.removeSHistoryListener(this);
    GeckoViewSessionStore._browserSHistoryListener?.delete(permanentKey);
  }

  collect(
    permanentKey, // eslint-disable-line no-shadow
    browsingContext, // eslint-disable-line no-shadow
    { collectFull = true, writeToCache = false }
  ) {
    // Don't bother doing anything if we haven't seen any navigations.
    if (!collectFull && this._fromIndex === kNoIndex) {
      return null;
    }

    const fromIndex = collectFull ? -1 : this._fromIndex;
    this._fromIndex = kNoIndex;

    const historychange = lazy.SessionHistory.collectFromParent(
      browsingContext.currentURI?.spec,
      true, // Bug 1704574
      browsingContext.sessionHistory,
      fromIndex
    );

    if (writeToCache) {
      const win =
        browsingContext.embedderElement?.ownerGlobal ||
        browsingContext.currentWindowGlobal?.browsingContext?.window;

      GeckoViewSessionStore.onTabStateUpdate(permanentKey, win, {
        data: { historychange },
      });
    }

    return historychange;
  }

  collectFrom(index) {
    if (this._fromIndex <= index) {
      // If we already know that we need to update history from index N we
      // can ignore any changes that happened with an element with index
      // larger than N.
      //
      // Note: initially we use kNoIndex which is MAX_SAFE_INTEGER which
      // means we don't ignore anything here, and in case of navigation in
      // the history back and forth cases we use kLastIndex which ignores
      // only the subsequent navigations, but not any new elements added.
      return;
    }

    const bc = BrowsingContext.getCurrentTopByBrowserId(this._browserId);
    if (bc?.embedderElement?.frameLoader) {
      this._fromIndex = index;

      // Queue a tab state update on the |browser.sessionstore.interval|
      // timer. We'll call this.collect() when we receive the update.
      bc.embedderElement.frameLoader.requestSHistoryUpdate();
    }
  }

  OnHistoryNewEntry(newURI, oldIndex) {
    // We use oldIndex - 1 to collect the current entry as well. This makes
    // sure to collect any changes that were made to the entry while the
    // document was active.
    this.collectFrom(oldIndex == -1 ? oldIndex : oldIndex - 1);
  }
  OnHistoryGotoIndex() {
    this.collectFrom(kLastIndex);
  }
  OnHistoryPurge() {
    this.collectFrom(-1);
  }
  OnHistoryReload() {
    this.collectFrom(-1);
    return true;
  }
  OnHistoryReplaceEntry() {
    this.collectFrom(-1);
  }
}

export var GeckoViewSessionStore = {
  // For each <browser> element, records the SHistoryListener.
  _browserSHistoryListener: new WeakMap(),

  observe(aSubject, aTopic) {
    debug`observe ${aTopic}`;

    switch (aTopic) {
      case "browsing-context-did-set-embedder": {
        if (aSubject === aSubject.top && aSubject.isContent) {
          const permanentKey = aSubject.embedderElement?.permanentKey;
          if (permanentKey) {
            this.maybeRecreateSHistoryListener(permanentKey, aSubject);
          }
        }
        break;
      }
      case "browsing-context-discarded":
        const permanentKey = aSubject?.embedderElement?.permanentKey;
        if (permanentKey) {
          this._browserSHistoryListener
            .get(permanentKey)
            ?.unregister(permanentKey);
        }
        break;
    }
  },

  onTabStateUpdate(permanentKey, win, data) {
    win.WindowEventDispatcher.sendRequest({
      type: "GeckoView:StateUpdated",
      data: data.data,
    });
  },

  getOrCreateSHistoryListener(permanentKey, browsingContext) {
    if (!permanentKey || browsingContext !== browsingContext.top) {
      return null;
    }

    const listener = this._browserSHistoryListener.get(permanentKey);
    if (listener) {
      return listener;
    }

    return this.createSHistoryListener(permanentKey, browsingContext, false);
  },

  maybeRecreateSHistoryListener(permanentKey, browsingContext) {
    const listener = this._browserSHistoryListener.get(permanentKey);
    if (!listener || listener._browserId != browsingContext.browserId) {
      listener?.unregister(permanentKey);
      this.createSHistoryListener(permanentKey, browsingContext, true);
    }
  },

  createSHistoryListener(permanentKey, browsingContext, collectImmediately) {
    const sessionHistory = browsingContext.sessionHistory;
    if (!sessionHistory) {
      return null;
    }

    const listener = new SHistoryListener(browsingContext);
    sessionHistory.addSHistoryListener(listener);
    this._browserSHistoryListener.set(permanentKey, listener);

    if (
      collectImmediately &&
      (!(browsingContext.currentURI?.spec === "about:blank") ||
        sessionHistory.count !== 0)
    ) {
      listener.collect(permanentKey, browsingContext, { writeToCache: true });
    }

    return listener;
  },

  updateSessionStoreFromTabListener(
    browser,
    browsingContext,
    permanentKey,
    update,
    forStorage = false
  ) {
    permanentKey = browser?.permanentKey ?? permanentKey;
    if (!permanentKey) {
      return;
    }

    if (browsingContext.isReplaced) {
      return;
    }

    const listener = this.getOrCreateSHistoryListener(
      permanentKey,
      browsingContext
    );

    if (listener) {
      const historychange =
        // If it is not the scheduled update (tab closed, window closed etc),
        // try to store the loading non-web-controlled page opened in _blank
        // first.
        (forStorage &&
          lazy.SessionHistory.collectNonWebControlledBlankLoadingSession(
            browsingContext
          )) ||
        listener.collect(permanentKey, browsingContext, {
          collectFull: !!update.sHistoryNeeded,
          writeToCache: false,
        });

      if (historychange) {
        update.data.historychange = historychange;
      }
    }

    const win =
      browsingContext.embedderElement?.ownerGlobal ||
      browsingContext.currentWindowGlobal?.browsingContext?.window;

    this.onTabStateUpdate(permanentKey, win, update);
  },
};
