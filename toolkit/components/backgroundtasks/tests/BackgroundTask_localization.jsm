/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { EXIT_CODE } = ChromeUtils.import(
  "resource://gre/modules/BackgroundTasksManager.jsm"
).BackgroundTasksManager;

/**
 * Return 0 (success) if in the given resource file, the given string
 * identifier has the given string value, 11 (failure) otherwise.
 */
async function runBackgroundTask(commandLine) {
  let resource = commandLine.getArgument(0);
  let id = commandLine.getArgument(1);
  let expected = commandLine.getArgument(2);

  let l10n = new Localization([resource]);
  let value = await l10n.formatValue(id);

  let exitCode = value == expected ? EXIT_CODE.SUCCESS : 11;

  console.error(
    `runBackgroundTask: in resource '${resource}': for id '${id}', ` +
      `expected is '${expected}' and value is '${value}'; ` +
      `exiting with status ${exitCode}`
  );

  return exitCode;
}
