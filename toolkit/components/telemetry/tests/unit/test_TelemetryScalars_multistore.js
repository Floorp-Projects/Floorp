/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const CATEGORY = "telemetry.test";
const MAIN_ONLY = `${CATEGORY}.main_only`;
const SYNC_ONLY = `${CATEGORY}.sync_only`;
const MULTIPLE_STORES = `${CATEGORY}.multiple_stores`;
const MULTIPLE_STORES_STRING = `${CATEGORY}.multiple_stores_string`;
const MULTIPLE_STORES_BOOL = `${CATEGORY}.multiple_stores_bool`;
const MULTIPLE_STORES_KEYED = `${CATEGORY}.multiple_stores_keyed`;

function getParentSnapshot(store, keyed = false, clear = false) {
  return keyed
    ? Telemetry.getSnapshotForKeyedScalars(store, clear).parent
    : Telemetry.getSnapshotForScalars(store, clear).parent;
}

add_task(async function test_multistore_basics() {
  Telemetry.clearScalars();

  const expectedUint = 3785;
  const expectedBool = true;
  const expectedString = "some value";
  const expectedKey = "some key";
  Telemetry.scalarSet(MAIN_ONLY, expectedUint);
  Telemetry.scalarSet(SYNC_ONLY, expectedUint);
  Telemetry.scalarSet(MULTIPLE_STORES, expectedUint);
  Telemetry.scalarSet(MULTIPLE_STORES_STRING, expectedString);
  Telemetry.scalarSet(MULTIPLE_STORES_BOOL, expectedBool);
  Telemetry.keyedScalarSet(MULTIPLE_STORES_KEYED, expectedKey, expectedUint);

  const mainScalars = getParentSnapshot("main");
  const syncScalars = getParentSnapshot("sync");
  const mainKeyedScalars = getParentSnapshot("main", true /* keyed */);
  const syncKeyedScalars = getParentSnapshot("sync", true /* keyed */);

  Assert.ok(
    MAIN_ONLY in mainScalars,
    `Main-store scalar ${MAIN_ONLY} must be in main snapshot.`
  );
  Assert.ok(
    !(MAIN_ONLY in syncScalars),
    `Main-store scalar ${MAIN_ONLY} must not be in sync snapshot.`
  );
  Assert.equal(
    mainScalars[MAIN_ONLY],
    expectedUint,
    `Main-store scalar ${MAIN_ONLY} must have correct value.`
  );

  Assert.ok(
    SYNC_ONLY in syncScalars,
    `Sync-store scalar ${SYNC_ONLY} must be in sync snapshot.`
  );
  Assert.ok(
    !(SYNC_ONLY in mainScalars),
    `Sync-store scalar ${SYNC_ONLY} must not be in main snapshot.`
  );
  Assert.equal(
    syncScalars[SYNC_ONLY],
    expectedUint,
    `Sync-store scalar ${SYNC_ONLY} must have correct value.`
  );

  Assert.ok(
    MULTIPLE_STORES in mainScalars && MULTIPLE_STORES in syncScalars,
    `Multi-store scalar ${MULTIPLE_STORES} must be in both main and sync snapshots.`
  );
  Assert.equal(
    mainScalars[MULTIPLE_STORES],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES} must have correct value in main store.`
  );
  Assert.equal(
    syncScalars[MULTIPLE_STORES],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES} must have correct value in sync store.`
  );

  Assert.ok(
    MULTIPLE_STORES_STRING in mainScalars &&
      MULTIPLE_STORES_STRING in syncScalars,
    `Multi-store scalar ${MULTIPLE_STORES_STRING} must be in both main and sync snapshots.`
  );
  Assert.equal(
    mainScalars[MULTIPLE_STORES_STRING],
    expectedString,
    `Multi-store scalar ${MULTIPLE_STORES_STRING} must have correct value in main store.`
  );
  Assert.equal(
    syncScalars[MULTIPLE_STORES_STRING],
    expectedString,
    `Multi-store scalar ${MULTIPLE_STORES_STRING} must have correct value in sync store.`
  );

  Assert.ok(
    MULTIPLE_STORES_BOOL in mainScalars && MULTIPLE_STORES_BOOL in syncScalars,
    `Multi-store scalar ${MULTIPLE_STORES_BOOL} must be in both main and sync snapshots.`
  );
  Assert.equal(
    mainScalars[MULTIPLE_STORES_BOOL],
    expectedBool,
    `Multi-store scalar ${MULTIPLE_STORES_BOOL} must have correct value in main store.`
  );
  Assert.equal(
    syncScalars[MULTIPLE_STORES_BOOL],
    expectedBool,
    `Multi-store scalar ${MULTIPLE_STORES_BOOL} must have correct value in sync store.`
  );

  Assert.ok(
    MULTIPLE_STORES_KEYED in mainKeyedScalars &&
      MULTIPLE_STORES_KEYED in syncKeyedScalars,
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must be in both main and sync snapshots.`
  );
  Assert.ok(
    expectedKey in mainKeyedScalars[MULTIPLE_STORES_KEYED] &&
      expectedKey in syncKeyedScalars[MULTIPLE_STORES_KEYED],
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must have key ${expectedKey} in both snapshots.`
  );
  Assert.equal(
    mainKeyedScalars[MULTIPLE_STORES_KEYED][expectedKey],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must have correct value in main store.`
  );
  Assert.equal(
    syncKeyedScalars[MULTIPLE_STORES_KEYED][expectedKey],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must have correct value in sync store.`
  );
});

