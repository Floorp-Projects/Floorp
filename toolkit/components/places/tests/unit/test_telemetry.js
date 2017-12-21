/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests common Places telemetry probes by faking the telemetry service.

Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

var histograms = {
  PLACES_PAGES_COUNT: val => Assert.equal(val, 1),
  PLACES_BOOKMARKS_COUNT: val => Assert.equal(val, 1),
  PLACES_TAGS_COUNT: val => Assert.equal(val, 1),
  PLACES_KEYWORDS_COUNT: val => Assert.equal(val, 1),
  PLACES_SORTED_BOOKMARKS_PERC: val => Assert.equal(val, 100),
  PLACES_TAGGED_BOOKMARKS_PERC: val => Assert.equal(val, 100),
  PLACES_DATABASE_FILESIZE_MB: val => Assert.ok(val > 0),
  PLACES_DATABASE_PAGESIZE_B: val => Assert.equal(val, 32768),
  PLACES_DATABASE_SIZE_PER_PAGE_B: val => Assert.ok(val > 0),
  PLACES_EXPIRATION_STEPS_TO_CLEAN2: val => Assert.ok(val > 1),
  // PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS:  val => do_check_true(val > 1),
  PLACES_IDLE_FRECENCY_DECAY_TIME_MS: val => Assert.ok(val >= 0),
  PLACES_IDLE_MAINTENANCE_TIME_MS: val => Assert.ok(val > 0),
  // One from the `setItemAnnotation` call; the other from the mobile root.
  // This can be removed along with the anno in bug 1306445.
  PLACES_ANNOS_BOOKMARKS_COUNT: val => Assert.equal(val, 2),
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
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
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

  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "moz test",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "moz test",
        url: uri,
      }]
    }],
  });

  PlacesUtils.tagging.tagURI(uri, ["tag"]);
  await PlacesUtils.keywords.insert({ url: uri.spec, keyword: "keyword"});

  // Set a large annotation.
  let content = "";
  while (content.length < 1024) {
    content += "0";
  }

  let itemId = await PlacesUtils.promiseItemId(bookmarks[1].guid);

  PlacesUtils.annotations.setItemAnnotation(itemId, "test-anno", content, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations.setPageAnnotation(uri, "test-anno", content, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  // Request to gather telemetry data.
  Cc["@mozilla.org/places/categoriesStarter;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "gather-telemetry", null);

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
      visitDate: newTimeInMicroseconds()
    });
  }
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);
  await promiseForceExpirationStep(2);
  await promiseForceExpirationStep(2);

  // Test autocomplete probes.
  /*
  // This is useful for manual testing by changing the minimum time for
  // autocomplete telemetry to 0, but there is no way to artificially delay
  // autocomplete by more than 50ms in a realiable way.
  Services.prefs.setIntPref("browser.urlbar.search.sources", 3);
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 0);
  function AutoCompleteInput(aSearches) {
    this.searches = aSearches;
  }
  AutoCompleteInput.prototype = {
    timeout: 10,
    textValue: "",
    searchParam: "",
    popupOpen: false,
    minResultsForPopup: 0,
    invalidate: function() {},
    disableAutoComplete: false,
    completeDefaultIndex: false,
    get popup() { return this; },
    onSearchBegin: function() {},
    onSearchComplete: function() {},
    setSelectedIndex: function() {},
    get searchCount() { return this.searches.length; },
    getSearchAt: function(aIndex) { return this.searches[aIndex]; },
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIAutoCompleteInput,
      Ci.nsIAutoCompletePopup,
    ])
  };
  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  controller.input = new AutoCompleteInput(["unifiedcomplete"]);
  controller.startSearch("moz");
  */

  // Test idle probes.
  PlacesUtils.history.QueryInterface(Ci.nsIObserver)
                     .observe(null, "idle-daily", null);
  await PlacesDBUtils.maintenanceOnIdle();

  for (let histogramId in histograms) {
    info("checking histogram " + histogramId);
    let validate = histograms[histogramId];
    let snapshot = Services.telemetry.getHistogramById(histogramId).snapshot();
    validate(snapshot.sum);
    Assert.ok(snapshot.counts.reduce((a, b) => a + b) > 0);
  }
});
