"use strict";

this.EXPORTED_SYMBOLS = ["UrlClassifierTestUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const TRACKING_TABLE_NAME = "mochitest-track-simple";
const TRACKING_TABLE_PREF = "urlclassifier.trackingTable";
const WHITELIST_TABLE_NAME = "mochitest-trackwhite-simple";
const WHITELIST_TABLE_PREF = "urlclassifier.trackingWhitelistTable";

Cu.import("resource://gre/modules/Services.jsm");

this.UrlClassifierTestUtils = {

  addTestTrackers() {
    // Add some URLs to the tracking databases
    let trackingURL1 = "tracking.example.com/";
    let trackingURL2 = "itisatracker.org/";
    let trackingURL3 = "trackertest.org/";
    let whitelistedURL = "itisatrap.org/?resource=itisatracker.org";

    let trackingUpdate =
          "n:1000\ni:" + TRACKING_TABLE_NAME + "\nad:3\n" +
          "a:1:32:" + trackingURL1.length + "\n" +
          trackingURL1 + "\n" +
          "a:2:32:" + trackingURL2.length + "\n" +
          trackingURL2 + "\n" +
          "a:3:32:" + trackingURL3.length + "\n" +
          trackingURL3 + "\n";
    let whitelistUpdate =
          "n:1000\ni:" + WHITELIST_TABLE_NAME + "\nad:1\n" +
          "a:1:32:" + whitelistedURL.length + "\n" +
          whitelistedURL + "\n";

    var tables = [
      {
        pref: TRACKING_TABLE_PREF,
        name: TRACKING_TABLE_NAME,
        update: trackingUpdate
      },
      {
        pref: WHITELIST_TABLE_PREF,
        name: WHITELIST_TABLE_NAME,
        update: whitelistUpdate
      }
    ];

    return this.useTestDatabase(tables);
  },

  cleanupTestTrackers() {
    Services.prefs.clearUserPref(TRACKING_TABLE_PREF);
    Services.prefs.clearUserPref(WHITELIST_TABLE_PREF);
  },

  /**
   * Add some entries to a test tracking protection database, and resets
   * back to the default database after the test ends.
   *
   * @return {Promise}
   */
  useTestDatabase(tables) {
    for (var table of tables) {
      Services.prefs.setCharPref(table.pref, table.name);
    }

    return new Promise((resolve, reject) => {
      let dbService = Cc["@mozilla.org/url-classifier/dbservice;1"].
                      getService(Ci.nsIUrlClassifierDBService);
      let listener = {
        QueryInterface: iid => {
          if (iid.equals(Ci.nsISupports) ||
              iid.equals(Ci.nsIUrlClassifierUpdateObserver))
            return listener;

          throw Cr.NS_ERROR_NO_INTERFACE;
        },
        updateUrlRequested: url => { },
        streamFinished: status => { },
        updateError: errorCode => {
          reject("Couldn't update classifier.");
        },
        updateSuccess: requestedTimeout => {
          resolve();
        }
      };

      for (var table of tables) {
        dbService.beginUpdate(listener, table.name, "");
        dbService.beginStream("", "");
        dbService.updateStream(table.update);
        dbService.finishStream();
        dbService.finishUpdate();
      }
    });
  },
};
