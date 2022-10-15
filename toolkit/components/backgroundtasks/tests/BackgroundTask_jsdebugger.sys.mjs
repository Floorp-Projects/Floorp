/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This task is intended to be interrupted by the JS debugger in tests.
 *
 * This task exposes its `exitCode` so that in the future the JS
 * debugger can change its exit code dynamically from a failing exit
 * code to exit code 0.
 */

export function runBackgroundTask(commandLine) {
  // In the future, will be modifed by the JS debugger (to 0, success).
  var exposedExitCode = 0;

  console.error(
    `runBackgroundTask: will exit with exitCode: ${exposedExitCode}`
  );

  return exposedExitCode;
}
