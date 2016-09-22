/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests common Places telemetry probes by faking the telemetry service.

Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");

var histograms = {
  PLACES_PAGES_COUNT: val => do_check_eq(val, 1),
  PLACES_BOOKMARKS_COUNT: val => do_check_eq(val, 1),
  PLACES_TAGS_COUNT: val => do_check_eq(val, 1),
  PLACES_KEYWORDS_COUNT: val => do_check_eq(val, 1),
  PLACES_SORTED_BOOKMARKS_PERC: val => do_check_eq(val, 100),
  PLACES_TAGGED_BOOKMARKS_PERC: val => do_check_eq(val, 100),
  PLACES_DATABASE_FILESIZE_MB: val => do_check_true(val > 0),
  PLACES_DATABASE_PAGESIZE_B: val => do_check_eq(val, 32768),
  PLACES_DATABASE_SIZE_PER_PAGE_B: val => do_check_true(val > 0),
  PLACES_EXPIRATION_STEPS_TO_CLEAN2: val => do_check_true(val > 1),
  //PLACES_AUTOCOMPLETE_1ST_RESULT_TIME_MS:  val => do_check_true(val > 1),
  PLACES_IDLE_FRECENCY_DECAY_TIME_MS: val => do_check_true(val > 0),
  PLACES_IDLE_MAINTENANCE_TIME_MS: val => do_check_true(val > 0),
  PLACES_ANNOS_BOOKMARKS_COUNT: val => do_check_eq(val, 1),
  PLACES_ANNOS_PAGES_COUNT: val => do_check_eq(val, 1),
  PLACES_MAINTENANCE_DAYSFROMLAST: val => do_check_true(val >= 0),
}

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

add_task(function* test_execute()
{
  // Put some trash in the database.
  let uri = NetUtil.newURI("http://moz.org/");

  let folderId = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                                    "moz test",
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX);
  let itemId = PlacesUtils.bookmarks.insertBookmark(folderId,
                                                    uri,
                                                    PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                    "moz test");
  PlacesUtils.tagging.tagURI(uri, ["tag"]);
  yield PlacesUtils.keywords.insert({ url: uri.spec, keyword: "keyword"});

  // Set a large annotation.
  let content = "";
  while (content.length < 1024) {
    content += "0";
  }
  PlacesUtils.annotations.setItemAnnotation(itemId, "test-anno", content, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);
  PlacesUtils.annotations.setPageAnnotation(uri, "test-anno", content, 0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  // Request to gather telemetry data.
  Cc["@mozilla.org/places/categoriesStarter;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "gather-telemetry", null);

  yield PlacesTestUtils.promiseAsyncUpdates();

  // Test expiration probes.
  let timeInMicroseconds = getExpirablePRTime(8);

  function newTimeInMicroseconds() {
    timeInMicroseconds = timeInMicroseconds + 1000;
    return timeInMicroseconds;
  }

  for (let i = 0; i < 3; i++) {
    yield PlacesTestUtils.addVisits({
      uri: NetUtil.newURI("http://" +  i + ".moz.org/"),
      visitDate: newTimeInMicroseconds()
    });
  }
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);
  yield promiseForceExpirationStep(2);
  yield promiseForceExpirationStep(2);

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
  PlacesDBUtils.maintenanceOnIdle();

  yield promiseTopicObserved("places-maintenance-finished");

  for (let histogramId in histograms) {
    do_print("checking histogram " + histogramId);
    let validate = histograms[histogramId];
    let snapshot = Services.telemetry.getHistogramById(histogramId).snapshot();
    validate(snapshot.sum);
    do_check_true(snapshot.counts.reduce((a, b) => a + b) > 0);
  }
});
