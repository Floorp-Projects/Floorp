/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const UINT_SCALAR = "telemetry.test.unsigned_int_kind";
const STRING_SCALAR = "telemetry.test.string_kind";
const BOOLEAN_SCALAR = "telemetry.test.boolean_kind";

add_task(function* test_serializationFormat() {
  Telemetry.clearScalars();

  // Set the scalars to a known value.
  const expectedUint = 3785;
  const expectedString = "some value";
  Telemetry.scalarSet(UINT_SCALAR, expectedUint);
  Telemetry.scalarSet(STRING_SCALAR, expectedString);
  Telemetry.scalarSet(BOOLEAN_SCALAR, true);

  // Get a snapshot of the scalars.
  const scalars =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  // Check that they are serialized to the correct format.
  Assert.equal(typeof(scalars[UINT_SCALAR]), "number",
               UINT_SCALAR + " must be serialized to the correct format.");
  Assert.ok(Number.isInteger(scalars[UINT_SCALAR]),
               UINT_SCALAR + " must be a finite integer.");
  Assert.equal(scalars[UINT_SCALAR], expectedUint,
               UINT_SCALAR + " must have the correct value.");
  Assert.equal(typeof(scalars[STRING_SCALAR]), "string",
               STRING_SCALAR + " must be serialized to the correct format.");
  Assert.equal(scalars[STRING_SCALAR], expectedString,
               STRING_SCALAR + " must have the correct value.");
  Assert.equal(typeof(scalars[BOOLEAN_SCALAR]), "boolean",
               BOOLEAN_SCALAR + " must be serialized to the correct format.");
  Assert.equal(scalars[BOOLEAN_SCALAR], true,
               BOOLEAN_SCALAR + " must have the correct value.");
});

