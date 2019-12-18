/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const CATEGORY = "telemetry.test";
const MAIN_ONLY = `${CATEGORY}.main_only`;
const IMPRESSION_ID_ONLY = `${CATEGORY}.impression_id_only`;

add_task(async function test_multistore_basics() {
  Telemetry.clearScalars();

  const expectedUint = 3785;
  const expectedString = "{some_impression_id}";
  Telemetry.scalarSet(MAIN_ONLY, expectedUint);
  Telemetry.scalarSet(IMPRESSION_ID_ONLY, expectedString);

  const mainScalars = Telemetry.getSnapshotForScalars("main").parent;
  const impressionIdScalars = Telemetry.getSnapshotForScalars(
    "deletion-request"
  ).parent;

  Assert.ok(
    MAIN_ONLY in mainScalars,
    `Main-store scalar ${MAIN_ONLY} must be in main snapshot.`
  );
  Assert.ok(
    !(MAIN_ONLY in impressionIdScalars),
    `Main-store scalar ${MAIN_ONLY} must not be in deletion-request snapshot.`
  );
  Assert.equal(
    mainScalars[MAIN_ONLY],
    expectedUint,
    `Main-store scalar ${MAIN_ONLY} must have correct value.`
  );

  Assert.ok(
    IMPRESSION_ID_ONLY in impressionIdScalars,
    `Deletion-request store scalar ${IMPRESSION_ID_ONLY} must be in deletion-request snapshot.`
  );
  Assert.ok(
    !(IMPRESSION_ID_ONLY in mainScalars),
    `Deletion-request scalar ${IMPRESSION_ID_ONLY} must not be in main snapshot.`
  );
  Assert.equal(
    impressionIdScalars[IMPRESSION_ID_ONLY],
    expectedString,
    `Deletion-request store scalar ${IMPRESSION_ID_ONLY} must have correct value.`
  );
});
