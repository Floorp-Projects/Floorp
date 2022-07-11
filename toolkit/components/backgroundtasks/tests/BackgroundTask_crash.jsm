/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

async function runBackgroundTask(commandLine) {
  // This task depends on `CrashTestUtils.jsm` and requires the
  // sibling `testcrasher` library to be in the current working
  // directory.  Fail right away if we can't find the module or the
  // native library.
  let cwd = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  let protocolHandler = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  var curDirURI = Services.io.newFileURI(cwd);
  protocolHandler.setSubstitution("test", curDirURI);

  const { CrashTestUtils } = ChromeUtils.import(
    "resource://test/CrashTestUtils.jsm"
  );

  // Get the temp dir.
  var env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  var tmpd = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  tmpd.initWithPath(env.get("XPCSHELL_TEST_TEMP_DIR"));

  var crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
    Ci.nsICrashReporter
  );

  // We need to call this or crash events go in an undefined location.
  crashReporter.UpdateCrashEventsDir();

  // Setting the minidump path is not allowed in content processes,
  // but background tasks run in the parent.
  crashReporter.minidumpPath = tmpd;

  // Arguments are [crashType, key1, value1, key2, value2, ...].
  let i = 0;
  let crashType = Number.parseInt(commandLine.getArgument(i));
  i += 1;
  while (i + 1 < commandLine.length) {
    let key = commandLine.getArgument(i);
    let value = commandLine.getArgument(i + 1);
    i += 2;
    crashReporter.annotateCrashReport(key, value);
  }

  console.log(`Crashing with crash type ${crashType}`);

  // Now actually crash.
  CrashTestUtils.crash(crashType);

  // This is moot, since we crashed, but...
  return 1;
}