add_task(function* test_nonexistingScalar() {
  const NON_EXISTING_SCALAR = "telemetry.test.non_existing";

  Telemetry.clearScalars();

  // Make sure we throw on any operation for non-existing scalars.
  Assert.throws(() => Telemetry.scalarAdd(NON_EXISTING_SCALAR, 11715),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Adding to a non existing scalar must throw.");
  Assert.throws(() => Telemetry.scalarSet(NON_EXISTING_SCALAR, 11715),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Setting a non existing scalar must throw.");
  Assert.throws(() => Telemetry.scalarSetMaximum(NON_EXISTING_SCALAR, 11715),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Setting the maximum of a non existing scalar must throw.");

  // Get a snapshot of the scalars.
  const scalars =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  Assert.ok(!(NON_EXISTING_SCALAR in scalars), "The non existing scalar must not be persisted.");
});

add_task(function* test_expiredScalar() {
  const EXPIRED_SCALAR = "telemetry.test.expired";
  const UNEXPIRED_SCALAR = "telemetry.test.unexpired";

  Telemetry.clearScalars();

  // Try to set the expired scalar to some value. We will not be recording the value,
  // but we shouldn't throw.
  Telemetry.scalarAdd(EXPIRED_SCALAR, 11715);
  Telemetry.scalarSet(EXPIRED_SCALAR, 11715);
  Telemetry.scalarSetMaximum(EXPIRED_SCALAR, 11715);

  // The unexpired scalar has an expiration version, but far away in the future.
  const expectedValue = 11716;
  Telemetry.scalarSet(UNEXPIRED_SCALAR, expectedValue);

  // Get a snapshot of the scalars.
  const scalars =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  Assert.ok(!(EXPIRED_SCALAR in scalars), "The expired scalar must not be persisted.");
  Assert.equal(scalars[UNEXPIRED_SCALAR], expectedValue,
               "The unexpired scalar must be persisted with the correct value.");
});

add_task(function* test_unsignedIntScalar() {
  let checkScalar = (expectedValue) => {
    const scalars =
      Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    Assert.equal(scalars[UINT_SCALAR], expectedValue,
                 UINT_SCALAR + " must contain the expected value.");
  };

  Telemetry.clearScalars();

  // Let's start with an accumulation without a prior set.
  Telemetry.scalarAdd(UINT_SCALAR, 1);
  Telemetry.scalarAdd(UINT_SCALAR, 2);
  // Do we get what we expect?
  checkScalar(3);

  // Let's test setting the scalar to a value.
  Telemetry.scalarSet(UINT_SCALAR, 3785);
  checkScalar(3785);
  Telemetry.scalarAdd(UINT_SCALAR, 1);
  checkScalar(3786);

  // Does setMaximum work?
  Telemetry.scalarSet(UINT_SCALAR, 2);
  checkScalar(2);
  Telemetry.scalarSetMaximum(UINT_SCALAR, 5);
  checkScalar(5);
  // The value of the probe should still be 5, as the previous value
  // is greater than the one we want to set.
  Telemetry.scalarSetMaximum(UINT_SCALAR, 3);
  checkScalar(5);

  // Check that non-integer numbers get truncated and set.
  Telemetry.scalarSet(UINT_SCALAR, 3.785);
  checkScalar(3);

  // Setting or adding a negative number must report an error through
  // the console and drop the change (shouldn't throw).
  Telemetry.scalarAdd(UINT_SCALAR, -5);
  Telemetry.scalarSet(UINT_SCALAR, -5);
  Telemetry.scalarSetMaximum(UINT_SCALAR, -1);
  checkScalar(3);

  // What happens if we try to set a value of a different type?
  Telemetry.scalarSet(UINT_SCALAR, 1);
  Assert.throws(() => Telemetry.scalarSet(UINT_SCALAR, "unexpected value"),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Setting the scalar to an unexpected value type must throw.");
  Assert.throws(() => Telemetry.scalarAdd(UINT_SCALAR, "unexpected value"),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Adding an unexpected value type must throw.");
  Assert.throws(() => Telemetry.scalarSetMaximum(UINT_SCALAR, "unexpected value"),
                /NS_ERROR_ILLEGAL_VALUE/,
                "Setting the scalar to an unexpected value type must throw.");
  // The stored value must not be compromised.
  checkScalar(1);
});

add_task(function* test_stringScalar() {
  let checkExpectedString = (expectedString) => {
    const scalars =
      Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    Assert.equal(scalars[STRING_SCALAR], expectedString,
                 STRING_SCALAR + " must contain the expected string value.");
  };

  Telemetry.clearScalars();

  // Let's check simple strings...
  let expected = "test string";
  Telemetry.scalarSet(STRING_SCALAR, expected);
  checkExpectedString(expected);
  expected = "漢語";
  Telemetry.scalarSet(STRING_SCALAR, expected);
  checkExpectedString(expected);

  // We have some unsupported operations for strings.
  Assert.throws(() => Telemetry.scalarAdd(STRING_SCALAR, 1),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarAdd(STRING_SCALAR, "string value"),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarSetMaximum(STRING_SCALAR, 1),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarSetMaximum(STRING_SCALAR, "string value"),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarSet(STRING_SCALAR, 1),
                /NS_ERROR_ILLEGAL_VALUE/,
                "The string scalar must throw if we're not setting a string.");

  // Try to set the scalar to a string longer than the maximum length limit.
  const LONG_STRING = "browser.qaxfiuosnzmhlg.rpvxicawolhtvmbkpnludhedobxvkjwqyeyvmv";
  Telemetry.scalarSet(STRING_SCALAR, LONG_STRING);
  checkExpectedString(LONG_STRING.substr(0, 50));
});

add_task(function* test_booleanScalar() {
  let checkExpectedBool = (expectedBoolean) => {
    const scalars =
      Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    Assert.equal(scalars[BOOLEAN_SCALAR], expectedBoolean,
                 BOOLEAN_SCALAR + " must contain the expected boolean value.");
  };

  Telemetry.clearScalars();

  // Set a test boolean value.
  let expected = false;
  Telemetry.scalarSet(BOOLEAN_SCALAR, expected);
  checkExpectedBool(expected);
  expected = true;
  Telemetry.scalarSet(BOOLEAN_SCALAR, expected);
  checkExpectedBool(expected);

  // Check that setting a numeric value implicitly converts to boolean.
  Telemetry.scalarSet(BOOLEAN_SCALAR, 1);
  checkExpectedBool(true);
  Telemetry.scalarSet(BOOLEAN_SCALAR, 0);
  checkExpectedBool(false);
  Telemetry.scalarSet(BOOLEAN_SCALAR, 1.0);
  checkExpectedBool(true);
  Telemetry.scalarSet(BOOLEAN_SCALAR, 0.0);
  checkExpectedBool(false);

  // Check that unsupported operations for booleans throw.
  Assert.throws(() => Telemetry.scalarAdd(BOOLEAN_SCALAR, 1),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarAdd(BOOLEAN_SCALAR, "string value"),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarSetMaximum(BOOLEAN_SCALAR, 1),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarSetMaximum(BOOLEAN_SCALAR, "string value"),
                /NS_ERROR_NOT_AVAILABLE/,
                "Using an unsupported operation must throw.");
  Assert.throws(() => Telemetry.scalarSet(BOOLEAN_SCALAR, "true"),
                /NS_ERROR_ILLEGAL_VALUE/,
                "The boolean scalar must throw if we're not setting a boolean.");
});

add_task(function* test_scalarRecording() {
  const OPTIN_SCALAR = "telemetry.test.release_optin";
  const OPTOUT_SCALAR = "telemetry.test.release_optout";

  let checkValue = (scalarName, expectedValue) => {
    const scalars =
      Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    Assert.equal(scalars[scalarName], expectedValue,
                 scalarName + " must contain the expected value.");
  };

  let checkNotSerialized = (scalarName) => {
    const scalars =
      Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    Assert.ok(!(scalarName in scalars), scalarName + " was not recorded.");
  };

  Telemetry.canRecordBase = false;
  Telemetry.canRecordExtended = false;
  Telemetry.clearScalars();

  // Check that no scalar is recorded if both base and extended recording are off.
  Telemetry.scalarSet(OPTOUT_SCALAR, 3);
  Telemetry.scalarSet(OPTIN_SCALAR, 3);
  checkNotSerialized(OPTOUT_SCALAR);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that opt-out scalars are recorded, while opt-in are not.
  Telemetry.canRecordBase = true;
  Telemetry.scalarSet(OPTOUT_SCALAR, 3);
  Telemetry.scalarSet(OPTIN_SCALAR, 3);
  checkValue(OPTOUT_SCALAR, 3);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that both opt-out and opt-in scalars are recorded.
  Telemetry.canRecordExtended = true;
  Telemetry.scalarSet(OPTOUT_SCALAR, 5);
  Telemetry.scalarSet(OPTIN_SCALAR, 6);
  checkValue(OPTOUT_SCALAR, 5);
  checkValue(OPTIN_SCALAR, 6);
});

add_task(function* test_subsession() {
  Telemetry.clearScalars();

  // Set the scalars to a known value.
  Telemetry.scalarSet(UINT_SCALAR, 3785);
  Telemetry.scalarSet(STRING_SCALAR, "some value");
  Telemetry.scalarSet(BOOLEAN_SCALAR, false);

  // Get a snapshot and reset the subsession. The value we set must be there.
  let scalars =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(scalars[UINT_SCALAR], 3785,
               UINT_SCALAR + " must contain the expected value.");
  Assert.equal(scalars[STRING_SCALAR], "some value",
               STRING_SCALAR + " must contain the expected value.");
  Assert.equal(scalars[BOOLEAN_SCALAR], false,
               BOOLEAN_SCALAR + " must contain the expected value.");

  // Get a new snapshot and reset the subsession again. Since no new value
  // was set, the scalars should not be reported.
  scalars =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.ok(!(UINT_SCALAR in scalars), UINT_SCALAR + " must be empty and not reported.");
  Assert.ok(!(STRING_SCALAR in scalars), STRING_SCALAR + " must be empty and not reported.");
  Assert.ok(!(BOOLEAN_SCALAR in scalars), BOOLEAN_SCALAR + " must be empty and not reported.");
});
