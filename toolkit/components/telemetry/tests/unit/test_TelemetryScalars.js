/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const UINT_SCALAR = "telemetry.test.unsigned_int_kind";
const STRING_SCALAR = "telemetry.test.string_kind";
const BOOLEAN_SCALAR = "telemetry.test.boolean_kind";
const KEYED_UINT_SCALAR = "telemetry.test.keyed_unsigned_int";

function getProcessScalars(aChannel, aProcessName, aKeyed = false, aClear = false) {
  const scalars = aKeyed ?
    Telemetry.snapshotKeyedScalars(aChannel, aClear)[aProcessName] :
    Telemetry.snapshotScalars(aChannel, aClear)[aProcessName];
  return scalars || {};
}

function getParentProcessScalars(aChannel, aKeyed = false, aClear = false) {
  return getProcessScalars(aChannel, "parent", aKeyed, aClear);
}

add_task(async function test_serializationFormat() {
  Telemetry.clearScalars();

  // Set the scalars to a known value.
  const expectedUint = 3785;
  const expectedString = "some value";
  Telemetry.scalarSet(UINT_SCALAR, expectedUint);
  Telemetry.scalarSet(STRING_SCALAR, expectedString);
  Telemetry.scalarSet(BOOLEAN_SCALAR, true);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, "first_key", 1234);

  // Get a snapshot of the scalars for the main process (internally called "default").
  const scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

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
  Assert.ok(!(KEYED_UINT_SCALAR in scalars),
            "Keyed scalars must be reported in a separate section.");
});

add_task(async function test_keyedSerializationFormat() {
  Telemetry.clearScalars();

  const expectedKey = "first_key";
  const expectedOtherKey = "漢語";
  const expectedUint = 3785;
  const expectedOtherValue = 1107;

  Telemetry.scalarSet(UINT_SCALAR, expectedUint);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, expectedKey, expectedUint);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, expectedOtherKey, expectedOtherValue);

  // Get a snapshot of the scalars.
  const keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  Assert.ok(!(UINT_SCALAR in keyedScalars),
            UINT_SCALAR + " must not be serialized with the keyed scalars.");
  Assert.ok(KEYED_UINT_SCALAR in keyedScalars,
            KEYED_UINT_SCALAR + " must be serialized with the keyed scalars.");
  Assert.equal(Object.keys(keyedScalars[KEYED_UINT_SCALAR]).length, 2,
               "The keyed scalar must contain exactly 2 keys.");
  Assert.ok(expectedKey in keyedScalars[KEYED_UINT_SCALAR],
            KEYED_UINT_SCALAR + " must contain the expected keys.");
  Assert.ok(expectedOtherKey in keyedScalars[KEYED_UINT_SCALAR],
            KEYED_UINT_SCALAR + " must contain the expected keys.");
  Assert.ok(Number.isInteger(keyedScalars[KEYED_UINT_SCALAR][expectedKey]),
               KEYED_UINT_SCALAR + "." + expectedKey + " must be a finite integer.");
  Assert.equal(keyedScalars[KEYED_UINT_SCALAR][expectedKey], expectedUint,
               KEYED_UINT_SCALAR + "." + expectedKey + " must have the correct value.");
  Assert.equal(keyedScalars[KEYED_UINT_SCALAR][expectedOtherKey], expectedOtherValue,
               KEYED_UINT_SCALAR + "." + expectedOtherKey + " must have the correct value.");
});

add_task(async function test_nonexistingScalar() {
  const NON_EXISTING_SCALAR = "telemetry.test.non_existing";

  Telemetry.clearScalars();

  // The JS API must not throw when used incorrectly but rather print
  // a message to the console.
  Telemetry.scalarAdd(NON_EXISTING_SCALAR, 11715);
  Telemetry.scalarSet(NON_EXISTING_SCALAR, 11715);
  Telemetry.scalarSetMaximum(NON_EXISTING_SCALAR, 11715);

  // Make sure we do not throw on any operation for non-existing scalars.
  Telemetry.keyedScalarAdd(NON_EXISTING_SCALAR, "some_key", 11715);
  Telemetry.keyedScalarSet(NON_EXISTING_SCALAR, "some_key", 11715);
  Telemetry.keyedScalarSetMaximum(NON_EXISTING_SCALAR, "some_key", 11715);

  // Get a snapshot of the scalars.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  Assert.ok(!(NON_EXISTING_SCALAR in scalars), "The non existing scalar must not be persisted.");

  const keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  Assert.ok(!(NON_EXISTING_SCALAR in keyedScalars),
            "The non existing keyed scalar must not be persisted.");
});

