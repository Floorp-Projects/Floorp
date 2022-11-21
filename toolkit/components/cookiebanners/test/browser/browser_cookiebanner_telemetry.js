/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_DETECT_ONLY,
  MODE_UNSET,
} = Ci.nsICookieBannerService;

const TEST_MODES = [
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_DETECT_ONLY,
  MODE_UNSET, // Should be recorded as invalid.
  99, // Invalid
  -1, // Invalid
];

function convertModeToTelemetryString(mode) {
  switch (mode) {
    case MODE_DISABLED:
      return "disabled";
    case MODE_REJECT:
      return "reject";
    case MODE_REJECT_OR_ACCEPT:
      return "reject_or_accept";
    case MODE_DETECT_ONLY:
      return "detect_only";
  }

  return "invalid";
}

add_setup(function() {
  // Clear telemetry before starting telemetry test.
  Services.fog.testResetFOG();
});

add_task(async function test_service_mode_telemetry() {
  let service = Cc["@mozilla.org/cookie-banner-service;1"].getService(
    Ci.nsIObserver
  );

  for (let mode of TEST_MODES) {
    for (let modePBM of TEST_MODES) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["cookiebanners.service.mode", mode],
          ["cookiebanners.service.mode.privateBrowsing", modePBM],
        ],
      });

      // Trigger the idle-daily on the cookie banner service.
      service.observe(null, "idle-daily", null);

      // Verify the telemetry value.
      for (let label of [
        "disabled",
        "reject",
        "reject_or_accept",
        "detect_only",
        "invalid",
      ]) {
        let expected = convertModeToTelemetryString(mode) == label;
        let expectedPBM = convertModeToTelemetryString(modePBM) == label;

        is(
          Glean.cookieBanners.normalWindowServiceMode[label].testGetValue(),
          expected,
          `Has set label ${label} to ${expected} for mode ${mode}.`
        );
        is(
          Glean.cookieBanners.privateWindowServiceMode[label].testGetValue(),
          expectedPBM,
          `Has set label '${label}' to ${expected} for mode ${modePBM}.`
        );
      }
    }
  }
});
