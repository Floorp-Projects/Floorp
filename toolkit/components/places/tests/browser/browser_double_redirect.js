// Test for bug 411966.
// When a page redirects multiple times, from_visit should point to the
// previous visit in the chain, not to the first visit in the chain.

add_task(function* () {
  yield PlacesTestUtils.clearHistory();

  const BASE_URL = "http://example.com/tests/toolkit/components/places/tests/browser/";
  const TEST_URI = NetUtil.newURI(BASE_URL + "begin.html");
  const FIRST_REDIRECTING_URI = NetUtil.newURI(BASE_URL + "redirect_twice.sjs");
  const FINAL_URI = NetUtil.newURI(BASE_URL + "final.html");

  let promiseVisits = new Promise(resolve => {
    PlacesUtils.history.addObserver({
      __proto__: NavHistoryObserver.prototype,
      _notified: [],
      onVisit: function (uri, id, time, sessionId, referrerId, transition) {
        info("Received onVisit: " + uri.spec);
        this._notified.push(uri);

        if (!uri.equals(FINAL_URI)) {
          return;
        }

        is(this._notified.length, 4);
        PlacesUtils.history.removeObserver(this);

        Task.spawn(function* () {
          // Get all pages visited from the original typed one
          let db = yield PlacesUtils.promiseDBConnection();
          let rows = yield db.execute(
            `SELECT url FROM moz_historyvisits
             JOIN moz_places h ON h.id = place_id
             WHERE from_visit IN
                (SELECT v.id FROM moz_historyvisits v
                 JOIN moz_places p ON p.id = v.place_id
                 WHERE p.url_hash = hash(:url) AND p.url = :url)
            `, { url: TEST_URI.spec });

          is(rows.length, 1, "Found right number of visits");
          let visitedUrl = rows[0].getResultByName("url");
          // Check that redirect from_visit is not from the original typed one
          is(visitedUrl, FIRST_REDIRECTING_URI.spec, "Check referrer for " + visitedUrl);

          resolve();
        });
      }
    }, false);
  });

  PlacesUtils.history.markPageAsTyped(TEST_URI);
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: TEST_URI.spec,
  }, function* (browser) {
    // Load begin page, click link on page to record visits.
    yield BrowserTestUtils.synthesizeMouseAtCenter("#clickme", {}, browser);

    yield promiseVisits;
  });

  yield PlacesTestUtils.clearHistory();
});
