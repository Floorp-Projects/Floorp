/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export async function runBackgroundTask(commandLine) {
  // This task depends on `CrashTestUtils.sys.mjs` and requires the
  // sibling `testcrasher` library to be in the current working
  // directory.  Fail right away if we can't find the module or the
  // native library.
  let cwd = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  let protocolHandler = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  var curDirURI = Services.io.newFileURI(cwd);
  protocolHandler.setSubstitution("test", curDirURI);

  const { CrashTestUtils } = ChromeUtils.importESModule(
    "resource://test/CrashTestUtils.sys.mjs"
  );

  // Get the temp dir.
  var tmpd = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  tmpd.initWithPath(Services.env.get("XPCSHELL_TEST_TEMP_DIR"));

  // We need to call this or crash events go in an undefined location.
  Services.appinfo.UpdateCrashEventsDir();

  // Setting the minidump path is not allowed in content processes,
  // but background tasks run in the parent.
  Services.appinfo.minidumpPath = tmpd;

  // Arguments are [crashType, key1, value1, key2, value2, ...].
  let i = 0;
  let crashType = Number.parseInt(commandLine.getArgument(i));
  i += 1;
  while (i + 1 < commandLine.length) {
    let key = commandLine.getArgument(i);
    let value = commandLine.getArgument(i + 1);
    i += 2;
    Services.appinfo.annotateCrashReport(key, value);
  }

  console.log(`Crashing with crash type ${crashType}`);

  // Now actually crash.
  CrashTestUtils.crash(crashType);

  // This is moot, since we crashed, but...
  return 1;
}
