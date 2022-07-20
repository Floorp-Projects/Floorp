/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

const { ASRouterTargeting } = ChromeUtils.import(
  "resource://activity-stream/lib/ASRouterTargeting.jsm"
);

// Background tasks are "live" with a temporary profile that doesn't map common
// network preferences to https://mochi.test in the way that regular testing
// profiles do.  Therefore, certain targeting getters will fail due to non-local
// network connections.  Exclude these.
const EXCLUDED_NAMES = [
  "region", // Queries Mozilla Location Services.
  "needsUpdate", // Queries Balrog update server.
];

/**
 * Return 0 (success) if all targeting getters succeed, 11 (failure)
 * otherwise.
 */
async function runBackgroundTask(commandLine) {
  let exitCode = EXIT_CODE.SUCCESS;

  // Can't use `ASRouterTargeting.getEnvironmentSnapshot`, since that
  // ignores errors, and this is testing that every getter succeeds.
  let target = ASRouterTargeting.Environment;
  let environment = {};
  for (let name of Object.keys(target)) {
    if (EXCLUDED_NAMES.includes(name)) {
      continue;
    }

    try {
      console.debug(`Fetching property ${name}`);
      environment[name] = await target[name];
    } catch (e) {
      exitCode = 11;
      console.error(`Caught exception for property ${name}:`, e);
    }
  }

  console.log(`ASRouterTargeting.Environment:`, environment);

  console.error(`runBackgroundTask: exiting with status ${exitCode}`);

  return exitCode;
}
