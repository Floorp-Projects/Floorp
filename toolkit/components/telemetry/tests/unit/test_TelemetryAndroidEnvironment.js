/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* Android-only TelemetryEnvironment xpcshell test that ensures that the device data is stored in the Environment.
 */

const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);

/**
 * Check that a value is a string and not empty.
 *
 * @param aValue The variable to check.
 * @return True if |aValue| has type "string" and is not empty, False otherwise.
 */
function checkString(aValue) {
  return typeof aValue == "string" && aValue != "";
}

/**
 * If value is non-null, check if it's a valid string.
 *
 * @param aValue The variable to check.
 * @return True if it's null or a valid string, false if it's non-null and an invalid
 *         string.
 */
function checkNullOrString(aValue) {
  if (aValue) {
    return checkString(aValue);
  } else if (aValue === null) {
    return true;
  }

  return false;
}

/**
 * If value is non-null, check if it's a boolean.
 *
 * @param aValue The variable to check.
 * @return True if it's null or a valid boolean, false if it's non-null and an invalid
 *         boolean.
 */
function checkNullOrBool(aValue) {
  return aValue === null || typeof aValue == "boolean";
}

function checkSystemSection(data) {
  Assert.ok("system" in data, "There must be a system section in Environment.");
  // Device data is only available on Android.
  if (gIsAndroid) {
    let deviceData = data.system.device;
    Assert.ok(checkNullOrString(deviceData.model));
    Assert.ok(checkNullOrString(deviceData.manufacturer));
    Assert.ok(checkNullOrString(deviceData.hardware));
    Assert.ok(checkNullOrBool(deviceData.isTablet));
  }
}

add_task(async function test_systemEnvironment() {
  let environmentData = TelemetryEnvironment.currentEnvironment;
  checkSystemSection(environmentData);
});
