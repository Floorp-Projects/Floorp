"use strict";

var EXPORTED_SYMBOLS = ["UrlClassifierTestUtils"];

const ANNOTATION_TABLE_NAME = "mochitest1-track-simple";
const ANNOTATION_TABLE_PREF = "urlclassifier.trackingAnnotationTable";
const ANNOTATION_ENTITYLIST_TABLE_NAME = "mochitest1-trackwhite-simple";
const ANNOTATION_ENTITYLIST_TABLE_PREF =
  "urlclassifier.trackingAnnotationWhitelistTable";

const TRACKING_TABLE_NAME = "mochitest2-track-simple";
const TRACKING_TABLE_PREF = "urlclassifier.trackingTable";
const ENTITYLIST_TABLE_NAME = "mochitest2-trackwhite-simple";
const ENTITYLIST_TABLE_PREF = "urlclassifier.trackingWhitelistTable";

const SOCIAL_ANNOTATION_TABLE_NAME = "mochitest3-track-simple";
const SOCIAL_ANNOTATION_TABLE_PREF =
  "urlclassifier.features.socialtracking.annotate.blacklistTables";
const SOCIAL_TRACKING_TABLE_NAME = "mochitest4-track-simple";
const SOCIAL_TRACKING_TABLE_PREF =
  "urlclassifier.features.socialtracking.blacklistTables";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

