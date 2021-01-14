const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

const PREF_FREC_DECAY_RATE_DEF = 0.975;

/**
 * Promises that the many frecencies changed notification has been seen.
 *
 * @returns {Promise} A promise which is resolved when the notification is seen.
 */
function promiseManyFrecenciesChanged() {
  let deferred = PromiseUtils.defer();
  let obs = new NavHistoryObserver();
  obs.onManyFrecenciesChanged = () => {
    PlacesUtils.history.removeObserver(obs);
    Assert.ok(true);
    deferred.resolve();
  };
  PlacesUtils.history.addObserver(obs);
  return deferred.promise;
}

add_task(async function setup() {
  Services.prefs.setCharPref(
    "places.frecency.decayRate",
    PREF_FREC_DECAY_RATE_DEF
  );
});

add_task(async function test_frecency_decay() {
  let unvisitedBookmarkFrecency = Services.prefs.getIntPref(
    "places.frecency.unvisitedBookmarkBonus"
  );

  // Add a bookmark and check its frecency.
  let url = "http://example.com/b";
  let promiseOne = promiseManyFrecenciesChanged();
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  await promiseOne;

  // Trigger DecayFrecency.
  let promiseMany = promiseManyFrecenciesChanged();
  PlacesUtils.history
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "idle-daily", "");
  await promiseMany;

  // Now check the new frecency is correct.
  let newFrecency = await PlacesTestUtils.fieldInDB(url, "frecency");

  Assert.equal(
    newFrecency,
    Math.round(unvisitedBookmarkFrecency * PREF_FREC_DECAY_RATE_DEF),
    "Frecencies should match"
  );
});
