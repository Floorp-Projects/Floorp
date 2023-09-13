/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test PlacesFrecencyRecalculator scheduling.

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

async function getOriginFrecency(origin) {
  let db = await PlacesUtils.promiseDBConnection();
  return (
    await db.execute(
      `SELECT frecency
       FROM moz_origins
       WHERE host = :origin`,
      { origin }
    )
  )[0].getResultByIndex(0);
}

async function resetOriginFrecency(origin) {
  await PlacesUtils.withConnectionWrapper(
    "test_frecency_recalculator reset origin",
    async db => {
      await db.executeCached(
        `UPDATE moz_origins
         SET frecency = -1
         WHERE host = :origin`,
        { origin }
      );
    }
  );
}

async function addVisitsAndSetRecalc(urls) {
  await PlacesTestUtils.addVisits(urls);
  await PlacesUtils.withConnectionWrapper(
    "test_frecency_recalculator set recalc",
    async db => {
      await db.executeCached(
        `UPDATE moz_places
         SET frecency = -1
         WHERE url in (
           ${PlacesUtils.sqlBindPlaceholders(urls)}
         )`,
        urls
      );
      await db.executeCached(
        `UPDATE moz_places
         SET recalc_frecency = (CASE WHEN url in (
               ${PlacesUtils.sqlBindPlaceholders(urls)}
             ) THEN 1 ELSE 0 END)`,
        urls
      );
    }
  );
}

add_task(async function test() {
  info("On startup a recalculation is always pending.");
  Assert.ok(
    PlacesFrecencyRecalculator.isRecalculationPending,
    "Recalculation should be pending"
  );
  // If everything gets recalculated, then it should not be pending anymore.
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.ok(
    !PlacesFrecencyRecalculator.isRecalculationPending,
    "Recalculation should not be pending"
  );

  // If after a recalculation there's outdated entries left, a new recalculation
  // should be pending.
  info("Insert outdated frecencies");
  const url1 = new URL("https://test1.moz.org/");
  const url2 = new URL("https://test2.moz.org/");
  await addVisitsAndSetRecalc([url1.href, url2.href]);
  await resetOriginFrecency(url1.host);
  await resetOriginFrecency(url2.host);

  Assert.ok(
    PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should have set shouldStartFrecencyRecalculation"
  );
  await PlacesFrecencyRecalculator.recalculateSomeFrecencies({ chunkSize: 1 });
  Assert.ok(
    PlacesFrecencyRecalculator.isRecalculationPending,
    "Recalculation should be pending"
  );
  // Recalculating uri1 will set its origin to recalc, that means there's 2
  // origins to recalc now. Passing chunkSize: 2 here would then retrigger the
  // recalc, thinking we saturated the chunk, thus we use 3.
  await PlacesFrecencyRecalculator.recalculateSomeFrecencies({ chunkSize: 3 });
  Assert.ok(
    !PlacesFrecencyRecalculator.isRecalculationPending,
    "Recalculation should not be pending"
  );
  Assert.ok(
    !PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should have unset shouldStartFrecencyRecalculation"
  );

  Assert.greater(await getOriginFrecency(url1.host), 0);
  Assert.greater(await getOriginFrecency(url2.host), 0);

  info("Changing recalc_frecency of an entry adds a pending recalculation.");
  PlacesUtils.history.shouldStartFrecencyRecalculation = false;
  let promiseNotify = TestUtils.topicObserved("frecency-recalculation-needed");
  await PlacesUtils.withConnectionWrapper(
    "test_frecency_recalculator",
    async db => {
      await db.executeCached(
        `UPDATE moz_places SET recalc_frecency = 1 WHERE url = :url`,
        { url: url1.href }
      );
    }
  );
  Assert.ok(
    PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should have set shouldStartFrecencyRecalculation"
  );

  await promiseNotify;
  Assert.ok(
    PlacesFrecencyRecalculator.isRecalculationPending,
    "Recalculation should be pending"
  );
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.ok(
    !PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should have unset shouldStartFrecencyRecalculation"
  );
});

add_task(async function test_chunk_time_telemetry() {
  await PlacesUtils.bookmarks.insert({
    url: "https://test-bookmark.com",
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  Assert.ok(
    PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should have set shouldStartFrecencyRecalculation"
  );
  let histogram = TelemetryTestUtils.getAndClearHistogram(
    "PLACES_FRECENCY_RECALC_CHUNK_TIME_MS"
  );
  let subject = {};
  PlacesFrecencyRecalculator.observe(subject, "test-execute-taskFn", "");
  await subject.promise;
  let snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((a, b) => a + b, 0),
    1
  );
  Assert.greater(snapshot.sum, 0);
  Assert.ok(
    !PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should have unset shouldStartFrecencyRecalculation"
  );

  // It should now not report any new time, since there's nothing to recalculate.
  histogram.clear();
  PlacesFrecencyRecalculator.observe(subject, "test-execute-taskFn", "");
  await subject.promise;
  snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((a, b) => a + b, 0),
    0
  );
  Assert.ok(
    !PlacesUtils.history.shouldStartFrecencyRecalculation,
    "Should still not have set shouldStartFrecencyRecalculation"
  );
});
