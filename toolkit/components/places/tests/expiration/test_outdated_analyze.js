/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that expiration executes ANALYZE when statistics are outdated.

const TEST_URL = "http://www.mozilla.org/";

XPCOMUtils.defineLazyServiceGetter(this, "gHistory",
                                   "@mozilla.org/browser/history;1",
                                   "mozIAsyncHistory");

/**
 * Object that represents a mozIVisitInfo object.
 *
 * @param [optional] aTransitionType
 *        The transition type of the visit.  Defaults to TRANSITION_LINK if not
 *        provided.
 * @param [optional] aVisitTime
 *        The time of the visit.  Defaults to now if not provided.
 */
function VisitInfo(aTransitionType, aVisitTime) {
  this.transitionType =
    aTransitionType === undefined ? TRANSITION_LINK : aTransitionType;
  this.visitDate = aVisitTime || Date.now() * 1000;
}

function run_test() {
  do_test_pending();

  // Init expiration before "importing".
  force_expiration_start();

  // Add a bunch of pages (at laast IMPORT_PAGES_THRESHOLD pages).
  let places = [];
  for (let i = 0; i < 100; i++) {
    places.push({
      uri: NetUtil.newURI(TEST_URL + i),
      title: "Title" + i,
      visits: [new VisitInfo]
    });
  }
  gHistory.updatePlaces(places);

  // Set interval to a small value to expire on it.
  setInterval(1); // 1s

  Services.obs.addObserver(function observeExpiration(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observeExpiration,
                                PlacesUtils.TOPIC_EXPIRATION_FINISHED);

    // Check that statistica are up-to-date.
    let stmt = DBConn().createAsyncStatement(
      "SELECT (SELECT COUNT(*) FROM moz_places) - "
      +        "(SELECT SUBSTR(stat,1,LENGTH(stat)-2) FROM sqlite_stat1 "
      +         "WHERE idx = 'moz_places_url_hashindex')"
    );
    stmt.executeAsync({
      handleResult: function(aResultSet) {
        let row = aResultSet.getNextRow();
        this._difference = row.getResultByIndex(0);
      },
      handleError: function(aError) {
        do_throw("Unexpected error (" + aError.result + "): " + aError.message);
      },
      handleCompletion: function(aReason) {
        do_check_true(this._difference === 0);
        do_test_finished();
      }
    });
    stmt.finalize();
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
}