add_task(async function test_multistore_uint() {
  Telemetry.clearScalars();

  // Uint scalars are the only kind with an implicit default value of 0.
  // They shouldn't report any value until set, but if you Add or SetMaximum
  // they pretend that they have been set to 0 for the purposes of that operation.

  function assertNotIn() {
    let mainScalars = getParentSnapshot("main");
    let syncScalars = getParentSnapshot("sync");

    if (!mainScalars && !syncScalars) {
      Assert.ok(true, "No scalars at all");
    } else {
      Assert.ok(
        !(MULTIPLE_STORES in mainScalars) && !(MULTIPLE_STORES in syncScalars),
        `Multi-store scalar ${MULTIPLE_STORES} must not have an initial value in either store.`
      );
    }
  }
  assertNotIn();

  // Test that Add operates on implicit 0.
  Telemetry.scalarAdd(MULTIPLE_STORES, 1);

  function assertBothEqual(val, clear = false) {
    let mainScalars = getParentSnapshot("main", false, clear);
    let syncScalars = getParentSnapshot("sync", false, clear);

    Assert.ok(
      MULTIPLE_STORES in mainScalars && MULTIPLE_STORES in syncScalars,
      `Multi-store scalar ${MULTIPLE_STORES} must be in both main and sync snapshots.`
    );
    Assert.equal(
      mainScalars[MULTIPLE_STORES],
      val,
      `Multi-store scalar ${MULTIPLE_STORES} must have the correct value in main store.`
    );
    Assert.equal(
      syncScalars[MULTIPLE_STORES],
      val,
      `Multi-store scalar ${MULTIPLE_STORES} must have the correct value in sync store.`
    );
  }

  assertBothEqual(1, true /* clear */);

  assertNotIn();

  // Test that SetMaximum operates on implicit 0.
  Telemetry.scalarSetMaximum(MULTIPLE_STORES, 1337);
  assertBothEqual(1337);

  // Test that Add works, since we're in the neighbourhood.
  Telemetry.scalarAdd(MULTIPLE_STORES, 1);
  assertBothEqual(1338, true /* clear */);

  assertNotIn();

  // Test that clearing individual stores works
  // and that afterwards the values are managed independently.
  Telemetry.scalarAdd(MULTIPLE_STORES, 1234);
  assertBothEqual(1234);
  let syncScalars = getParentSnapshot(
    "sync",
    false /* keyed */,
    true /* clear */
  );
  Assert.equal(
    syncScalars[MULTIPLE_STORES],
    1234,
    `Multi-store scalar ${MULTIPLE_STORES} must be present in a second snapshot.`
  );
  syncScalars = getParentSnapshot("sync");
  Assert.equal(
    syncScalars,
    undefined,
    `Multi-store scalar ${MULTIPLE_STORES} must not be present after clearing.`
  );
  let mainScalars = getParentSnapshot("main");
  Assert.equal(
    mainScalars[MULTIPLE_STORES],
    1234,
    `Multi-store scalar ${MULTIPLE_STORES} must maintain value in main store after sync store is cleared.`
  );

  Telemetry.scalarSetMaximum(MULTIPLE_STORES, 1);
  syncScalars = getParentSnapshot("sync");
  Assert.equal(
    syncScalars[MULTIPLE_STORES],
    1,
    `Multi-store scalar ${MULTIPLE_STORES} must return to using implicit 0 for setMax operation.`
  );
  mainScalars = getParentSnapshot("main");
  Assert.equal(
    mainScalars[MULTIPLE_STORES],
    1234,
    `Multi-store scalar ${MULTIPLE_STORES} must retain old value.`
  );

  Telemetry.scalarAdd(MULTIPLE_STORES, 1);
  syncScalars = getParentSnapshot("sync");
  Assert.equal(
    syncScalars[MULTIPLE_STORES],
    2,
    `Multi-store scalar ${MULTIPLE_STORES} must manage independently for add operations.`
  );
  mainScalars = getParentSnapshot("main");
  Assert.equal(
    mainScalars[MULTIPLE_STORES],
    1235,
    `Multi-store scalar ${MULTIPLE_STORES} must add properly.`
  );

  Telemetry.scalarSet(MULTIPLE_STORES, 9876);
  assertBothEqual(9876);
});

