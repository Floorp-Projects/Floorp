/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests alternative origins frecency.
// Note: the order of the tests here matters, since we are emulating subsquent
// starts of the recalculator component with different initial conditions.

const FEATURE_PREF = "places.frecency.origins.alternative.featureGate";

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

add_setup(async function () {
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
    !PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.enabled,
    "Check the pref is disabled by default"
  );
  // Avoid hitting the cache, we want to check the actual database value.
  PlacesUtils.metadata.cache.clear();
  Assert.ok(
    ObjectUtils.isEmpty(
      await PlacesUtils.metadata.get(
        PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
        Object.create(null)
      )
    ),
    "Check there's no variables stored"
  );
});

add_task(
  {
    pref_set: [[FEATURE_PREF, true]],
  },
  async function test_enable_init() {
    // Set alt_frecency to NULL and recalc_alt_frecency = 0 for the entries in
    // moz_origins to verify they are recalculated.
    await PlacesTestUtils.updateDatabaseValues("moz_origins", {
      alt_frecency: null,
      recalc_alt_frecency: 0,
    });

    await restartRecalculator();

    // Ensure moz_meta doesn't report anything.
    Assert.ok(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.enabled,
      "Check the pref is enabled"
    );
    // Avoid hitting the cache, we want to check the actual database value.
    PlacesUtils.metadata.cache.clear();
    Assert.equal(
      (
        await PlacesUtils.metadata.get(
          PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins
            .metadataKey,
          Object.create(null)
        )
      ).version,
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.variables
        .version,
      "Check the algorithm version has been stored"
    );

    // Check all alternative frecencies have been calculated, since we just have
    // a few.
    let origins = await getAllOrigins();
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
    pref_set: [[FEATURE_PREF, true]],
  },
  async function test_different_version() {
    let origins = await getAllOrigins();
    Assert.ok(
      origins.every(o => o.recalc_alt_frecency == 0),
      "All the entries should not need recalculation"
    );

    // Set alt_frecency to NULL for the entries in moz_origins to verify they
    // are recalculated.
    await PlacesTestUtils.updateDatabaseValues("moz_origins", {
      alt_frecency: null,
    });

    // It doesn't matter that the version is, it just have to be different.
    let variables = await PlacesUtils.metadata.get(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
      Object.create(null)
    );
    variables.version = 999;
    await PlacesUtils.metadata.set(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
      variables
    );

    await restartRecalculator();

    // Check alternative frecency has been marked for recalculation.
    // Note just after init we reculate a chunk, and this test code is expected
    // to run before that... though we can't be sure, so if this starts failing
    // intermittently we'll have to add more synchronization test code.
    origins = await getAllOrigins();
    // Ensure moz_meta has been updated.
    Assert.ok(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.enabled,
      "Check the pref is enabled"
    );
    // Avoid hitting the cache, we want to check the actual database value.
    PlacesUtils.metadata.cache.clear();
    Assert.deepEqual(
      (
        await PlacesUtils.metadata.get(
          PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins
            .metadataKey,
          Object.create(null)
        )
      ).version,
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.variables
        .version,
      "Check the algorithm version has been stored"
    );

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
    pref_set: [[FEATURE_PREF, true]],
  },
  async function test_different_variables() {
    let origins = await getAllOrigins();
    Assert.ok(
      origins.every(o => o.recalc_alt_frecency == 0),
      "All the entries should not need recalculation"
    );

    // Set alt_frecency to NULL for the entries in moz_origins to verify they
    // are recalculated.
    await PlacesTestUtils.updateDatabaseValues("moz_origins", {
      alt_frecency: null,
    });

    // Change variables.
    let variables = await PlacesUtils.metadata.get(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
      Object.create(null)
    );
    Assert.greater(Object.keys(variables).length, 1);
    Assert.ok("version" in variables, "At least the version is always present");
    await PlacesUtils.metadata.set(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
      {
        version:
          PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.variables
            .version,
        someVar: 1,
      }
    );

    await restartRecalculator();

    // Ensure moz_meta has been updated.
    Assert.ok(
      PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.enabled,
      "Check the pref is enabled"
    );
    // Avoid hitting the cache, we want to check the actual database value.
    PlacesUtils.metadata.cache.clear();
    Assert.deepEqual(
      await PlacesUtils.metadata.get(
        PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
        Object.create(null)
      ),
      variables,
      "Check the algorithm variables have been stored"
    );

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

add_task(async function test_disable() {
  let origins = await getAllOrigins();
  Assert.ok(
    origins.every(o => o.recalc_alt_frecency == 0),
    "All the entries should not need recalculation"
  );
  // Avoid hitting the cache, we want to check the actual database value.
  PlacesUtils.metadata.cache.clear();
  Assert.equal(
    (
      await PlacesUtils.metadata.get(
        PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
        Object.create(null)
      )
    ).version,
    PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.variables
      .version,
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
  // Avoid hitting the cache, we want to check the actual database value.
  PlacesUtils.metadata.cache.clear();
  Assert.ok(
    ObjectUtils.isEmpty(
      await PlacesUtils.metadata.get(
        PlacesFrecencyRecalculator.alternativeFrecencyInfo.origins.metadataKey,
        Object.create(null)
      )
    ),
    "Check the algorithm variables has been removed"
  );
});
