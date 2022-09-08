/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { Checker } = ChromeUtils.import(
  "resource://gre/modules/UpdateService.jsm"
);
const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

async function runBackgroundTask(commandLine) {
  let filePath = commandLine.getArgument(0);
  await EnterprisePolicyTesting.setupPolicyEngineWithJson(filePath);

  let checker = new Checker();
  let actual = await checker.getUpdateURL();
  let expected = commandLine.getArgument(1);

  // 0, 1, 2, 3 are all meaningful exit codes already.
  let exitCode = expected == actual ? 0 : 4;
  console.error(
    `runBackgroundTask: policies read AppUpdateURL '${actual}',
     expected '${expected}', exiting with exitCode ${exitCode}`
  );

  return exitCode;
}
