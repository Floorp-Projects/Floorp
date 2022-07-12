/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["BrowserTelemetryUtils"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

// The maximum number of concurrently-loaded origins allowed in order to
// qualify for the Fission rollout experiment.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "fissionExperimentMaxOrigins",
  "fission.experiment.max-origins.origin-cap",
  30
);
// The length of the sliding window during which a user must stay below
// the max origin cap. If the last time a user passed the max origin cap
// fell outside of this window, they will requalify for the experiment.
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "fissionExperimentSlidingWindowMS",
  "fission.experiment.max-origins.sliding-window-ms",
  7 * 24 * 60 * 60 * 1000
);
// The pref holding the current qaualification state of the user. If
// true, the user is currently qualified from the experiment.
const FISSION_EXPERIMENT_PREF_QUALIFIED =
  "fission.experiment.max-origins.qualified";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "fissionExperimentQualified",
  FISSION_EXPERIMENT_PREF_QUALIFIED,
  false
);
// The pref holding the timestamp of the last time we saw an origin
// count below the cap while the user was not currently marked as
// qualified.
const FISSION_EXPERIMENT_PREF_LAST_QUALIFIED =
  "fission.experiment.max-origins.last-qualified";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "fissionExperimentLastQualified",
  FISSION_EXPERIMENT_PREF_LAST_QUALIFIED,
  0
);
// The pref holding the timestamp of the last time we saw an origin
// count exceeding the cap.
const FISSION_EXPERIMENT_PREF_LAST_DISQUALIFIED =
  "fission.experiment.max-origins.last-disqualified";
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "fissionExperimentLastDisqualified",
  FISSION_EXPERIMENT_PREF_LAST_DISQUALIFIED,
  0
);

var BrowserTelemetryUtils = {
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
    }

    // Update the Fission experiment qualification state based on the
    // current origin count:

    // If we don't already have a last disqualification timestamp, look
    // through the existing histogram values, and use the existing
    // maximum value as the initial count. This will prevent us from
    // enrolling users in the experiment if they have a history of
    // exceeding our origin cap.
    if (!this._checkedInitialExperimentQualification) {
      this._checkedInitialExperimentQualification = true;
      if (
        !Services.prefs.prefHasUserValue(
          FISSION_EXPERIMENT_PREF_LAST_DISQUALIFIED
        )
      ) {
        for (let [bucketStr, entryCount] of Object.entries(
          histogram.snapshot().values
        )) {
          let bucket = Number(bucketStr);
          if (bucket > originCount && entryCount > 0) {
            originCount = bucket;
          }
        }
        Services.prefs.setIntPref(FISSION_EXPERIMENT_PREF_LAST_DISQUALIFIED, 0);
      }
    }

    let currentTimeSec = currentTime / 1000;
    if (originCount < lazy.fissionExperimentMaxOrigins) {
      let lastDisqualified = lazy.fissionExperimentLastDisqualified;
      let lastQualified = lazy.fissionExperimentLastQualified;

      // If the last time we saw a qualifying origin count was earlier
      // than the last time we say a disqualifying count, update any
      // existing last disqualified timestamp to just before now, on the
      // basis that our origin count has probably just fallen below the
      // cap.
      if (lastDisqualified > 0 && lastQualified <= lastDisqualified) {
        Services.prefs.setIntPref(
          FISSION_EXPERIMENT_PREF_LAST_DISQUALIFIED,
          currentTimeSec - 1
        );
      }

      if (!lazy.fissionExperimentQualified) {
        Services.prefs.setIntPref(
          FISSION_EXPERIMENT_PREF_LAST_QUALIFIED,
          currentTimeSec
        );

        // We have a qualifying origin count now. If the last time we were
        // disqualified was prior to the start of our current sliding
        // window, re-qualify the user.
        if (
          currentTimeSec - lastDisqualified >=
          lazy.fissionExperimentSlidingWindowMS / 1000
        ) {
          Services.prefs.setBoolPref(FISSION_EXPERIMENT_PREF_QUALIFIED, true);
        }
      }
    } else {
      Services.prefs.setIntPref(
        FISSION_EXPERIMENT_PREF_LAST_DISQUALIFIED,
        currentTimeSec
      );
      Services.prefs.setBoolPref(FISSION_EXPERIMENT_PREF_QUALIFIED, false);
    }
  },
};
