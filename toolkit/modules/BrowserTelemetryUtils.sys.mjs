/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export var BrowserTelemetryUtils = {
  recordSiteOriginTelemetry(aWindows, aIsGeckoView) {
    Services.tm.idleDispatchToMainThread(() => {
      this._recordSiteOriginTelemetry(aWindows, aIsGeckoView);
    });
  },

  computeSiteOriginCount(aWindows, aIsGeckoView) {
    // Geckoview and Desktop work differently. On desktop, aBrowser objects
    // holds an array of tabs which we can use to get the <browser> objects.
    // In Geckoview, it is apps' responsibility to keep track of the tabs, so
    // there isn't an easy way for us to get the tabs.
    let tabs = [];
    if (aIsGeckoView) {
      // To get all active windows; Each tab has its own window
      tabs = aWindows;
    } else {
      for (const win of aWindows) {
        tabs = tabs.concat(win.gBrowser.tabs);
      }
    }

    let topLevelBCs = [];

    for (const tab of tabs) {
      let browser;
      if (aIsGeckoView) {
        browser = tab.browser;
      } else {
        browser = tab.linkedBrowser;
      }

      if (browser.browsingContext) {
        // This is the top level browsingContext
        topLevelBCs.push(browser.browsingContext);
      }
    }

    return CanonicalBrowsingContext.countSiteOrigins(topLevelBCs);
  },

  _recordSiteOriginTelemetry(aWindows, aIsGeckoView) {
    let currentTime = Date.now();

    // default is 5 minutes
    if (!this.min_interval) {
      this.min_interval = Services.prefs.getIntPref(
        "telemetry.number_of_site_origin.min_interval",
        300000
      );
    }

    let originCount = this.computeSiteOriginCount(aWindows, aIsGeckoView);
    let histogram = Services.telemetry.getHistogramById(
      "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_ALL_TABS"
    );

    // Discard the first load because most of the time the first load only has 1
    // tab and 1 window open, so it is useless to report it.
    if (!this._lastRecordSiteOrigin) {
      this._lastRecordSiteOrigin = currentTime;
    } else if (currentTime >= this._lastRecordSiteOrigin + this.min_interval) {
      this._lastRecordSiteOrigin = currentTime;

      histogram.add(originCount);
      Glean.fogValidation.gvsvNumberOfUniqueSiteOriginsAllTabs.accumulateSamples(
        [originCount]
      );
    }
  },
};
