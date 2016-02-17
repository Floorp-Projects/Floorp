"use strict";

this.EXPORTED_SYMBOLS = [
  "PlacesTestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["URL"]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

this.PlacesTestUtils = Object.freeze({
  /**
   * Asynchronously adds visits to a page.
   *
   * @param aPlaceInfo
   *        Can be an nsIURI, in such a case a single LINK visit will be added.
   *        Otherwise can be an object describing the visit to add, or an array
   *        of these objects:
   *          { uri: nsIURI of the page,
   *            [optional] transition: one of the TRANSITION_* from nsINavHistoryService,
   *            [optional] title: title of the page,
   *            [optional] visitDate: visit date in microseconds from the epoch
   *            [optional] referrer: nsIURI of the referrer for this visit
   *          }
   *
   * @return {Promise}
   * @resolves When all visits have been added successfully.
   * @rejects JavaScript exception.
   */
  addVisits: Task.async(function* (placeInfo) {
    let places = [];
    if (placeInfo instanceof Ci.nsIURI ||
        placeInfo instanceof URL ||
        typeof placeInfo == "string") {
      places.push({ uri: placeInfo });
    }
    else if (Array.isArray(placeInfo)) {
      places = places.concat(placeInfo);
    } else if (typeof placeInfo == "object" && placeInfo.uri) {
      places.push(placeInfo)
    } else {
      throw new Error("Unsupported type passed to addVisits");
    }

    // Create mozIVisitInfo for each entry.
    let now = Date.now();
    for (let place of places) {
      if (typeof place.title != "string") {
        place.title = "test visit for " + place.uri.spec;
      }
      if (typeof place.uri == "string") {
        place.uri = NetUtil.newURI(place.uri);
      } else if (place.uri instanceof URL) {
        place.uri = NetUtil.newURI(place.href);
      }
      place.visits = [{
        transitionType: place.transition === undefined ? Ci.nsINavHistoryService.TRANSITION_LINK
                                                       : place.transition,
        visitDate: place.visitDate || (now++) * 1000,
        referrerURI: place.referrer
      }];
    }

    yield new Promise((resolve, reject) => {
      PlacesUtils.asyncHistory.updatePlaces(
        places,
        {
          handleError(resultCode, placeInfo) {
            let ex = new Components.Exception("Unexpected error in adding visits.",
                                              resultCode);
            reject(ex);
          },
          handleResult: function () {},
          handleCompletion() {
            resolve();
          }
        }
      );
    });
  }),

  /**
   * Clear all history.
   *
   * @return {Promise}
   * @resolves When history was cleared successfully.
   * @rejects JavaScript exception.
   */
  clearHistory() {
    let expirationFinished = new Promise(resolve => {
      Services.obs.addObserver(function observe(subj, topic, data) {
        Services.obs.removeObserver(observe, topic);
        resolve();
      }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
    });

    return Promise.all([expirationFinished, PlacesUtils.history.clear()]);
  },

  /**
   * Waits for all pending async statements on the default connection.
   *
   * @return {Promise}
   * @resolves When all pending async statements finished.
   * @rejects Never.
   *
   * @note The result is achieved by asynchronously executing a query requiring
   *       a write lock.  Since all statements on the same connection are
   *       serialized, the end of this write operation means that all writes are
   *       complete.  Note that WAL makes so that writers don't block readers, but
   *       this is a problem only across different connections.
   */
  promiseAsyncUpdates() {
    return new Promise(resolve => {
      let db = PlacesUtils.history.DBConnection;
      let begin = db.createAsyncStatement("BEGIN EXCLUSIVE");
      begin.executeAsync();
      begin.finalize();

      let commit = db.createAsyncStatement("COMMIT");
      commit.executeAsync({
        handleResult: function () {},
        handleError: function () {},
        handleCompletion: function(aReason)
        {
          resolve();
        }
      });
      commit.finalize();
    });
  }

});
