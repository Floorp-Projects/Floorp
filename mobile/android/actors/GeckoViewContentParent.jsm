/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContentParent"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewContentParent");

class GeckoViewContentParent extends JSWindowActorParent {
  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  async collectState() {
    return this.sendQuery("CollectSessionState");
  }

  restoreState({ history, loadOptions, formdata, scrolldata }) {
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
      loadOptions,
    });

    if (!formdata && !scrolldata) {
      return;
    }

    const self = this;
    const progressFilter = Cc[
      "@mozilla.org/appshell/component/browser-status-filter;1"
    ].createInstance(Ci.nsIWebProgress);

    const progressListener = {
      QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener"]),

      onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
        self.sendAsyncMessage("RestoreSessionState", { formdata, scrolldata });
        progressFilter.removeProgressListener(this);
        self.browser.removeProgressListener(progressFilter);
      },
    };

    const flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
    progressFilter.addProgressListener(progressListener, flags);
    this.browser.addProgressListener(progressFilter, flags);
  }
}
