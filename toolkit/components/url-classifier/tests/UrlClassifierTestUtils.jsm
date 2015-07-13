"use strict";

this.EXPORTED_SYMBOLS = ["UrlClassifierTestUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.UrlClassifierTestUtils = {

  addTestTrackers() {
    const TABLE_PREF = "urlclassifier.trackingTable";
    const TABLE_NAME = "test-track-simple";

    // Add some URLs to the tracking database (to be blocked)
    let testData = "tracking.example.com/";
    let testUpdate =
          "n:1000\ni:test-track-simple\nad:1\n" +
          "a:524:32:" + testData.length + "\n" +
          testData;

    return this.useTestDatabase(TABLE_PREF, TABLE_NAME, testUpdate);
  },

  cleanupTestTrackers() {
    const TABLE_PREF = "urlclassifier.trackingTable";
    Services.prefs.clearUserPref(TABLE_PREF);
  },

  /**
   * Add some entries to a test tracking protection database, and resets
   * back to the default database after the test ends.
   *
   * @return {Promise}
   */
  useTestDatabase(tablePref, tableName, update) {
    Services.prefs.setCharPref(tablePref, tableName);

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

      dbService.beginUpdate(listener, tableName, "");
      dbService.beginStream("", "");
      dbService.updateStream(update);
      dbService.finishStream();
      dbService.finishUpdate();
    });
  },
};