add_task(async function test_empty_absence() {
  // Current semantics are we don't snapshot empty things.
  // So no {parent: {}, ...}. Instead {...}.

  Telemetry.clearScalars();

  Telemetry.scalarSet(MULTIPLE_STORES, 1);
  let snapshot = getParentSnapshot("main", false /* keyed */, true /* clear */);

  Assert.ok(
    MULTIPLE_STORES in snapshot,
    `${MULTIPLE_STORES} must be in the snapshot.`
  );
  Assert.equal(
    snapshot[MULTIPLE_STORES],
    1,
    `${MULTIPLE_STORES} must have the correct value.`
  );

  snapshot = getParentSnapshot("main", false /* keyed */, true /* clear */);
  Assert.equal(
    snapshot,
    undefined,
    `Parent snapshot must be empty if no data.`
  );

  snapshot = getParentSnapshot("sync", false /* keyed */, true /* clear */);
  Assert.ok(
    MULTIPLE_STORES in snapshot,
    `${MULTIPLE_STORES} must be in the sync snapshot.`
  );
  Assert.equal(
    snapshot[MULTIPLE_STORES],
    1,
    `${MULTIPLE_STORES} must have the correct value in the sync snapshot.`
  );
});

add_task(async function test_empty_absence_keyed() {
  // Current semantics are we don't snapshot empty things.
  // So no {parent: {}, ...}. Instead {...}.
  // And for Keyed Scalars, no {parent: { keyed_scalar: {} }, ...}. Just {...}.

  Telemetry.clearScalars();

  const key = "just a key, y'know";
  Telemetry.keyedScalarSet(MULTIPLE_STORES_KEYED, key, 1);
  let snapshot = getParentSnapshot("main", true /* keyed */, true /* clear */);

  Assert.ok(
    MULTIPLE_STORES_KEYED in snapshot,
    `${MULTIPLE_STORES_KEYED} must be in the snapshot.`
  );
  Assert.ok(
    key in snapshot[MULTIPLE_STORES_KEYED],
    `${MULTIPLE_STORES_KEYED} must have the stored key.`
  );
  Assert.equal(
    snapshot[MULTIPLE_STORES_KEYED][key],
    1,
    `${MULTIPLE_STORES_KEYED}[${key}] should have the correct value.`
  );

  snapshot = getParentSnapshot("main", true /* keyed */);
  Assert.equal(
    snapshot,
    undefined,
    `Parent snapshot should be empty if no data.`
  );
  snapshot = getParentSnapshot("sync", true /* keyed */);

  Assert.ok(
    MULTIPLE_STORES_KEYED in snapshot,
    `${MULTIPLE_STORES_KEYED} must be in the sync snapshot.`
  );
  Assert.ok(
    key in snapshot[MULTIPLE_STORES_KEYED],
    `${MULTIPLE_STORES_KEYED} must have the stored key.`
  );
  Assert.equal(
    snapshot[MULTIPLE_STORES_KEYED][key],
    1,
    `${MULTIPLE_STORES_KEYED}[${key}] should have the correct value.`
  );
});

