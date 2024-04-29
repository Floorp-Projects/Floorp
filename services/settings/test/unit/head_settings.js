/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../common/tests/unit/head_global.js */
/* import-globals-from ../../../common/tests/unit/head_helpers.js */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  Database: "resource://services-settings/Database.sys.mjs",
  Policy: "resource://services-common/uptake-telemetry.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  RemoteSettingsClient:
    "resource://services-settings/RemoteSettingsClient.sys.mjs",
  RemoteSettingsWorker:
    "resource://services-settings/RemoteSettingsWorker.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  SharedUtils: "resource://services-settings/SharedUtils.sys.mjs",
  SyncHistory: "resource://services-settings/SyncHistory.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
  UptakeTelemetry: "resource://services-common/uptake-telemetry.sys.mjs",
  Utils: "resource://services-settings/Utils.sys.mjs",
});
