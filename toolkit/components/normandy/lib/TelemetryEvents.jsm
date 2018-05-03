/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["TelemetryEvents"];

const TELEMETRY_CATEGORY = "normandy";

const TelemetryEvents = {
  eventSchema: {
    enroll: {
      methods: ["enroll"],
      objects: ["preference_study", "addon_study", "preference_rollout"],
      extra_keys: ["experimentType", "branch", "addonId", "addonVersion"],
      record_on_release: true,
    },

    enroll_failed: {
      methods: ["enrollFailed"],
      objects: ["addon_study", "preference_rollout"],
      extra_keys: ["reason", "preference"],
      record_on_release: true,
    },

    update: {
      methods: ["update"],
      objects: ["preference_rollout"],
      extra_keys: ["previousState"],
      record_on_release: true,
    },

    unenroll: {
      methods: ["unenroll"],
      objects: ["preference_study", "addon_study", "preference_rollout"],
      extra_keys: ["reason", "didResetValue", "addonId", "addonVersion"],
      record_on_release: true,
    },

    unenroll_failed: {
      methods: ["unenrollFailed"],
      objects: ["preference_rollout"],
      extra_keys: ["reason"],
      record_on_release: true,
    },

    graduate: {
      methods: ["graduate"],
      objects: ["preference_rollout"],
      extra_keys: [],
      record_on_release: true,
    },
  },

  init() {
    Services.telemetry.registerEvents(TELEMETRY_CATEGORY, this.eventSchema);
  },

  sendEvent(method, object, value, extra) {
    Services.telemetry.recordEvent(TELEMETRY_CATEGORY, method, object, value, extra);
  },
};
