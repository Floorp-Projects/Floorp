/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Invoke this task like `firefox.exe --backgroundtask message ...`.
//
// This task is complicated because it's meant for manual testing by QA but also
// for automated testing.  We might split these two functions at some point.
//
// First, note that the task takes significant configuration from the command
// line.  This is different than the eventual home for this functionality, the
// background update task, which will take this configuration from the default
// browsing profile.
//
// This task accepts the following command line arguments:
//
// --debug: enable very verbose debug logging.  Note that the `MOZ_LOG`
// environment variables still apply.
//
// --stage: use stage Remote Settings
//   (`https://settings-cdn.stage.mozaws.net/v1`) rather than production
//   (`https://firefox.settings.services.mozilla.com/v1`)
//
// --preview: enable Remote Settings and Experiment previews.
//
// `--url about:studies?...` (as copy-pasted from Experimenter): opt in to
//   `optin_branch` of experiment with `optin_slug` from `optin_collection`.
//
// `--url file:///path/to/recipe.json?optin_branch=...` (as downloaded from
//   Experimenter): opt in to `optin_branch` of given experiment recipe.
//
// `--experiments file:///path/to/recipe.json` (as downloaded from
//   Experimenter): enable given experiment recipe, possibly enrolling into a
//   branch via regular bucketing.
//
// `--targeting-snapshot /path/to/targeting.snapshot.json`: take default profile
//   targeting information from given JSON file.
//
// `--reset-storage`: clear Activity Stream storage, including lifetime limit
//   caps.
//
// The following flags are intended for automated testing.
//
// --sentinel: bracket important output with given sentinel for easy parsing.
// --randomizationId: set Nimbus/Normandy randomization ID for deterministic bucketing.
// --disable-alerts-service: do not show system/OS-level alerts.
// --no-experiments: don't talk to Remote Settings server at all.
// --no-datareporting: set `datareporting.healthreport.uploadEnabled=false` in
//   the background task profile.
// --no-optoutstudies: set `app.shield.optoutstudies.enabled=false` in the
//   background task profile.

"use strict";

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

const { ASRouter } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouter.jsm"
);
const { BackgroundTasksUtils } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ClientEnvironmentBase:
    "resource://gre/modules/components-utils/ClientEnvironment.jsm",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
  IndexedDB: "resource://gre/modules/IndexedDB.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  RemoteSettingsClient: "resource://services-settings/RemoteSettingsClient.jsm",
  ToastNotification: "resource://activity-stream/lib/ToastNotification.jsm",
  Utils: "resource://services-settings/Utils.jsm",
});

const SERVER_STAGE = "https://settings-cdn.stage.mozaws.net/v1";

// Default profile targeting snapshot.
let defaultProfileTargetingSnapshot = {};

// Bracket important output with given sentinel for easy parsing.
let outputInfo;
outputInfo = (sentinel, info) => {
  dump(`${sentinel}${JSON.stringify(info)}${sentinel}\n`);
};

