/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_UNSET,
} = Ci.nsICookieBannerService;

const TEST_MODES = [
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_UNSET, // Should be recorded as invalid.
  99, // Invalid
  -1, // Invalid
];

function convertModeToTelemetryString(mode) {
  let str;
  switch (mode) {
    case MODE_DISABLED:
      str = "disabled";
      break;
    case MODE_REJECT:
      str = "reject";
      break;
    case MODE_REJECT_OR_ACCEPT:
      str = "reject_or_accept";
      break;
    default:
      str = "invalid";
  }

  return str;
}

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
      for (let label of ["disabled", "reject", "reject_or_accept", "invalid"]) {
        Assert.equal(
          convertModeToTelemetryString(mode) == label,
          Glean.cookieBanners.normalWindowServiceMode[label].testGetValue()
        );
        Assert.equal(
          convertModeToTelemetryString(modePBM) == label,
          Glean.cookieBanners.privateWindowServiceMode[label].testGetValue()
        );
      }
    }
  }
});