add_task(async function test_expiredScalar() {
  const EXPIRED_SCALAR = "telemetry.test.expired";
  const EXPIRED_KEYED_SCALAR = "telemetry.test.keyed_expired";
  const UNEXPIRED_SCALAR = "telemetry.test.unexpired";

  Telemetry.clearScalars();

  // Try to set the expired scalar to some value. We will not be recording the value,
  // but we shouldn't throw.
  Telemetry.scalarAdd(EXPIRED_SCALAR, 11715);
  Telemetry.scalarSet(EXPIRED_SCALAR, 11715);
  Telemetry.scalarSetMaximum(EXPIRED_SCALAR, 11715);
  Telemetry.keyedScalarAdd(EXPIRED_KEYED_SCALAR, "some_key", 11715);
  Telemetry.keyedScalarSet(EXPIRED_KEYED_SCALAR, "some_key", 11715);
  Telemetry.keyedScalarSetMaximum(EXPIRED_KEYED_SCALAR, "some_key", 11715);

  // The unexpired scalar has an expiration version, but far away in the future.
  const expectedValue = 11716;
  Telemetry.scalarSet(UNEXPIRED_SCALAR, expectedValue);

  // Get a snapshot of the scalars.
  const scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
  const keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  Assert.ok(!(EXPIRED_SCALAR in scalars), "The expired scalar must not be persisted.");
  Assert.equal(scalars[UNEXPIRED_SCALAR], expectedValue,
               "The unexpired scalar must be persisted with the correct value.");
  Assert.ok(!(EXPIRED_KEYED_SCALAR in keyedScalars),
            "The expired keyed scalar must not be persisted.");
});

add_task(async function test_unsignedIntScalar() {
  let checkScalar = (expectedValue) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
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

  // If we try to set a value of a different type, the JS API should not
  // throw but rather print a console message.
  Telemetry.scalarSet(UINT_SCALAR, 1);
  Telemetry.scalarSet(UINT_SCALAR, "unexpected value");
  Telemetry.scalarAdd(UINT_SCALAR, "unexpected value");
  Telemetry.scalarSetMaximum(UINT_SCALAR, "unexpected value");
  // The stored value must not be compromised.
  checkScalar(1);
});

add_task(async function test_stringScalar() {
  let checkExpectedString = (expectedString) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
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
  Telemetry.scalarAdd(STRING_SCALAR, 1);
  Telemetry.scalarAdd(STRING_SCALAR, "string value");
  Telemetry.scalarSetMaximum(STRING_SCALAR, 1);
  Telemetry.scalarSetMaximum(STRING_SCALAR, "string value");
  Telemetry.scalarSet(STRING_SCALAR, 1);

  // Try to set the scalar to a string longer than the maximum length limit.
  const LONG_STRING = "browser.qaxfiuosnzmhlg.rpvxicawolhtvmbkpnludhedobxvkjwqyeyvmv";
  Telemetry.scalarSet(STRING_SCALAR, LONG_STRING);
  checkExpectedString(LONG_STRING.substr(0, 50));
});

add_task(async function test_booleanScalar() {
  let checkExpectedBool = (expectedBoolean) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
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

  // Check that unsupported operations for booleans do not throw.
  Telemetry.scalarAdd(BOOLEAN_SCALAR, 1);
  Telemetry.scalarAdd(BOOLEAN_SCALAR, "string value");
  Telemetry.scalarSetMaximum(BOOLEAN_SCALAR, 1);
  Telemetry.scalarSetMaximum(BOOLEAN_SCALAR, "string value");
  Telemetry.scalarSet(BOOLEAN_SCALAR, "true");
});

