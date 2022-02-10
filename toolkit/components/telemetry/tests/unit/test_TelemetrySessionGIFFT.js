/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

const { TelemetrySession } = ChromeUtils.import(
  "resource://gre/modules/TelemetrySession.jsm"
);

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

add_task(function test_setup() {
  // FOG needs a profile dir and to be init'd in order to be tested.
  do_get_profile();
  Services.fog.initializeFOG();

  // Otherwise payload assembly will fail due to missing values.
  TelemetrySession.earlyInit(true);
});

add_task(function test_assemblingInstrumentation() {
  Telemetry.clearScalars();

  Assert.equal(
    undefined,
    Glean.gifftValidation.mainPingAssembling.testGetValue()
  );
  let snapshot = Telemetry.getSnapshotForScalars().parent;
  Assert.ok(
    !snapshot || !("gifft.validation.main_ping_assembling" in snapshot)
  );
  Assert.ok(
    !snapshot ||
      !("gifft.validation.mirror_for_main_ping_assembling" in snapshot)
  );

  let payload = TelemetrySession.getPayload("reason", true);

  Assert.equal(true, Glean.gifftValidation.mainPingAssembling.testGetValue());
  // Assembling the payload clears the subsession, so these should've been set
  // and then cleared.
  snapshot = Telemetry.getSnapshotForScalars().parent;
  Assert.ok(
    !snapshot || !("gifft.validation.main_ping_assembling" in snapshot)
  );
  Assert.ok(
    !snapshot ||
      !("gifft.validation.mirror_for_main_ping_assembling" in snapshot)
  );

  // We can verify that any assembled payload has both values.
  Assert.ok(
    payload.processes.parent.scalars["gifft.validation.main_ping_assembling"]
  );
  Assert.ok(
    payload.processes.parent.scalars[
      "gifft.validation.mirror_for_main_ping_assembling"
    ]
  );
});
