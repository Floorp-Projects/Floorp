/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests alternative origins frecency.
// Note: the order of the tests here matters, since we are emulating subsquent
// starts of the recalculator component with different initial conditions.

let altFrecency = PlacesFrecencyRecalculator.originsAlternativeFrecencyInfo;

async function restartRecalculator() {
  let subject = {};
  PlacesFrecencyRecalculator.observe(
    subject,
    "test-alternative-frecency-init",
    ""
  );
  await subject.promise;
}

async function getAllOrigins() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`SELECT * FROM moz_origins`);
  Assert.greater(rows.length, 0);
  return rows.map(r => ({
    host: r.getResultByName("host"),
    frecency: r.getResultByName("frecency"),
    recalc_frecency: r.getResultByName("recalc_frecency"),
    alt_frecency: r.getResultByName("alt_frecency"),
    recalc_alt_frecency: r.getResultByName("recalc_alt_frecency"),
  }));
}

add_setup(async function() {
  await PlacesTestUtils.addVisits([
    "https://testdomain1.moz.org",
    "https://testdomain2.moz.org",
    "https://testdomain3.moz.org",
  ]);
  registerCleanupFunction(PlacesUtils.history.clear);
});

add_task(async function test_normal_init() {
  // Ensure moz_meta doesn't report anything.
  Assert.ok(
    !Services.prefs.getBoolPref(altFrecency.pref),
    "Check the pref is disabled by default"
  );
  Assert.equal(
    await PlacesUtils.metadata.get(altFrecency.key, 0),
    0,
    "Check there's no version stored"
  );
});

add_task(
  {
    pref_set: [[altFrecency.pref, true]],
  },
  async function test_enable_init() {
    // Set recalc_alt_frecency = 0 for the entries in moz_origins to verify
    // they are flipped back to 1.
    await PlacesUtils.withConnectionWrapper(
      "Set recalc_alt_frecency to 0",
      async db => {
        await db.execute(`UPDATE moz_origins SET recalc_alt_frecency = 0`);
      }
    );

    let promiseInitialRecalc = TestUtils.topicObserved(
      "test-origins-alternative-frecency-first-recalc"
    );
    await restartRecalculator();

    // Check alternative frecency has been marked for recalculation.
    // Note just after init we reculate a chunk, and this test code is expected
    // to run before that... though we can't be sure, so if this starts failing
    // intermittently we'll have to add more synchronization test code.
    let origins = await getAllOrigins();
    Assert.ok(
      origins.every(o => o.recalc_alt_frecency == 1),
      "All the entries have been marked for recalc"
    );

    // Ensure moz_meta doesn't report anything.
    Assert.ok(
      Services.prefs.getBoolPref(altFrecency.pref),
      "Check the pref is enabled"
    );
    Assert.equal(
      await PlacesUtils.metadata.get(altFrecency.key, 0),
      altFrecency.version,
      "Check the algorithm version has been stored"
    );

    await promiseInitialRecalc;
    // Check all alternative frecencies have been calculated, since we just have
    // a few.
    origins = await getAllOrigins();
    Assert.ok(
      origins.every(o => o.recalc_alt_frecency == 0),
      "All the entries have been recalculated"
    );
    Assert.ok(
      origins.every(o => o.alt_frecency > 0),
      "All the entries have been recalculated"
    );
    Assert.ok(
      PlacesFrecencyRecalculator.isRecalculationPending,
      "Recalculation should be pending"
    );
  }
);

add_task(
  {
    pref_set: [[altFrecency.pref, true]],
  },
  async function test_different_version() {
    let origins = await getAllOrigins();
    Assert.ok(
      origins.every(o => o.recalc_alt_frecency == 0),
      "All the entries should not need recalculation"
    );

    // It doesn't matter that the version is, it just have to be different.
    await PlacesUtils.metadata.set(altFrecency.key, 999);

    let promiseInitialRecalc = TestUtils.topicObserved(
      "test-origins-alternative-frecency-first-recalc"
    );
    await restartRecalculator();

    // Check alternative frecency has been marked for recalculation.
    // Note just after init we reculate a chunk, and this test code is expected
    // to run before that... though we can't be sure, so if this starts failing
    // intermittently we'll have to add more synchronization test code.
    origins = await getAllOrigins();
    Assert.ok(
      origins.every(o => o.recalc_alt_frecency == 1),
      "All the entries have been marked for recalc"
    );

    // Ensure moz_meta has been updated.
    Assert.ok(
      Services.prefs.getBoolPref(altFrecency.pref),
      "Check the pref is enabled"
    );
    Assert.equal(
      await PlacesUtils.metadata.get(altFrecency.key, 0),
      altFrecency.version,
      "Check the algorithm version has been stored"
    );

    await promiseInitialRecalc;
  }
);

add_task(async function test_disable() {
  let origins = await getAllOrigins();
  Assert.ok(
    origins.every(o => o.recalc_alt_frecency == 0),
    "All the entries should not need recalculation"
  );

  Assert.equal(
    await PlacesUtils.metadata.get(altFrecency.key, 0),
    altFrecency.version,
    "Check the algorithm version has been stored"
  );

  await restartRecalculator();

  // Check alternative frecency has not been marked for recalculation.
  origins = await getAllOrigins();
  Assert.ok(
    origins.every(o => o.recalc_alt_frecency == 0),
    "The entries not have been marked for recalc"
  );
  Assert.ok(
    origins.every(o => o.alt_frecency === null),
    "All the alt_frecency values should have been nullified"
  );

  // Ensure moz_meta has been updated.
  Assert.equal(
    await PlacesUtils.metadata.get(altFrecency.key, 0),
    0,
    "Check the algorithm version has been removed"
  );
});