add_task(async function test_scalarRecording() {
  const OPTIN_SCALAR = "telemetry.test.release_optin";
  const OPTOUT_SCALAR = "telemetry.test.release_optout";

  let checkValue = (scalarName, expectedValue) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    Assert.equal(scalars[scalarName], expectedValue,
                 scalarName + " must contain the expected value.");
  };

  let checkNotSerialized = (scalarName) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
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

add_task(async function test_keyedScalarRecording() {
  const OPTIN_SCALAR = "telemetry.test.keyed_release_optin";
  const OPTOUT_SCALAR = "telemetry.test.keyed_release_optout";
  const testKey = "policy_key";

  let checkValue = (scalarName, expectedValue) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
    Assert.equal(scalars[scalarName][testKey], expectedValue,
                 scalarName + " must contain the expected value.");
  };

  let checkNotSerialized = (scalarName) => {
    const scalars =
      getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
    Assert.ok(!(scalarName in scalars), scalarName + " was not recorded.");
  };

  Telemetry.canRecordBase = false;
  Telemetry.canRecordExtended = false;
  Telemetry.clearScalars();

  // Check that no scalar is recorded if both base and extended recording are off.
  Telemetry.keyedScalarSet(OPTOUT_SCALAR, testKey, 3);
  Telemetry.keyedScalarSet(OPTIN_SCALAR, testKey, 3);
  checkNotSerialized(OPTOUT_SCALAR);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that opt-out scalars are recorded, while opt-in are not.
  Telemetry.canRecordBase = true;
  Telemetry.keyedScalarSet(OPTOUT_SCALAR, testKey, 3);
  Telemetry.keyedScalarSet(OPTIN_SCALAR, testKey, 3);
  checkValue(OPTOUT_SCALAR, 3);
  checkNotSerialized(OPTIN_SCALAR);

  // Check that both opt-out and opt-in scalars are recorded.
  Telemetry.canRecordExtended = true;
  Telemetry.keyedScalarSet(OPTOUT_SCALAR, testKey, 5);
  Telemetry.keyedScalarSet(OPTIN_SCALAR, testKey, 6);
  checkValue(OPTOUT_SCALAR, 5);
  checkValue(OPTIN_SCALAR, 6);
});

add_task(async function test_subsession() {
  Telemetry.clearScalars();

  // Set the scalars to a known value.
  Telemetry.scalarSet(UINT_SCALAR, 3785);
  Telemetry.scalarSet(STRING_SCALAR, "some value");
  Telemetry.scalarSet(BOOLEAN_SCALAR, false);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, "some_random_key", 12);

  // Get a snapshot and reset the subsession. The value we set must be there.
  let scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false, true);
  let keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, true);

  Assert.equal(scalars[UINT_SCALAR], 3785,
               UINT_SCALAR + " must contain the expected value.");
  Assert.equal(scalars[STRING_SCALAR], "some value",
               STRING_SCALAR + " must contain the expected value.");
  Assert.equal(scalars[BOOLEAN_SCALAR], false,
               BOOLEAN_SCALAR + " must contain the expected value.");
  Assert.equal(keyedScalars[KEYED_UINT_SCALAR].some_random_key, 12,
               KEYED_UINT_SCALAR + " must contain the expected value.");

  // Get a new snapshot and reset the subsession again. Since no new value
  // was set, the scalars should not be reported.
  scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false, true);
  keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, true);

  Assert.ok(!(UINT_SCALAR in scalars), UINT_SCALAR + " must be empty and not reported.");
  Assert.ok(!(STRING_SCALAR in scalars), STRING_SCALAR + " must be empty and not reported.");
  Assert.ok(!(BOOLEAN_SCALAR in scalars), BOOLEAN_SCALAR + " must be empty and not reported.");
  Assert.ok(!(KEYED_UINT_SCALAR in keyedScalars), KEYED_UINT_SCALAR + " must be empty and not reported.");
});