var UrlClassifierTestUtils = {
  addTestTrackers() {
    // Add some URLs to the tracking databases
    let annotationURL1 = "tracking.example.org/"; // only for annotations
    let annotationURL2 = "itisatracker.org/";
    let annotationURL3 = "trackertest.org/";
    let annotationURL4 = "another-tracking.example.net/";
    let annotationURL5 = "tlsresumptiontest.example.org/";
    let annotationEntitylistedURL = "itisatrap.org/?resource=example.org";
    let trackingURL1 = "tracking.example.com/"; // only for TP
    let trackingURL2 = "itisatracker.org/";
    let trackingURL3 = "trackertest.org/";
    let entitylistedURL = "itisatrap.org/?resource=itisatracker.org";
    let socialTrackingURL = "social-tracking.example.org/";

    let annotationUpdate =
      "n:1000\ni:" +
      ANNOTATION_TABLE_NAME +
      "\nad:5\n" +
      "a:1:32:" +
      annotationURL1.length +
      "\n" +
      annotationURL1 +
      "\n" +
      "a:2:32:" +
      annotationURL2.length +
      "\n" +
      annotationURL2 +
      "\n" +
      "a:3:32:" +
      annotationURL3.length +
      "\n" +
      annotationURL3 +
      "\n" +
      "a:4:32:" +
      annotationURL4.length +
      "\n" +
      annotationURL4 +
      "\n" +
      "a:5:32:" +
      annotationURL5.length +
      "\n" +
      annotationURL5 +
      "\n";
    let socialAnnotationUpdate =
      "n:1000\ni:" +
      SOCIAL_ANNOTATION_TABLE_NAME +
      "\nad:1\n" +
      "a:1:32:" +
      socialTrackingURL.length +
      "\n" +
      socialTrackingURL +
      "\n";
    let annotationEntitylistUpdate =
      "n:1000\ni:" +
      ANNOTATION_ENTITYLIST_TABLE_NAME +
      "\nad:1\n" +
      "a:1:32:" +
      annotationEntitylistedURL.length +
      "\n" +
      annotationEntitylistedURL +
      "\n";
    let trackingUpdate =
      "n:1000\ni:" +
      TRACKING_TABLE_NAME +
      "\nad:3\n" +
      "a:1:32:" +
      trackingURL1.length +
      "\n" +
      trackingURL1 +
      "\n" +
      "a:2:32:" +
      trackingURL2.length +
      "\n" +
      trackingURL2 +
      "\n" +
      "a:3:32:" +
      trackingURL3.length +
      "\n" +
      trackingURL3 +
      "\n";
    let socialTrackingUpdate =
      "n:1000\ni:" +
      SOCIAL_TRACKING_TABLE_NAME +
      "\nad:1\n" +
      "a:1:32:" +
      socialTrackingURL.length +
      "\n" +
      socialTrackingURL +
      "\n";
    let entitylistUpdate =
      "n:1000\ni:" +
      ENTITYLIST_TABLE_NAME +
      "\nad:1\n" +
      "a:1:32:" +
      entitylistedURL.length +
      "\n" +
      entitylistedURL +
      "\n";

    var tables = [
      {
        pref: ANNOTATION_TABLE_PREF,
        name: ANNOTATION_TABLE_NAME,
        update: annotationUpdate,
      },
      {
        pref: SOCIAL_ANNOTATION_TABLE_PREF,
        name: SOCIAL_ANNOTATION_TABLE_NAME,
        update: socialAnnotationUpdate,
      },
      {
        pref: ANNOTATION_ENTITYLIST_TABLE_PREF,
        name: ANNOTATION_ENTITYLIST_TABLE_NAME,
        update: annotationEntitylistUpdate,
      },
      {
        pref: TRACKING_TABLE_PREF,
        name: TRACKING_TABLE_NAME,
        update: trackingUpdate,
      },
      {
        pref: SOCIAL_TRACKING_TABLE_PREF,
        name: SOCIAL_TRACKING_TABLE_NAME,
        update: socialTrackingUpdate,
      },
      {
        pref: ENTITYLIST_TABLE_PREF,
        name: ENTITYLIST_TABLE_NAME,
        update: entitylistUpdate,
      },
    ];

    let tableIndex = 0;
    let doOneUpdate = () => {
      if (tableIndex == tables.length) {
        return Promise.resolve();
      }
      return this.useTestDatabase(tables[tableIndex]).then(
        () => {
          tableIndex++;
          return doOneUpdate();
        },
        aErrMsg => {
          dump("Rejected: " + aErrMsg + ". Retry later.\n");
          return new Promise(resolve => {
            timer.initWithCallback(resolve, 100, Ci.nsITimer.TYPE_ONE_SHOT);
          }).then(doOneUpdate);
        }
      );
    };

    return doOneUpdate();
  },

  cleanupTestTrackers() {
    Services.prefs.clearUserPref(ANNOTATION_TABLE_PREF);
    Services.prefs.clearUserPref(SOCIAL_ANNOTATION_TABLE_PREF);
    Services.prefs.clearUserPref(ANNOTATION_ENTITYLIST_TABLE_PREF);
    Services.prefs.clearUserPref(TRACKING_TABLE_PREF);
    Services.prefs.clearUserPref(SOCIAL_TRACKING_TABLE_PREF);
    Services.prefs.clearUserPref(ENTITYLIST_TABLE_PREF);
  },

  /**
   * Add some entries to a test tracking protection database, and resets
   * back to the default database after the test ends.
   *
   * @return {Promise}
   */
  useTestDatabase(table) {
    Services.prefs.setCharPref(table.pref, table.name);

    return new Promise((resolve, reject) => {
      let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
        Ci.nsIUrlClassifierDBService
      );
      let listener = {
        QueryInterface: iid => {
          if (
            iid.equals(Ci.nsISupports) ||
            iid.equals(Ci.nsIUrlClassifierUpdateObserver)
          ) {
            return listener;
          }

          throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
        },
        updateUrlRequested: url => {},
        streamFinished: status => {},
        updateError: errorCode => {
          reject("Got updateError when updating " + table.name);
        },
        updateSuccess: requestedTimeout => {
          resolve();
        },
      };

      try {
        dbService.beginUpdate(listener, table.name, "");
        dbService.beginStream("", "");
        dbService.updateStream(table.update);
        dbService.finishStream();
        dbService.finishUpdate();
      } catch (e) {
        reject("Failed to update with dbService: " + table.name);
      }
    });
  },

  /**
   * Handle the next "urlclassifier-before-block-channel" event.
   * @param {Object} options
   * @param {String} [options.filterOrigin] - Only handle event for channels
   * with matching origin.
   * @param {function} [options.onBeforeBlockChannel] - Optional callback for
   * the event. Called before acting on the channel.
   * @param {("allow"|"replace")} [options.action] - Whether to allow or replace
   * the channel.
   * @returns {Promise} - Resolves once event has been handled.
   */
  handleBeforeBlockChannel({
    filterOrigin = null,
    onBeforeBlockChannel,
    action,
  }) {
    if (action && action != "allow" && action != "replace") {
      throw new Error("Invalid action " + action);
    }
    let channelClassifierService = Cc[
      "@mozilla.org/url-classifier/channel-classifier-service;1"
    ].getService(Ci.nsIChannelClassifierService);

    let resolver;
    let promise = new Promise(resolve => {
      resolver = resolve;
    });

    let observer = {
      observe(subject, topic) {
        if (topic != "urlclassifier-before-block-channel") {
          return;
        }
        let channel = subject.QueryInterface(Ci.nsIUrlClassifierBlockedChannel);

        if (filterOrigin) {
          let { url } = channel;
          let { origin } = new URL(url);
          if (filterOrigin != origin) {
            return;
          }
        }

        if (onBeforeBlockChannel) {
          onBeforeBlockChannel(channel);
        }
        if (action) {
          channel[action]();
        }

        channelClassifierService.removeListener(observer);
        resolver();
      },
    };
    channelClassifierService.addListener(observer);
    return promise;
  },
};