add_task(async function test_multistore_default_values() {
  Telemetry.clearScalars();

  const expectedUint = 3785;
  const expectedKey = "some key";
  Telemetry.scalarSet(MAIN_ONLY, expectedUint);
  Telemetry.scalarSet(SYNC_ONLY, expectedUint);
  Telemetry.scalarSet(MULTIPLE_STORES, expectedUint);
  Telemetry.keyedScalarSet(MULTIPLE_STORES_KEYED, expectedKey, expectedUint);

  let mainScalars;
  let mainKeyedScalars;

  // Getting snapshot and NOT clearing (using default values for optional parameters)
  mainScalars = Telemetry.getSnapshotForScalars().parent;
  mainKeyedScalars = Telemetry.getSnapshotForKeyedScalars().parent;

  Assert.equal(
    mainScalars[MAIN_ONLY],
    expectedUint,
    `Main-store scalar ${MAIN_ONLY} must have correct value.`
  );
  Assert.ok(
    !(SYNC_ONLY in mainScalars),
    `Sync-store scalar ${SYNC_ONLY} must not be in main snapshot.`
  );
  Assert.equal(
    mainScalars[MULTIPLE_STORES],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES} must have correct value in main store.`
  );
  Assert.equal(
    mainKeyedScalars[MULTIPLE_STORES_KEYED][expectedKey],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must have correct value in main store.`
  );

  // Getting snapshot and clearing
  mainScalars = Telemetry.getSnapshotForScalars("main", true).parent;
  mainKeyedScalars = Telemetry.getSnapshotForKeyedScalars("main", true).parent;

  Assert.equal(
    mainScalars[MAIN_ONLY],
    expectedUint,
    `Main-store scalar ${MAIN_ONLY} must have correct value.`
  );
  Assert.ok(
    !(SYNC_ONLY in mainScalars),
    `Sync-store scalar ${SYNC_ONLY} must not be in main snapshot.`
  );
  Assert.equal(
    mainScalars[MULTIPLE_STORES],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES} must have correct value in main store.`
  );
  Assert.equal(
    mainKeyedScalars[MULTIPLE_STORES_KEYED][expectedKey],
    expectedUint,
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must have correct value in main store.`
  );

  // Getting snapshot (with default values), should be empty now
  mainScalars = Telemetry.getSnapshotForScalars().parent || {};
  mainKeyedScalars = Telemetry.getSnapshotForKeyedScalars().parent || {};

  Assert.ok(
    !(MAIN_ONLY in mainScalars),
    `Main-store scalar ${MAIN_ONLY} must not be in main snapshot.`
  );
  Assert.ok(
    !(MULTIPLE_STORES in mainScalars),
    `Multi-store scalar ${MULTIPLE_STORES} must not be in main snapshot.`
  );
  Assert.ok(
    !(MULTIPLE_STORES_KEYED in mainKeyedScalars),
    `Multi-store scalar ${MULTIPLE_STORES_KEYED} must not be in main snapshot.`
  );
});