add_task(async function test_keyed_uint() {
  Telemetry.clearScalars();

  const KEYS = [ "a_key", "another_key", "third_key" ];
  let expectedValues = [ 1, 1, 1 ];

  // Set all the keys to a baseline value.
  for (let key of KEYS) {
    Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, key, 1);
  }

  // Increment only one key.
  Telemetry.keyedScalarAdd(KEYED_UINT_SCALAR, KEYS[1], 1);
  expectedValues[1]++;

  // Use SetMaximum on the third key.
  Telemetry.keyedScalarSetMaximum(KEYED_UINT_SCALAR, KEYS[2], 37);
  expectedValues[2] = 37;

  // Get a snapshot of the scalars and make sure the keys contain
  // the correct values.
  const keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  for (let k = 0; k < 3; k++) {
    const keyName = KEYS[k];
    Assert.equal(keyedScalars[KEYED_UINT_SCALAR][keyName], expectedValues[k],
                 KEYED_UINT_SCALAR + "." + keyName + " must contain the correct value.");
  }

  // Do not throw when doing unsupported things on uint keyed scalars.
  // Just test one single unsupported operation, the other are covered in the plain
  // unsigned scalar test.
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, "new_key", "unexpected value");
});

add_task(async function test_keyed_boolean() {
  Telemetry.clearScalars();

  const KEYED_BOOLEAN_TYPE = "telemetry.test.keyed_boolean_kind";
  const first_key = "first_key";
  const second_key = "second_key";

  // Set the initial values.
  Telemetry.keyedScalarSet(KEYED_BOOLEAN_TYPE, first_key, true);
  Telemetry.keyedScalarSet(KEYED_BOOLEAN_TYPE, second_key, false);

  // Get a snapshot of the scalars and make sure the keys contain
  // the correct values.
  let keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(keyedScalars[KEYED_BOOLEAN_TYPE][first_key], true,
               "The key must contain the expected value.");
  Assert.equal(keyedScalars[KEYED_BOOLEAN_TYPE][second_key], false,
               "The key must contain the expected value.");

  // Now flip the values and make sure we get the expected values back.
  Telemetry.keyedScalarSet(KEYED_BOOLEAN_TYPE, first_key, false);
  Telemetry.keyedScalarSet(KEYED_BOOLEAN_TYPE, second_key, true);

  keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(keyedScalars[KEYED_BOOLEAN_TYPE][first_key], false,
               "The key must contain the expected value.");
  Assert.equal(keyedScalars[KEYED_BOOLEAN_TYPE][second_key], true,
               "The key must contain the expected value.");

  // Do not throw when doing unsupported things on a boolean keyed scalars.
  // Just test one single unsupported operation, the other are covered in the plain
  // boolean scalar test.
  Telemetry.keyedScalarAdd(KEYED_BOOLEAN_TYPE, "somehey", 1);
});

add_task(async function test_keyed_keys_length() {
  Telemetry.clearScalars();

  const LONG_KEY_STRING =
    "browser.qaxfiuosnzmhlg.rpvxicawolhtvmbkpnludhedobxvkjwqyeyvmv.somemoresowereach70chars";
  const NORMAL_KEY = "a_key";

  // Set the value for a key within the length limits.
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, NORMAL_KEY, 1);

  // Now try to set and modify the value for a very long key (must not throw).
  Telemetry.keyedScalarAdd(KEYED_UINT_SCALAR, LONG_KEY_STRING, 10);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, LONG_KEY_STRING, 1);
  Telemetry.keyedScalarSetMaximum(KEYED_UINT_SCALAR, LONG_KEY_STRING, 10);

  // Also attempt to set the value for an empty key.
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, "", 1);

  // Make sure the key with the right length contains the expected value.
  let keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(Object.keys(keyedScalars[KEYED_UINT_SCALAR]).length, 1,
               "The keyed scalar must contain exactly 1 key.");
  Assert.ok(NORMAL_KEY in keyedScalars[KEYED_UINT_SCALAR],
            "The keyed scalar must contain the expected key.");
  Assert.equal(keyedScalars[KEYED_UINT_SCALAR][NORMAL_KEY], 1,
               "The key must contain the expected value.");
  Assert.ok(!(LONG_KEY_STRING in keyedScalars[KEYED_UINT_SCALAR]),
            "The data for the long key should not have been recorded.");
  Assert.ok(!("" in keyedScalars[KEYED_UINT_SCALAR]),
            "The data for the empty key should not have been recorded.");
});

