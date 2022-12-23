/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { OsEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/OsEnvironment.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

// Valid values that telemetry might report. Except for "Error" and
// "NoSuchFeature", these are also the values that we read from the registry.
var APP_SOURCE_ANYWHERE = "Anywhere";
var APP_SOURCE_RECOMMENDATIONS = "Recommendations";
var APP_SOURCE_PREFER_STORE = "PreferStore";
var APP_SOURCE_STORE_ONLY = "StoreOnly";
var APP_SOURCE_NO_SUCH_FEATURE = "NoSuchFeature";
var APP_SOURCE_COLLECTION_ERROR = "Error";

// This variable will store the value that we will fake reading from the
// registry (i.e. the value that would normally be read from
// SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\AicEnabled"). A value
// of undefined represents the registry value not existing.
var gRegistryData;
// If this is set to true, we will throw an exception rather than providing a
// value when Policy.getAllowedAppSources is called.
var gErrorReadingRegistryValue = false;
// This holds the value that Policy.windowsVersionHasAppSourcesFeature will
// return, to allow us to simulate detection of operating system versions
// without the App Sources feature.
var gHaveAppSourcesFeature = true;
// If this is set to true, we will throw an exception rather than providing a
// value when Policy.windowsVersionHasAppSourcesFeature is called.
var gErrorDetectingAppSourcesFeature = false;

function setup() {
  // Allow the test to run successfully for all products whether they have the
  // probe or not.
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  // Mock up the function that gets the allowed app sources so we don't actually
  // have to change OS settings to test.
  OsEnvironment.Policy.getAllowedAppSources = () => {
    if (gErrorReadingRegistryValue) {
      throw new Error("Arbitrary Testing Error");
    }
    // To mimic the normal functionality here, we want to return undefined if
    // the registry value doesn't exist. But that is already how we are
    // representing that state in gRegistryData. So no conversion is necessary.
    return gRegistryData;
  };
  OsEnvironment.Policy.windowsVersionHasAppSourcesFeature = () => {
    if (gErrorDetectingAppSourcesFeature) {
      throw new Error("Arbitrary Testing Error");
    }
    return gHaveAppSourcesFeature;
  };
}

function runTest(
  {
    registryData = "",
    registryValueExists = true,
    registryReadError = false,
    osHasAppSourcesFeature = true,
    errorDetectingAppSourcesFeature = false,
  },
  expectedScalarValue
) {
  if (registryValueExists) {
    gRegistryData = registryData;
  } else {
    gRegistryData = undefined;
  }
  gErrorReadingRegistryValue = registryReadError;
  gHaveAppSourcesFeature = osHasAppSourcesFeature;
  gErrorDetectingAppSourcesFeature = errorDetectingAppSourcesFeature;

  OsEnvironment.reportAllowedAppSources();

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("parent"),
    "os.environment.allowed_app_sources",
    expectedScalarValue,
    "The allowed app sources reported should match the expected sources"
  );
}

add_task(async function testAppSources() {
  setup();

  runTest({ registryData: APP_SOURCE_ANYWHERE }, APP_SOURCE_ANYWHERE);
  runTest(
    { registryData: APP_SOURCE_RECOMMENDATIONS },
    APP_SOURCE_RECOMMENDATIONS
  );
  runTest({ registryData: APP_SOURCE_PREFER_STORE }, APP_SOURCE_PREFER_STORE);
  runTest({ registryData: APP_SOURCE_STORE_ONLY }, APP_SOURCE_STORE_ONLY);
  runTest({ registryValueExists: false }, APP_SOURCE_ANYWHERE);
  runTest({ registryReadError: true }, APP_SOURCE_COLLECTION_ERROR);
  runTest({ registryData: "UnexpectedValue" }, APP_SOURCE_COLLECTION_ERROR);
  runTest({ osHasAppSourcesFeature: false }, APP_SOURCE_NO_SUCH_FEATURE);
  runTest(
    { errorDetectingAppSourcesFeature: true },
    APP_SOURCE_COLLECTION_ERROR
  );
});
