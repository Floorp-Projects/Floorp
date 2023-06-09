/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { TelemetryUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryUtils.sys.mjs"
);
const { UpdateUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/UpdateUtils.sys.mjs"
);

add_task(async function testUpdateChannelOverride() {
  if (
    Services.prefs.prefHasDefaultValue(
      TelemetryUtils.Preferences.OverrideUpdateChannel
    ) ||
    Services.prefs.prefHasUserValue(
      TelemetryUtils.Preferences.OverrideUpdateChannel
    )
  ) {
    // If the pref is already set at this point, the test is running in a build
    // that makes use of the override pref. For testing purposes, unset the pref.
    Services.prefs.setStringPref(
      TelemetryUtils.Preferences.OverrideUpdateChannel,
      ""
    );
  }

  // Check that we return the same channel as UpdateUtils, by default
  Assert.equal(
    TelemetryUtils.getUpdateChannel(),
    UpdateUtils.getUpdateChannel(false),
    "The telemetry reported channel must match the one from UpdateChannel, by default."
  );

  // Now set the override pref and check that we return the correct channel
  const OVERRIDE_TEST_CHANNEL = "nightly-test";
  Services.prefs.setStringPref(
    TelemetryUtils.Preferences.OverrideUpdateChannel,
    OVERRIDE_TEST_CHANNEL
  );
  Assert.equal(
    TelemetryUtils.getUpdateChannel(),
    OVERRIDE_TEST_CHANNEL,
    "The telemetry reported channel must match the override pref when pref is set."
  );
});
