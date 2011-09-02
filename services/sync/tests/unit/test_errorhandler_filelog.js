/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/log4moz.js");

const logsdir = FileUtils.getDir("ProfD", ["weave", "logs"], true);
const LOG_PREFIX_SUCCESS = "success-";
const LOG_PREFIX_ERROR   = "error-";

const PROLONGED_ERROR_DURATION =
  (Svc.Prefs.get('errorhandler.networkFailureReportTimeout') * 2) * 1000;

function setLastSync(lastSyncValue) {
  Svc.Prefs.set("lastSync", (new Date(Date.now() -
    lastSyncValue)).toString());
}

function run_test() {
  run_next_test();
}

add_test(function test_noOutput() {
  // Clear log output from startup.
  Svc.Prefs.set("log.appender.file.logOnSuccess", false);
  Svc.Obs.notify("weave:service:sync:finish");

  // Clear again without having issued any output.
  Svc.Prefs.set("log.appender.file.logOnSuccess", true);

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

    Svc.Prefs.resetBranch("");
    run_next_test();
  });

  // Fake a successful sync.
  Svc.Obs.notify("weave:service:sync:finish");
});

add_test(function test_logOnSuccess_false() {
  Svc.Prefs.set("log.appender.file.logOnSuccess", false);

  let log = Log4Moz.repository.getLogger("Sync.Test.FileLog");
  log.info("this won't show up");

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);
    // No log file was written.
    do_check_false(logsdir.directoryEntries.hasMoreElements());

    Svc.Prefs.resetBranch("");
    run_next_test();
  });

  // Fake a successful sync.
  Svc.Obs.notify("weave:service:sync:finish");
});

function readFile(file, callback) {
  NetUtil.asyncFetch(file, function (inputStream, statusCode, request) {
    let data = NetUtil.readInputStreamToString(inputStream,
                                               inputStream.available());
    callback(statusCode, data);
  });
}

add_test(function test_logOnSuccess_true() {
  Svc.Prefs.set("log.appender.file.logOnSuccess", true);

  let log = Log4Moz.repository.getLogger("Sync.Test.FileLog");
  const MESSAGE = "this WILL show up";
  log.info(MESSAGE);

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

    // Exactly one log file was written.
    let entries = logsdir.directoryEntries;
    do_check_true(entries.hasMoreElements());
    let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
    do_check_eq(logfile.leafName.slice(-4), ".txt");
    do_check_eq(logfile.leafName.slice(0, LOG_PREFIX_SUCCESS.length),
                LOG_PREFIX_SUCCESS);
    do_check_false(entries.hasMoreElements());

    // Ensure the log message was actually written to file.
    readFile(logfile, function (error, data) {
      do_check_true(Components.isSuccessCode(error));
      do_check_neq(data.indexOf(MESSAGE), -1);

      // Clean up.
      try {
        logfile.remove(false);
      } catch(ex) {
        dump("Couldn't delete file: " + ex + "\n");
        // Stupid Windows box.
      }

      Svc.Prefs.resetBranch("");
      run_next_test();
    });
  });

  // Fake a successful sync.
  Svc.Obs.notify("weave:service:sync:finish");
});

add_test(function test_sync_error_logOnError_false() {
  Svc.Prefs.set("log.appender.file.logOnError", false);

  let log = Log4Moz.repository.getLogger("Sync.Test.FileLog");
  log.info("this won't show up");

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);
    // No log file was written.
    do_check_false(logsdir.directoryEntries.hasMoreElements());

    Svc.Prefs.resetBranch("");
    run_next_test();
  });

  // Fake an unsuccessful sync due to prolonged failure.
  setLastSync(PROLONGED_ERROR_DURATION);
  Svc.Obs.notify("weave:service:sync:error");
});

add_test(function test_sync_error_logOnError_true() {
  Svc.Prefs.set("log.appender.file.logOnError", true);

  let log = Log4Moz.repository.getLogger("Sync.Test.FileLog");
  const MESSAGE = "this WILL show up";
  log.info(MESSAGE);

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

    // Exactly one log file was written.
    let entries = logsdir.directoryEntries;
    do_check_true(entries.hasMoreElements());
    let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
    do_check_eq(logfile.leafName.slice(-4), ".txt");
    do_check_eq(logfile.leafName.slice(0, LOG_PREFIX_ERROR.length),
                LOG_PREFIX_ERROR);
    do_check_false(entries.hasMoreElements());

    // Ensure the log message was actually written to file.
    readFile(logfile, function (error, data) {
      do_check_true(Components.isSuccessCode(error));
      do_check_neq(data.indexOf(MESSAGE), -1);

      // Clean up.
      try {
        logfile.remove(false);
      } catch(ex) {
        dump("Couldn't delete file: " + ex + "\n");
        // Stupid Windows box.
      }

      Svc.Prefs.resetBranch("");
      run_next_test();
    });
  });

  // Fake an unsuccessful sync due to prolonged failure.
  setLastSync(PROLONGED_ERROR_DURATION);
  Svc.Obs.notify("weave:service:sync:error");
});

add_test(function test_login_error_logOnError_false() {
  Svc.Prefs.set("log.appender.file.logOnError", false);

  let log = Log4Moz.repository.getLogger("Sync.Test.FileLog");
  log.info("this won't show up");

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);
    // No log file was written.
    do_check_false(logsdir.directoryEntries.hasMoreElements());

    Svc.Prefs.resetBranch("");
    run_next_test();
  });

  // Fake an unsuccessful login due to prolonged failure.
  setLastSync(PROLONGED_ERROR_DURATION);
  Svc.Obs.notify("weave:service:login:error");
});

add_test(function test_login_error_logOnError_true() {
  Svc.Prefs.set("log.appender.file.logOnError", true);

  let log = Log4Moz.repository.getLogger("Sync.Test.FileLog");
  const MESSAGE = "this WILL show up";
  log.info(MESSAGE);

  Svc.Obs.add("weave:service:reset-file-log", function onResetFileLog() {
    Svc.Obs.remove("weave:service:reset-file-log", onResetFileLog);

    // Exactly one log file was written.
    let entries = logsdir.directoryEntries;
    do_check_true(entries.hasMoreElements());
    let logfile = entries.getNext().QueryInterface(Ci.nsILocalFile);
    do_check_eq(logfile.leafName.slice(-4), ".txt");
    do_check_eq(logfile.leafName.slice(0, LOG_PREFIX_ERROR.length),
                LOG_PREFIX_ERROR);
    do_check_false(entries.hasMoreElements());

    // Ensure the log message was actually written to file.
    readFile(logfile, function (error, data) {
      do_check_true(Components.isSuccessCode(error));
      do_check_neq(data.indexOf(MESSAGE), -1);

      // Clean up.
      try {
        logfile.remove(false);
      } catch(ex) {
        dump("Couldn't delete file: " + ex + "\n");
        // Stupid Windows box.
      }

      Svc.Prefs.resetBranch("");
      run_next_test();
    });
  });

  // Fake an unsuccessful login due to prolonged failure.
  setLastSync(PROLONGED_ERROR_DURATION);
  Svc.Obs.notify("weave:service:login:error");
});