async function handleCommandLine(commandLine) {
  const CASE_INSENSITIVE = false;

  // Output data over stdout for tests to consume and inspect.
  let sentinel = commandLine.handleFlagWithParam("sentinel", CASE_INSENSITIVE);
  outputInfo = outputInfo.bind(null, sentinel || "");

  // We always want `nimbus.debug=true` for `about:studies?...` URLs.
  Services.prefs.setBoolPref("nimbus.debug", true);

  // Maybe drive up logging for this testing task.
  Services.prefs.clearUserPref("services.settings.preview_enabled");
  Services.prefs.clearUserPref(
    "browser.newtabpage.activity-stream.asrouter.debugLogLevel"
  );
  Services.prefs.clearUserPref("messaging-system.log");
  Services.prefs.clearUserPref("services.settings.loglevel");
  Services.prefs.clearUserPref("toolkit.backgroundtasks.loglevel");
  if (commandLine.handleFlag("debug", CASE_INSENSITIVE)) {
    console.log("Saw --debug, making logging verbose");
    Services.prefs.setBoolPref("services.settings.preview_enabled", true);
    Services.prefs.setCharPref(
      "browser.newtabpage.activity-stream.asrouter.debugLogLevel",
      "debug"
    );
    Services.prefs.setCharPref("messaging-system.log", "debug");
    Services.prefs.setCharPref("services.settings.loglevel", "debug");
    Services.prefs.setCharPref("toolkit.backgroundtasks.loglevel", "debug");
  }

  // Always make alert service display message when showing an alert.
  // Optionally suppress actually showing OS-level alerts.
  let origAlertsService = lazy.ToastNotification.AlertsService;
  let disableAlertsService = commandLine.handleFlag(
    "disable-alerts-service",
    CASE_INSENSITIVE
  );
  if (disableAlertsService) {
    console.log("Saw --disable-alerts-service, not showing any alerts");
  }
  // Remove getter so that we can redefine property.
  delete lazy.ToastNotification.AlertsService;
  lazy.ToastNotification.AlertsService = {
    showAlert(...args) {
      outputInfo({ showAlert: { args } });
      if (!disableAlertsService) {
        origAlertsService.showAlert(...args);
      }
    },
  };

  let targetingSnapshotPath = commandLine.handleFlagWithParam(
    "targeting-snapshot",
    CASE_INSENSITIVE
  );
  if (targetingSnapshotPath) {
    defaultProfileTargetingSnapshot = await IOUtils.readJSON(
      targetingSnapshotPath
    );
    console.log(
      `Saw --targeting-snapshot, read snapshot from ${targetingSnapshotPath}`
    );
  }
  outputInfo({ defaultProfileTargetingSnapshot });

  lazy.RemoteSettings.enablePreviewMode(false);
  Services.prefs.clearUserPref(
    "messaging-system.rsexperimentloader.collection_id"
  );
  if (commandLine.handleFlag("preview", CASE_INSENSITIVE)) {
    console.log(
      `Saw --preview, invoking 'RemoteSettings.enablePreviewMode(true)' and ` +
        `setting 'messaging-system.rsexperimentloader.collection_id=\"nimbus-preview\"'`
    );
    lazy.RemoteSettings.enablePreviewMode(true);
    Services.prefs.setCharPref(
      "messaging-system.rsexperimentloader.collection_id",
      "nimbus-preview"
    );
  }

  Services.prefs.clearUserPref("services.settings.server");
  Services.prefs.clearUserPref("services.settings.load_dump");
  if (commandLine.handleFlag("stage", CASE_INSENSITIVE)) {
    console.log(
      `Saw --stage, setting 'services.settings.server="https://settings-cdn.stage.mozaws.net/v1"'`
    );
    Services.prefs.setCharPref("services.settings.server", SERVER_STAGE);
    Services.prefs.setBoolPref("services.settings.load_dump", false);

    if (lazy.Utils.SERVER_URL !== SERVER_STAGE) {
      throw new Error(
        "Pref services.settings.server ignored!" +
          "remember to set MOZ_REMOTE_SETTINGS_DEVTOOLS=1 in beta and release."
      );
    }
  }

  // Allow to override Nimbus randomization ID with `--randomizationId ...`.
  let randomizationId = commandLine.handleFlagWithParam(
    "randomizationId",
    CASE_INSENSITIVE
  );
  if (randomizationId) {
    console.log(`Saw --randomizationId: ${randomizationId}`);
    Services.prefs.setCharPref("app.normandy.user_id", randomizationId);
  }
  outputInfo({ randomizationId: lazy.ClientEnvironmentBase.randomizationId });

  // Allow to override Nimbus experiments with `--experiments /path/to/data.json`.
  let experiments = commandLine.handleFlagWithParam(
    "experiments",
    CASE_INSENSITIVE
  );
  if (experiments) {
    let experimentsPath = commandLine.resolveFile(experiments).path;
    let data = await IOUtils.readJSON(experimentsPath);
    if (!Array.isArray(data)) {
      if (data.permissions) {
        data = data.data;
      }
      data = [data];
    }

    lazy.RemoteSettingsClient.prototype.get = async () => {
      return data;
    };

    console.log(`Saw --experiments, read recipes from ${experimentsPath}`);
  }

  // Allow to turn off querying Remote Settings entirely, for tests.
  if (
    !experiments &&
    commandLine.handleFlag("no-experiments", CASE_INSENSITIVE)
  ) {
    lazy.RemoteSettingsClient.prototype.get = async () => {
      return [];
    };
    console.log(`Saw --no-experiments, returning [] recipes`);
  }

  // Allow to test various red buttons.
  Services.prefs.clearUserPref("datareporting.healthreport.uploadEnabled");
  if (commandLine.handleFlag("no-datareporting", CASE_INSENSITIVE)) {
    Services.prefs.setBoolPref(
      "datareporting.healthreport.uploadEnabled",
      false
    );
    console.log(
      `Saw --no-datareporting, set 'datareporting.healthreport.uploadEnabled=false'`
    );
  }

  Services.prefs.clearUserPref("app.shield.optoutstudies.enabled");
  if (commandLine.handleFlag("no-optoutstudies", CASE_INSENSITIVE)) {
    Services.prefs.setBoolPref("app.shield.optoutstudies.enabled", false);
    console.log(
      `Saw --no-datareporting, set 'app.shield.optoutstudies.enabled=false'`
    );
  }

  outputInfo({
    taskProfilePrefs: {
      "app.shield.optoutstudies.enabled": Services.prefs.getBoolPref(
        "app.shield.optoutstudies.enabled"
      ),
      "datareporting.healthreport.uploadEnabled": Services.prefs.getBoolPref(
        "datareporting.healthreport.uploadEnabled"
      ),
    },
  });

  if (commandLine.handleFlag("reset-storage", CASE_INSENSITIVE)) {
    console.log("Saw --reset-storage, deleting database 'ActivityStream'");
    console.log(
      `To hard reset, remove the contents of '${PathUtils.profileDir}'`
    );
    await lazy.IndexedDB.deleteDatabase("ActivityStream");
  }
}

async function runBackgroundTask(commandLine) {
  console.error("runBackgroundTask: message");

  // Most of the task is arranging configuration.
  await handleCommandLine(commandLine);

  // Here's where we actually start Nimbus and the Firefox Messaging
  // System.
  await BackgroundTasksUtils.enableNimbus(
    commandLine,
    defaultProfileTargetingSnapshot.environment
  );

  await BackgroundTasksUtils.enableFirefoxMessagingSystem(
    defaultProfileTargetingSnapshot.environment
  );

  // At the time of writing, toast notifications are torn down as the
  // process exits.  Give the user a chance to see the notification.
  await lazy.ExtensionUtils.promiseTimeout(1000);

  // Everything in `ASRouter` is asynchronous, so we need to give everything a
  // chance to complete.
  outputInfo({ ASRouterState: ASRouter.state });

  return EXIT_CODE.SUCCESS;
}