add_task(async function test_keyed_max_keys() {
  Telemetry.clearScalars();

  // Generate the names for the first 100 keys.
  let keyNamesSet = new Set();
  for (let k = 0; k < 100; k++) {
    keyNamesSet.add("key_" + k);
  }

  // Add 100 keys to an histogram and set their initial value.
  let valueToSet = 0;
  keyNamesSet.forEach(keyName => {
    Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, keyName, valueToSet++);
  });

  // Perform some operations on the 101th key. This should throw, as
  // we're not allowed to have more than 100 keys.
  const LAST_KEY_NAME = "overflowing_key";
  Telemetry.keyedScalarAdd(KEYED_UINT_SCALAR, LAST_KEY_NAME, 10);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, LAST_KEY_NAME, 1);
  Telemetry.keyedScalarSetMaximum(KEYED_UINT_SCALAR, LAST_KEY_NAME, 10);

  // Make sure all the keys except the last one are available and have the correct
  // values.
  let keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  // Check that the keyed scalar only contain the first 100 keys.
  const reportedKeysSet = new Set(Object.keys(keyedScalars[KEYED_UINT_SCALAR]));
  Assert.ok([...keyNamesSet].filter(x => reportedKeysSet.has(x)) &&
            [...reportedKeysSet].filter(x => keyNamesSet.has(x)),
            "The keyed scalar must contain all the 100 keys, and drop the others.");

  // Check that all the keys recorded the expected values.
  let expectedValue = 0;
  keyNamesSet.forEach(keyName => {
    Assert.equal(keyedScalars[KEYED_UINT_SCALAR][keyName], expectedValue++,
                 "The key must contain the expected value.");
  });
});

add_task(async function test_dynamicScalars_registration() {
  Telemetry.clearScalars();

  const TEST_CASES = [
    {
      "category": "telemetry.test",
      "data": {
        "missing_kind": {
          keyed: false,
          record_on_release: true
        },
      },
      "evaluation": /missing 'kind'/,
      "description": "Registration must fail if required fields are missing"
    },
    {
      "category": "telemetry.test",
      "data": {
        "invalid_collection": {
          kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
          record_on_release: "opt-in"
        },
      },
      "evaluation": /Invalid 'record_on_release'/,
      "description": "Registration must fail if 'record_on_release' is of the wrong type"
    },
    {
      "category": "telemetry.test",
      "data": {
        "invalid_kind": {
          kind: "12",
        },
      },
      "evaluation": /Invalid or missing 'kind'/,
      "description": "Registration must fail if 'kind' is of the wrong type"
    },
    {
      "category": "telemetry.test",
      "data": {
        "invalid_expired": {
          kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
          expired: "never",
        },
      },
      "evaluation": /Invalid 'expired'/,
      "description": "Registration must fail if 'expired' is of the wrong type"
    },
    {
      "category": "telemetry.test",
      "data": {
        "valid_scalar": {
          kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
          keyed: false,
          record_on_release: true
        },
        "invalid_scalar": {
          expired: false,
        },
      },
      "evaluation": /Invalid or missing 'kind'/,
      "description": "No scalar must be registered if the batch contains an invalid one"
    },
  ];

  for (let testCase of TEST_CASES) {
    Assert.throws(() => Telemetry.registerScalars(testCase.category, testCase.data),
                  testCase.evaluation, testCase.description);
  }
});

