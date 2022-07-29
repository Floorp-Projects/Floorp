/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { UpdateUtils } = ChromeUtils.import(
  "resource://gre/modules/UpdateUtils.jsm"
);

add_task(async function testUpdateChannelOverride() {
  if (Preferences.has(TelemetryUtils.Preferences.OverrideUpdateChannel)) {
    // If the pref is already set at this point, the test is running in a build
    // that makes use of the override pref. For testing purposes, unset the pref.
    Preferences.set(TelemetryUtils.Preferences.OverrideUpdateChannel, "");
  }

  // Check that we return the same channel as UpdateUtils, by default
  Assert.equal(
    TelemetryUtils.getUpdateChannel(),
    UpdateUtils.getUpdateChannel(false),
    "The telemetry reported channel must match the one from UpdateChannel, by default."
  );

  // Now set the override pref and check that we return the correct channel
  const OVERRIDE_TEST_CHANNEL = "nightly-test";
  Preferences.set(
    TelemetryUtils.Preferences.OverrideUpdateChannel,
    OVERRIDE_TEST_CHANNEL
  );
  Assert.equal(
    TelemetryUtils.getUpdateChannel(),
    OVERRIDE_TEST_CHANNEL,
    "The telemetry reported channel must match the override pref when pref is set."
  );
});
