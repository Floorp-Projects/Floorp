// Test for bug 411966.
// When a page redirects multiple times, from_visit should point to the
// previous visit in the chain, not to the first visit in the chain.

add_task(async function() {
  await PlacesUtils.history.clear();

  const BASE_URL =
    "http://example.com/tests/toolkit/components/places/tests/browser/";
  const TEST_URI = NetUtil.newURI(BASE_URL + "begin.html");
  const FIRST_REDIRECTING_URI = NetUtil.newURI(BASE_URL + "redirect_twice.sjs");
  const FINAL_URI = NetUtil.newURI(
    "http://test1.example.com/tests/toolkit/components/places/tests/browser/final.html"
  );

  let promiseVisits = new Promise(resolve => {
    let observer = {
      _notified: [],
      onVisit(uri, id, time, referrerId, transition) {
        info("Received onVisit: " + uri);
        this._notified.push(uri);

        if (uri != FINAL_URI.spec) {
          return;
        }

        is(this._notified.length, 4);
        PlacesObservers.removeListener(["page-visited"], this.handleEvents);

        (async function() {
          // Get all pages visited from the original typed one
          let db = await PlacesUtils.promiseDBConnection();
          let rows = await db.execute(
            `SELECT url FROM moz_historyvisits
             JOIN moz_places h ON h.id = place_id
             WHERE from_visit IN
                (SELECT v.id FROM moz_historyvisits v
                 JOIN moz_places p ON p.id = v.place_id
                 WHERE p.url_hash = hash(:url) AND p.url = :url)
            `,
            { url: TEST_URI.spec }
          );

          is(rows.length, 1, "Found right number of visits");
          let visitedUrl = rows[0].getResultByName("url");
          // Check that redirect from_visit is not from the original typed one
          is(
            visitedUrl,
            FIRST_REDIRECTING_URI.spec,
            "Check referrer for " + visitedUrl
          );

          resolve();
        })();
      },
      handleEvents(events) {
        is(events.length, 1, "Right number of visits notified");
        is(events[0].type, "page-visited");
        let {
          url,
          visitId,
          visitTime,
          referringVisitId,
          transitionType,
        } = events[0];
        this.onVisit(url, visitId, visitTime, referringVisitId, transitionType);
      },
    };
    observer.handleEvents = observer.handleEvents.bind(observer);
    PlacesObservers.addListener(["page-visited"], observer.handleEvents);
  });

  PlacesUtils.history.markPageAsTyped(TEST_URI);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_URI.spec,
    },
    async function(browser) {
      // Load begin page, click link on page to record visits.
      await BrowserTestUtils.synthesizeMouseAtCenter("#clickme", {}, browser);

      await promiseVisits;
    }
  );

  await PlacesUtils.history.clear();
});
