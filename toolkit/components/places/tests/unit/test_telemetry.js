/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests common Places telemetry probes by faking the telemetry service.

const { PlacesDBUtils } = ChromeUtils.import(
  "resource://gre/modules/PlacesDBUtils.jsm"
);

var histograms = {
  PLACES_PAGES_COUNT: val => Assert.equal(val, 1),
  PLACES_BOOKMARKS_COUNT: val => Assert.equal(val, 1),
  PLACES_TAGS_COUNT: val => Assert.equal(val, 1),
  PLACES_KEYWORDS_COUNT: val => Assert.equal(val, 1),
  PLACES_SORTED_BOOKMARKS_PERC: val => Assert.equal(val, 100),
  PLACES_TAGGED_BOOKMARKS_PERC: val => Assert.equal(val, 100),
  PLACES_DATABASE_FILESIZE_MB: val => Assert.ok(val > 0),
  PLACES_DATABASE_FAVICONS_FILESIZE_MB: val => Assert.ok(val > 0),
  PLACES_DATABASE_PAGESIZE_B: val => Assert.equal(val, 32768),
  PLACES_DATABASE_SIZE_PER_PAGE_B: val => Assert.ok(val > 0),
  PLACES_EXPIRATION_STEPS_TO_CLEAN2: val => Assert.ok(val > 1),
  PLACES_IDLE_FRECENCY_DECAY_TIME_MS: val => Assert.ok(val >= 0),
  PLACES_IDLE_MAINTENANCE_TIME_MS: val => Assert.ok(val > 0),
  PLACES_ANNOS_BOOKMARKS_COUNT: val => Assert.equal(val, 0),
  PLACES_ANNOS_PAGES_COUNT: val => Assert.equal(val, 1),
  PLACES_MAINTENANCE_DAYSFROMLAST: val => Assert.ok(val >= 0),
};

/**
 * Forces an expiration run.
 *
 * @param [optional] aLimit
 *        Limit for the expiration.  Pass -1 for unlimited.
 *        Any other non-positive value will just expire orphans.
 *
 * @return {Promise}
 * @resolves When expiration finishes.
 * @rejects Never.
 */
function promiseForceExpirationStep(aLimit) {
  let promise = promiseTopicObserved(PlacesUtils.TOPIC_EXPIRATION_FINISHED);
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(
    Ci.nsIObserver
  );
  expire.observe(null, "places-debug-start-expiration", aLimit);
  return promise;
}

/**
 * Returns a PRTime in the past usable to add expirable visits.
 *
 * param [optional] daysAgo
 *       Expiration ignores any visit added in the last 7 days, so by default
 *       this will be set to 7.
 * @note to be safe against DST issues we go back one day more.
 */
function getExpirablePRTime(daysAgo = 7) {
  let dateObj = new Date();
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  dateObj = new Date(dateObj.getTime() - (daysAgo + 1) * 86400000);
  return dateObj.getTime() * 1000;
}

add_task(async function test_execute() {
  // Put some trash in the database.
  let uri = Services.io.newURI("http://moz.org/");

  PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "moz test",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [
          {
            title: "moz test",
            url: uri,
          },
        ],
      },
    ],
  });

  PlacesUtils.tagging.tagURI(uri, ["tag"]);
  await PlacesUtils.keywords.insert({ url: uri.spec, keyword: "keyword" });

  // Set a large annotation.
  let content = "";
  while (content.length < 1024) {
    content += "0";
  }

  await PlacesUtils.history.update({
    url: uri,
    annotations: new Map([["test-anno", content]]),
  });

  await PlacesDBUtils.telemetry();

  await PlacesTestUtils.promiseAsyncUpdates();

  // Test expiration probes.
  let timeInMicroseconds = getExpirablePRTime(8);

  function newTimeInMicroseconds() {
    timeInMicroseconds = timeInMicroseconds + 1000;
    return timeInMicroseconds;
  }

  for (let i = 0; i < 3; i++) {
    await PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://" + i + ".moz.org/"),
      visitDate: newTimeInMicroseconds(),
    });
  }
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);
  await promiseForceExpirationStep(2);
  await promiseForceExpirationStep(2);

  // Test idle probes.
  PlacesUtils.history
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "idle-daily", null);
  await PlacesDBUtils.maintenanceOnIdle();

  for (let histogramId in histograms) {
    info("checking histogram " + histogramId);
    let validate = histograms[histogramId];
    let snapshot = Services.telemetry.getHistogramById(histogramId).snapshot();
    validate(snapshot.sum);
    Assert.ok(Object.values(snapshot.values).reduce((a, b) => a + b, 0) > 0);
  }
});
