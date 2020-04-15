/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PurgeTrackerService"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gClassifier",
  "@mozilla.org/url-classifier/dbservice;1",
  "nsIURIClassifier"
);

this.PurgeTrackerService = function() {};

PurgeTrackerService.prototype = {
  classID: Components.ID("{90d1fd17-2018-4e16-b73c-a04a26fa6dd4}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPurgeTrackerService]),

  // We can only know asynchronously if a host is matched by the tracking
  // protection list, so we cache the result for faster future lookups.
  _trackingState: new Map(),
  /**
   * We use this collator to compare strings as if they were numbers.
   * Timestamps are saved as strings since the number is too large for an int.
   **/
  collator: new Intl.Collator(undefined, {
    numeric: true,
    sensitivity: "base",
  }),

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "idle-daily":
        // only allow one idle-daily listener to trigger until the list has been fully parsed.
        Services.obs.removeObserver(this, "idle-daily");
        this.purgeTrackingCookieJars();
        break;
      case "profile-after-change":
        Services.obs.addObserver(this, "idle-daily");
        break;
    }
  },

  async isTracker(principal, feature) {
    if (principal.isNullPrincipal || principal.isSystemPrincipal) {
      return false;
    }
    let host;
    try {
      host = principal.URI.asciiHost;
    } catch (error) {
      return false;
    }

    if (!this._trackingState.has(host)) {
      // Temporarily set to false to avoid doing several lookups if a site has
      // several subframes on the same domain.
      this._trackingState.set(host, false);

      await new Promise(resolve => {
        try {
          gClassifier.asyncClassifyLocalWithFeatures(
            principal.URI,
            [feature],
            Ci.nsIUrlClassifierFeature.blacklist,
            list => {
              if (list.length) {
                this._trackingState.set(host, true);
              }
              resolve();
            }
          );
        } catch {
          // Error in asyncClassifyLocalWithFeatures, it is not a tracker.
          this._trackingState.set(host, false);
          resolve();
        }
      });
    }

    return this._trackingState.get(host);
  },

  resetPurgeList() {
    // We've reached the end of the cookies.
    // Restore the idle-daily listener so it will purge again tomorrow.
    Services.obs.addObserver(this, "idle-daily");
    // Set the date to 0 so we will start at the beginning of the list next time.
    Services.prefs.setStringPref(
      "privacy.purge_trackers.date_in_cookie_database",
      "0"
    );
  },

  /**
   * This loops through all cookies saved in the database and checks if they are a tracking cookie, if it is it checks
   * that they have an interaction permission which is still valid. If the Permission is not valid we delete all data
   * associated with the site that owns that cookie.
   */
  async purgeTrackingCookieJars() {
    let purgeEnabled = Services.prefs.getBoolPref(
      "privacy.purge_trackers.enabled",
      false
    );
    // Only purge if ETP is enabled.
    let cookieBehavior = Services.prefs.getIntPref(
      "network.cookie.cookieBehavior",
      Ci.nsICookieService.BEHAVIOR_ACCEPT
    );

    let etpActive =
      cookieBehavior == Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER ||
      cookieBehavior ==
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;

    if (!etpActive || !purgeEnabled) {
      LOG(
        `returning early, etpActive: ${etpActive}, purgeEnabled: ${purgeEnabled}`
      );
      this.resetPurgeList();
      return;
    }
    LOG("Purging trackers enabled, beginning batch.");
    // How many cookies to loop through in each batch before we quit
    const MAX_PURGE_COUNT = Services.prefs.getIntPref(
      "privacy.purge_trackers.max_purge_count",
      100
    );

    /**
     * We record the creationTime of the last cookie we looked at and
     * start from there next time. This way even if new cookies are added or old ones are deleted we
     * have a reliable way of finding our spot.
     **/
    let saved_date = Services.prefs.getStringPref(
      "privacy.purge_trackers.date_in_cookie_database",
      "0"
    );

    // TODO We only need the host name and creationTime, this gives too much info. See bug 1610373.
    let cookies = Services.cookies.cookies;

    // ensure we have only cookies that have a greater or equal creationTime than the saved creationTime.
    // TODO only get cookies that fulfill this condition. See bug 1610373.
    cookies = cookies.filter(cookie => {
      return (
        cookie.creationTime &&
        this.collator.compare(cookie.creationTime, saved_date) > 0
      );
    });

    // ensure the cookies are sorted by creationTime oldest to newest.
    // TODO get cookies in this order. See bug 1610373.
    cookies.sort((a, b) =>
      this.collator.compare(a.creationTime, b.creationTime)
    );
    cookies = cookies.slice(0, MAX_PURGE_COUNT);

    let feature = gClassifier.getFeatureByName("tracking-annotation");
    if (!feature) {
      LOG("returning early, feature undefined.");
      this.resetPurgeList();
      return;
    }

    let baseDomainsWithInteraction = new Set();
    for (let perm of Services.perms.getAllWithTypePrefix("storageAccessAPI")) {
      baseDomainsWithInteraction.add(perm.principal.baseDomain);
    }

    for (let cookie of cookies) {
      let httpsPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        "https://" +
          cookie.rawHost +
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
      );

      // Either the interaction permission was never granted or it expired.
      if (!baseDomainsWithInteraction.has(httpsPrincipal.baseDomain)) {
        let httpPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
          "http://" +
            cookie.rawHost +
            ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
        );

        // We purge if we also find it is a tracker.
        let isTracker =
          (await this.isTracker(httpsPrincipal, feature)) ||
          (await this.isTracker(httpPrincipal, feature));
        if (isTracker) {
          LOG(
            "tracking cookie found with no interaction permission, deleting related data.",
            cookie.rawHost
          );
          await new Promise(resolve => {
            Services.clearData.deleteDataFromPrincipal(
              httpsPrincipal,
              false,
              Ci.nsIClearDataService.CLEAR_ALL_CACHES |
                Ci.nsIClearDataService.CLEAR_COOKIES |
                Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
                Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
                Ci.nsIClearDataService.CLEAR_EME |
                Ci.nsIClearDataService.CLEAR_PLUGIN_DATA |
                Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES |
                Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS |
                Ci.nsIClearDataService.CLEAR_AUTH_TOKENS |
                Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
              resolve
            );
          });
          await new Promise(resolve => {
            Services.clearData.deleteDataFromPrincipal(
              httpPrincipal,
              false,
              Ci.nsIClearDataService.CLEAR_ALL_CACHES |
                Ci.nsIClearDataService.CLEAR_COOKIES |
                Ci.nsIClearDataService.CLEAR_DOM_STORAGES |
                Ci.nsIClearDataService.CLEAR_SECURITY_SETTINGS |
                Ci.nsIClearDataService.CLEAR_EME |
                Ci.nsIClearDataService.CLEAR_PLUGIN_DATA |
                Ci.nsIClearDataService.CLEAR_MEDIA_DEVICES |
                Ci.nsIClearDataService.CLEAR_STORAGE_ACCESS |
                Ci.nsIClearDataService.CLEAR_AUTH_TOKENS |
                Ci.nsIClearDataService.CLEAR_AUTH_CACHE,
              resolve
            );
          });
          LOG(`Data deleted from: `, cookie.rawHost);
        }
      }
      saved_date = cookie.creationTime;
    }

    Services.prefs.setStringPref(
      "privacy.purge_trackers.date_in_cookie_database",
      saved_date
    );

    // We've reached the end, no need to repeat again until next idle-daily.
    if (!cookies.length || cookies.length < 100) {
      LOG("All cookie purging finished, resetting list until tomorrow.");
      this.resetPurgeList();
      return;
    }

    LOG("Batch finished, queueing next batch.");
    Services.tm.idleDispatchToMainThread(() => {
      this.purgeTrackingCookieJars();
    });
  },
};

/**
 * Outputs the message to the JavaScript console as well as to stdout.
 *
 * @param {...string} args The message to output.
 */
var logConsole;
function LOG(...args) {
  if (!logConsole) {
    logConsole = console.createInstance({
      prefix: "*** PurgeTrackerService:",
      maxLogLevelPref: "privacy.purge_trackers.logging.level",
    });
  }
  logConsole.log(...args);
}
