/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// An xpcshell script to create a test database.  Useful for creating
// the test-env-32 and test-env-64 databases that we use to test migration
// of databases across architecture changes.
//
// To create a test database, simply run this script using xpcshell:
//
//   path/to/xpcshell path/to/make-test-env.js
//
// The script will create the test-env-32 or test-env-64 directory
// (depending on the current architecture) in the current working directory,
// create a database called "db" within it, and populate the database
// with sample data.
//
// Note: you don't necessarily need to run this script on every architecture
// for which you'd like to create a database.  Once you have a database for one
// architecture, you can use the mdb_dump and mdb_load utilities (if available
// for your systems) to create them for others.  To do so, first dump the data
// on the original architecture:
//
//   mdb_dump -s db path/to/original/test-env-dir > path/to/dump.txt
//
// Then load the data on the new architecture:
//
//   mkdir path/to/new/test-env-dir
//   mdb_load -s db path/to/dump.txt

"use strict";

const { KeyValueService } = ChromeUtils.import(
  "resource://gre/modules/kvstore.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

(async function() {
  const currentDir = await OS.File.getCurrentDirectory();
  const testEnvDir = Services.appinfo.is64Bit ? "test-env-64" : "test-env-32";
  const testEnvPath = OS.Path.join(currentDir, testEnvDir);
  await OS.File.makeDir(testEnvPath, { from: currentDir });

  const database = await KeyValueService.getOrCreate(testEnvPath, "db");
  await database.put("int-key", 1234);
  await database.put("double-key", 56.78);
  await database.put("string-key", "Héllo, wőrld!");
  await database.put("bool-key", true);

  scriptDone = true;
})();

// Do async processing until the async function call completes.
// From <https://developer.mozilla.org/en/XPConnect/xpcshell/HOWTO>
let scriptDone = false;
const mainThread = Services.tm.currentThread;
while (!scriptDone) {
  mainThread.processNextEvent(true);
}
while (mainThread.hasPendingEvents()) {
  mainThread.processNextEvent(true);
}