add_task(async function test_dynamicScalars_doubleRegistration() {
  Telemetry.clearScalars();

  // Register a test scalar.
  Telemetry.registerScalars("telemetry.test.dynamic", {
    "double_registration_1": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      record_on_release: true
    },
  });

  // Verify that we can record the scalar.
  Telemetry.scalarSet("telemetry.test.dynamic.double_registration_1", 1);

  // Register the same scalar again, along with a second scalar.
  // This must not throw.
  Telemetry.registerScalars("telemetry.test.dynamic", {
    "double_registration_1": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      record_on_release: true
    },
    "double_registration_2": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      record_on_release: true
    },
  });

  // Set the dynamic scalars to some test values.
  Telemetry.scalarAdd("telemetry.test.dynamic.double_registration_1", 1);
  Telemetry.scalarSet("telemetry.test.dynamic.double_registration_2", 3);

  // Get a snapshot of the scalars and check that the dynamic ones were correctly set.
  let scalars =
    getProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, "dynamic", false, false);

  Assert.equal(scalars["telemetry.test.dynamic.double_registration_1"], 2,
               "The recorded scalar must contain the right value.");
  Assert.equal(scalars["telemetry.test.dynamic.double_registration_2"], 3,
               "The recorded scalar must contain the right value.");

  // Register an existing scalar again, only change the definition
  // to make it expire.
  Telemetry.registerScalars("telemetry.test.dynamic", {
    "double_registration_2": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      record_on_release: true,
      expired: true
    },
  });

  // Attempt to record and make sure that no recording happens.
  Telemetry.scalarAdd("telemetry.test.dynamic.double_registration_2", 1);
  scalars =
    getProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, "dynamic", false, false);
  Assert.equal(scalars["telemetry.test.dynamic.double_registration_2"], 3,
               "The recorded scalar must contain the right value.");
});

add_task(async function test_dynamicScalars_recording() {
  Telemetry.clearScalars();

  // Disable extended recording so that we will just record opt-out.
  Telemetry.canRecordExtended = false;

  // Register some test scalars.
  Telemetry.registerScalars("telemetry.test.dynamic", {
    "record_optout": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      record_on_release: true
    },
    "record_keyed": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      keyed: true,
      record_on_release: true
    },
    "record_optin": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_BOOLEAN,
      record_on_release: false
    },
    "record_expired": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_STRING,
      expired: true,
      record_on_release: true
    },
  });

  // Set the dynamic scalars to some test values.
  Telemetry.scalarSet("telemetry.test.dynamic.record_optout", 1);
  Telemetry.keyedScalarSet("telemetry.test.dynamic.record_keyed", "someKey", 5);
  Telemetry.scalarSet("telemetry.test.dynamic.record_optin", false);
  Telemetry.scalarSet("telemetry.test.dynamic.record_expired", "test");

  // Get a snapshot of the scalars and check that the dynamic ones were correctly set.
  let scalars =
    getProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, "dynamic", false, false);
  let keyedScalars =
    getProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, "dynamic", true, true);

  Assert.ok(!("telemetry.test.dynamic.record_optin" in scalars),
            "Dynamic opt-in scalars must not be recorded.");
  Assert.ok("telemetry.test.dynamic.record_keyed" in keyedScalars,
            "Dynamic opt-out keyed scalars must be recorded.");
  Assert.ok(!("telemetry.test.dynamic.record_expired" in scalars),
            "Dynamic expired scalars must not be recorded.");
  Assert.ok("telemetry.test.dynamic.record_optout" in scalars,
            "Dynamic opt-out scalars must be recorded.");
  Assert.equal(scalars["telemetry.test.dynamic.record_optout"], 1,
               "The recorded scalar must contain the right value.");
  Assert.equal(keyedScalars["telemetry.test.dynamic.record_keyed"].someKey, 5,
               "The recorded keyed scalar must contain the right value.");

  // Enable extended recording.
  Telemetry.canRecordExtended = true;

  // Set the dynamic scalars to some test values.
  Telemetry.scalarSet("telemetry.test.dynamic.record_optin", true);
  Telemetry.scalarSet("telemetry.test.dynamic.record_expired", "test");

  // Get a snapshot of the scalars and check that the dynamic ones were correctly set.
  scalars =
    getProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, "dynamic", false, true);

  Assert.ok(!("telemetry.test.dynamic.record_expired" in scalars),
            "Dynamic expired scalars must not be recorded.");
  Assert.ok("telemetry.test.dynamic.record_optin" in scalars,
            "Dynamic opt-in scalars must be recorded.");
  Assert.equal(scalars["telemetry.test.dynamic.record_optin"], true,
               "The recorded scalar must contain the right value.");
});

