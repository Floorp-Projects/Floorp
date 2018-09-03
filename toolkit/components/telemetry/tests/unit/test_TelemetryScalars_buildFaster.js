/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const UINT_SCALAR = "telemetry.test.unsigned_int_kind";
const STRING_SCALAR = "telemetry.test.string_kind";
const BOOLEAN_SCALAR = "telemetry.test.boolean_kind";
const KEYED_UINT_SCALAR = "telemetry.test.keyed_unsigned_int";

ChromeUtils.import("resource://services-common/utils.js");

/**
 * Return the path to the definitions file for the scalars.
 */
function getDefinitionsPath() {
  // Write the scalar definition to the spec file in the binary directory.
  let definitionFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  definitionFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  definitionFile.append("ScalarArtifactDefinitions.json");
  return definitionFile.path;
}

add_task(async function test_setup() {
  do_get_profile();
});

add_task({
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  }, async function test_invalidJSON() {
  const INVALID_JSON = "{ invalid,JSON { {1}";
  const FILE_PATH = getDefinitionsPath();

  // Write a corrupted JSON file.
  await OS.File.writeAtomic(FILE_PATH, INVALID_JSON, { encoding: "utf-8", noOverwrite: false });

  // Simulate Firefox startup. This should not throw!
  await TelemetryController.testSetup();
  await TelemetryController.testPromiseJsProbeRegistration();

  // Cleanup.
  await TelemetryController.testShutdown();
  await OS.File.remove(FILE_PATH);
});

add_task({
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  }, async function test_dynamicBuiltin() {
  const DYNAMIC_SCALAR_SPEC =  {
    "telemetry.test": {
      "builtin_dynamic": {
        "kind": "nsITelemetry::SCALAR_TYPE_COUNT",
        "expired": false,
        "record_on_release": false,
        "keyed": false,
      },
      "builtin_dynamic_other": {
        "kind": "nsITelemetry::SCALAR_TYPE_BOOLEAN",
        "expired": false,
        "record_on_release": false,
        "keyed": false,
      },
    },
  };

  Telemetry.clearScalars();

  // Let's write to the definition file to also cover the file
  // loading part.
  const FILE_PATH = getDefinitionsPath();
  await CommonUtils.writeJSON(DYNAMIC_SCALAR_SPEC, FILE_PATH);

  // Start TelemetryController to trigger loading the specs.
  await TelemetryController.testReset();
  await TelemetryController.testPromiseJsProbeRegistration();

  // Store to that scalar.
  const TEST_SCALAR1 = "telemetry.test.builtin_dynamic";
  const TEST_SCALAR2 = "telemetry.test.builtin_dynamic_other";
  Telemetry.scalarSet(TEST_SCALAR1, 3785);
  Telemetry.scalarSet(TEST_SCALAR2, true);

  // Check the values we tried to store.
  const scalars =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false).parent;

  // Check that they are serialized to the correct format.
  Assert.equal(typeof(scalars[TEST_SCALAR1]), "number",
               TEST_SCALAR1 + " must be serialized to the correct format.");
  Assert.ok(Number.isInteger(scalars[TEST_SCALAR1]),
               TEST_SCALAR1 + " must be a finite integer.");
  Assert.equal(scalars[TEST_SCALAR1], 3785,
               TEST_SCALAR1 + " must have the correct value.");
  Assert.equal(typeof(scalars[TEST_SCALAR2]), "boolean",
               TEST_SCALAR2 + " must be serialized to the correct format.");
  Assert.equal(scalars[TEST_SCALAR2], true,
               TEST_SCALAR2 + " must have the correct value.");

  // Clean up.
  await TelemetryController.testShutdown();
  await OS.File.remove(FILE_PATH);
});

add_task(async function test_keyedDynamicBuiltin() {
  Telemetry.clearScalars();

  // Register the built-in scalars (let's not take the I/O hit).
  Telemetry.registerBuiltinScalars("telemetry.test", {
    "builtin_dynamic_keyed": {
      "kind": Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      "expired": false,
      "record_on_release": false,
      "keyed": true,
    },
  });

  // Store to that scalar.
  const TEST_SCALAR1 = "telemetry.test.builtin_dynamic_keyed";
  Telemetry.keyedScalarSet(TEST_SCALAR1, "test-key", 3785);

  // Check the values we tried to store.
  const scalars =
    Telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false).parent;

  // Check that they are serialized to the correct format.
  Assert.equal(typeof(scalars[TEST_SCALAR1]), "object",
               TEST_SCALAR1 + " must be a keyed scalar.");
  Assert.equal(typeof(scalars[TEST_SCALAR1]["test-key"]), "number",
               TEST_SCALAR1 + " must be serialized to the correct format.");
  Assert.ok(Number.isInteger(scalars[TEST_SCALAR1]["test-key"]),
               TEST_SCALAR1 + " must be a finite integer.");
  Assert.equal(scalars[TEST_SCALAR1]["test-key"], 3785,
               TEST_SCALAR1 + " must have the correct value.");
});
