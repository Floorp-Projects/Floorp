/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// NOTE: The sync test_errorhandler_* tests have quite good coverage for
// other aspects of this.

Cu.import("resource://services-common/logmanager.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

function run_test() {
  run_next_test();
}

// Returns an array of [consoleAppender, dumpAppender, [fileAppenders]] for
// the specified log.  Note that fileAppenders will usually have length=1
function getAppenders(log) {
  let capps = log.appenders.filter(app => app instanceof Log.ConsoleAppender);
  equal(capps.length, 1, "should only have one console appender");
  let dapps = log.appenders.filter(app => app instanceof Log.DumpAppender);
  equal(dapps.length, 1, "should only have one dump appender");
  let fapps = log.appenders.filter(app => app instanceof Log.StorageStreamAppender);
  return [capps[0], dapps[0], fapps];
}

// Test that the correct thing happens when no prefs exist for the log manager.
add_task(function* test_noPrefs() {
  // tell the log manager to init with a pref branch that doesn't exist.
  let lm = new LogManager("no-such-branch.", ["TestLog"], "test");

  let log = Log.repository.getLogger("TestLog");
  let [capp, dapp, fapps] = getAppenders(log);
  // the "dump" and "console" appenders should get Error level
  equal(capp.level, Log.Level.Error);
  equal(dapp.level, Log.Level.Error);
  // and the file (stream) appender gets Dump by default
  equal(fapps.length, 1, "only 1 file appender");
  equal(fapps[0].level, Log.Level.Debug);
  lm.finalize();
});

// Test that changes to the prefs used by the log manager are updated dynamically.
add_task(function* test_PrefChanges() {
  Services.prefs.setCharPref("log-manager.test.log.appender.console", "Trace");
  Services.prefs.setCharPref("log-manager.test.log.appender.dump", "Trace");
  Services.prefs.setCharPref("log-manager.test.log.appender.file.level", "Trace");
  let lm = new LogManager("log-manager.test.", ["TestLog2"], "test");

  let log = Log.repository.getLogger("TestLog2");
  let [capp, dapp, [fapp]] = getAppenders(log);
  equal(capp.level, Log.Level.Trace);
  equal(dapp.level, Log.Level.Trace);
  equal(fapp.level, Log.Level.Trace);
  // adjust the prefs and they should magically be reflected in the appenders.
  Services.prefs.setCharPref("log-manager.test.log.appender.console", "Debug");
  Services.prefs.setCharPref("log-manager.test.log.appender.dump", "Debug");
  Services.prefs.setCharPref("log-manager.test.log.appender.file.level", "Debug");
  equal(capp.level, Log.Level.Debug);
  equal(dapp.level, Log.Level.Debug);
  equal(fapp.level, Log.Level.Debug);
  // and invalid values should cause them to fallback to their defaults.
  Services.prefs.setCharPref("log-manager.test.log.appender.console", "xxx");
  Services.prefs.setCharPref("log-manager.test.log.appender.dump", "xxx");
  Services.prefs.setCharPref("log-manager.test.log.appender.file.level", "xxx");
  equal(capp.level, Log.Level.Error);
  equal(dapp.level, Log.Level.Error);
  equal(fapp.level, Log.Level.Debug);
  lm.finalize();
});

// Test that the same log used by multiple log managers does the right thing.
add_task(function* test_SharedLogs() {
  // create the prefs for the first instance.
  Services.prefs.setCharPref("log-manager-1.test.log.appender.console", "Trace");
  Services.prefs.setCharPref("log-manager-1.test.log.appender.dump", "Trace");
  Services.prefs.setCharPref("log-manager-1.test.log.appender.file.level", "Trace");
  let lm1 = new LogManager("log-manager-1.test.", ["TestLog3"], "test");

  // and the second.
  Services.prefs.setCharPref("log-manager-2.test.log.appender.console", "Debug");
  Services.prefs.setCharPref("log-manager-2.test.log.appender.dump", "Debug");
  Services.prefs.setCharPref("log-manager-2.test.log.appender.file.level", "Debug");
  let lm2 = new LogManager("log-manager-2.test.", ["TestLog3"], "test");

  let log = Log.repository.getLogger("TestLog3");
  let [capp, dapp, fapps] = getAppenders(log);

  // console and dump appenders should be "trace" as it is more verbose than
  // "debug"
  equal(capp.level, Log.Level.Trace);
  equal(dapp.level, Log.Level.Trace);

  // Set the prefs on the -1 branch to "Error" - it should then end up with
  // "Debug" from the -2 branch.
  Services.prefs.setCharPref("log-manager-1.test.log.appender.console", "Error");
  Services.prefs.setCharPref("log-manager-1.test.log.appender.dump", "Error");
  Services.prefs.setCharPref("log-manager-1.test.log.appender.file.level", "Error");

  equal(capp.level, Log.Level.Debug);
  equal(dapp.level, Log.Level.Debug);

  lm1.finalize();
  lm2.finalize();
});

// A little helper to test what log files exist.  We expect exactly zero (if
// prefix is null) or exactly one with the specified prefix.
function checkLogFile(prefix) {
  let logsdir = FileUtils.getDir("ProfD", ["weave", "logs"], true);
  let entries = logsdir.directoryEntries;
  if (!prefix) {
    // expecting no files.
    ok(!entries.hasMoreElements());
  } else {
    // expecting 1 file.
    ok(entries.hasMoreElements());
    let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
    equal(logfile.leafName.slice(-4), ".txt");
    ok(logfile.leafName.startsWith(prefix + "-test-"), logfile.leafName);
    // and remove it ready for the next check.
    logfile.remove(false);
  }
}

// Test that we correctly write error logs by default
add_task(function* test_logFileErrorDefault() {
  let lm = new LogManager("log-manager.test.", ["TestLog2"], "test");

  let log = Log.repository.getLogger("TestLog2");
  log.error("an error message");
  yield lm.resetFileLog(lm.REASON_ERROR);
  // One error log file exists.
  checkLogFile("error");
  lm.finalize();
});