add_task({
  skip_if: () => gIsAndroid
},
async function test_productSpecificScalar() {
  const DEFAULT_PRODUCT_SCALAR = "telemetry.test.default_products";
  const DESKTOP_ONLY_SCALAR = "telemetry.test.desktop_only";
  const MULTIPRODUCT_SCALAR = "telemetry.test.multiproduct";
  const MOBILE_ONLY_SCALAR = "telemetry.test.mobile_only";
  const MOBILE_ONLY_KEYED_SCALAR = "telemetry.test.keyed_mobile_only";

  Telemetry.clearScalars();

  // Try to set the desktop scalars
  let expectedValue = 11714;
  Telemetry.scalarAdd(DEFAULT_PRODUCT_SCALAR, expectedValue);
  Telemetry.scalarAdd(DESKTOP_ONLY_SCALAR, expectedValue);
  Telemetry.scalarAdd(MULTIPRODUCT_SCALAR, expectedValue);

  // Try to set the mobile-only scalar to some value. We will not be recording the value,
  // but we shouldn't throw.
  let expectedKey = "some_key";
  Telemetry.scalarSet(MOBILE_ONLY_SCALAR, 11715);
  Telemetry.scalarSetMaximum(MOBILE_ONLY_SCALAR, 11715);
  Telemetry.keyedScalarAdd(MOBILE_ONLY_KEYED_SCALAR, expectedKey, 11715);
  Telemetry.keyedScalarSet(MOBILE_ONLY_KEYED_SCALAR, expectedKey, 11715);
  Telemetry.keyedScalarSetMaximum(MOBILE_ONLY_KEYED_SCALAR, expectedKey, 11715);

  // Get a snapshot of the scalars.
  const scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
  const keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  Assert.equal(scalars[DEFAULT_PRODUCT_SCALAR], expectedValue, "The default platfomrs scalar must contain the right value");
  Assert.equal(scalars[DESKTOP_ONLY_SCALAR], expectedValue, "The desktop-only scalar must contain the right value");
  Assert.equal(scalars[MULTIPRODUCT_SCALAR], expectedValue, "The multiproduct scalar must contain the right value");

  Assert.ok(!(MOBILE_ONLY_SCALAR in scalars), "The mobile-only scalar must not be persisted.");
  Assert.ok(!(MOBILE_ONLY_KEYED_SCALAR in keyedScalars),
            "The mobile-only keyed scalar must not be persisted.");
});

add_task({
  skip_if: () => !gIsAndroid
},
async function test_mobileSpecificScalar() {
  const DEFAULT_PRODUCT_SCALAR = "telemetry.test.default_products";
  const DESKTOP_ONLY_SCALAR = "telemetry.test.desktop_only";
  const DESKTOP_ONLY_KEYED_SCALAR = "telemetry.test.keyed_desktop_only";
  const MULTIPRODUCT_SCALAR = "telemetry.test.multiproduct";
  const MOBILE_ONLY_SCALAR = "telemetry.test.mobile_only";
  const MOBILE_ONLY_KEYED_SCALAR = "telemetry.test.keyed_mobile_only";

  Telemetry.clearScalars();

  // Try to set the mobile and multiproduct scalars
  let expectedValue = 11714;
  let expectedKey = "some_key";
  Telemetry.scalarAdd(DEFAULT_PRODUCT_SCALAR, expectedValue);
  Telemetry.scalarAdd(MOBILE_ONLY_SCALAR, expectedValue);
  Telemetry.keyedScalarSet(MOBILE_ONLY_KEYED_SCALAR, expectedKey, expectedValue);
  Telemetry.scalarAdd(MULTIPRODUCT_SCALAR, expectedValue);

  // Try to set the desktop-only scalar to some value. We will not be recording the value,
  // but we shouldn't throw.
  Telemetry.scalarSet(DESKTOP_ONLY_SCALAR, 11715);
  Telemetry.scalarSetMaximum(DESKTOP_ONLY_SCALAR, 11715);
  Telemetry.keyedScalarAdd(DESKTOP_ONLY_KEYED_SCALAR, expectedKey, 11715);
  Telemetry.keyedScalarSet(DESKTOP_ONLY_KEYED_SCALAR, expectedKey, 11715);
  Telemetry.keyedScalarSetMaximum(DESKTOP_ONLY_KEYED_SCALAR, expectedKey, 11715);

  // Get a snapshot of the scalars.
  const scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
  const keyedScalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);

  Assert.equal(scalars[DEFAULT_PRODUCT_SCALAR], expectedValue, "The default products scalar must contain the right value");
  Assert.equal(scalars[MOBILE_ONLY_SCALAR], expectedValue, "The mobile-only scalar must contain the right value");
  Assert.equal(keyedScalars[MOBILE_ONLY_KEYED_SCALAR][expectedKey], expectedValue, "The mobile-only keyed scalar must contain the right value");
  Assert.equal(scalars[MULTIPRODUCT_SCALAR], expectedValue, "The multiproduct scalar must contain the right value");

  Assert.ok(!(DESKTOP_ONLY_SCALAR in scalars), "The desktop-only scalar must not be persisted.");
  Assert.ok(!(DESKTOP_ONLY_KEYED_SCALAR in keyedScalars),
            "The desktop-only keyed scalar must not be persisted.");
});

add_task({
  skip_if: () => !gIsAndroid
},
async function test_geckoviewSpecificScalar() {
  const DEFAULT_PRODUCT_SCALAR = "telemetry.test.default_products";
  const DESKTOP_ONLY_SCALAR = "telemetry.test.desktop_only";
  const MULTIPRODUCT_SCALAR = "telemetry.test.multiproduct";
  const MOBILE_ONLY_SCALAR = "telemetry.test.mobile_only";
  const GECKOVIEW_ONLY_SCALAR = "telemetry.test.geckoview_only";

  Telemetry.clearScalars();

  // Fake a geckoview-like environment
  Services.prefs.setBoolPref("toolkit.telemetry.isGeckoViewMode", true);
  Telemetry.resetCurrentProduct();

  // Try to set the mobile and multiproduct scalars
  let expectedValue = 11714;
  Telemetry.scalarAdd(GECKOVIEW_ONLY_SCALAR, expectedValue);
  Telemetry.scalarAdd(MOBILE_ONLY_SCALAR, expectedValue);
  Telemetry.scalarAdd(MULTIPRODUCT_SCALAR, expectedValue);
  Telemetry.scalarSet(DEFAULT_PRODUCT_SCALAR, expectedValue);

  // Try to set the desktop-only scalar to some value. We will not be recording the value,
  // but we shouldn't throw.
  Telemetry.scalarSet(DESKTOP_ONLY_SCALAR, 11715);
  Telemetry.scalarSetMaximum(DESKTOP_ONLY_SCALAR, 11715);

  // Get a snapshot of the scalars.
  const scalars =
    getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  Assert.equal(scalars[GECKOVIEW_ONLY_SCALAR], expectedValue, "The geckoview-only scalar must contain the right value");
  Assert.equal(scalars[MOBILE_ONLY_SCALAR], expectedValue, "The mobile-only scalar must contain the right value");
  Assert.equal(scalars[MULTIPRODUCT_SCALAR], expectedValue, "The multiproduct scalar must contain the right value");
  Assert.equal(scalars[DEFAULT_PRODUCT_SCALAR], expectedValue, "The default products scalar must contain the right value");

  Assert.ok(!(DESKTOP_ONLY_SCALAR in scalars), "The desktop-only scalar must not be persisted.");

  // Reset to original environment
  Services.prefs.clearUserPref("toolkit.telemetry.isGeckoViewMode");
  Telemetry.resetCurrentProduct();
});
