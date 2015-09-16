var {OS} = Components.utils.import("resource://gre/modules/osfile.jsm", {});
var {Services} = Components.utils.import("resource://gre/modules/Services.jsm", {});

/**
 * Test optional duration reporting that can be used for telemetry.
 */
add_task(function* duration() {
  Services.prefs.setBoolPref("toolkit.osfile.log", true);
  // Options structure passed to a OS.File copy method.
  let copyOptions = {
    // This field should be overridden with the actual duration
    // measurement.
    outExecutionDuration: null
  };
  let currentDir = yield OS.File.getCurrentDirectory();
  let pathSource = OS.Path.join(currentDir, "test_duration.js");
  let copyFile = pathSource + ".bak";
  function testOptions(options, name) {
    do_print("Checking outExecutionDuration for operation: " + name);
    do_print(name + ": Gathered method duration time: " +
      options.outExecutionDuration + "ms");
    // Making sure that duration was updated.
    do_check_eq(typeof options.outExecutionDuration, "number");
    do_check_true(options.outExecutionDuration >= 0);
  };
  // Testing duration of OS.File.copy.
  yield OS.File.copy(pathSource, copyFile, copyOptions);
  testOptions(copyOptions, "OS.File.copy");
  yield OS.File.remove(copyFile);

  // Trying an operation where options are cloned.
  let pathDest = OS.Path.join(OS.Constants.Path.tmpDir,
    "osfile async test read writeAtomic.tmp");
  let tmpPath = pathDest + ".tmp";
  let readOptions = {
    outExecutionDuration: null
  };
  let contents = yield OS.File.read(pathSource, undefined, readOptions);
  testOptions(readOptions, "OS.File.read");
  // Options structure passed to a OS.File writeAtomic method.
  let writeAtomicOptions = {
    // This field should be first initialized with the actual
    // duration measurement then progressively incremented.
    outExecutionDuration: null,
    tmpPath: tmpPath
  };
  yield OS.File.writeAtomic(pathDest, contents, writeAtomicOptions);
  testOptions(writeAtomicOptions, "OS.File.writeAtomic");
  yield OS.File.remove(pathDest);

  do_print("Ensuring that we can use outExecutionDuration to accumulate durations");

  let ARBITRARY_BASE_DURATION = 5;
  copyOptions = {
    // This field should now be incremented with the actual duration
    // measurement.
    outExecutionDuration: ARBITRARY_BASE_DURATION
  };
  let backupDuration = ARBITRARY_BASE_DURATION;
  // Testing duration of OS.File.copy.
  yield OS.File.copy(pathSource, copyFile, copyOptions);

  do_check_true(copyOptions.outExecutionDuration >= backupDuration);

  backupDuration = copyOptions.outExecutionDuration;
  yield OS.File.remove(copyFile, copyOptions);
  do_check_true(copyOptions.outExecutionDuration >= backupDuration);

  // Trying an operation where options are cloned.
  // Options structure passed to a OS.File writeAtomic method.
  writeAtomicOptions = {
    // This field should be overridden with the actual duration
    // measurement.
    outExecutionDuration: copyOptions.outExecutionDuration,
    tmpPath: tmpPath
  };
  backupDuration = writeAtomicOptions.outExecutionDuration;

  yield OS.File.writeAtomic(pathDest, contents, writeAtomicOptions);
  do_check_true(copyOptions.outExecutionDuration >= backupDuration);
  OS.File.remove(pathDest);

  // Testing an operation that doesn't take arguments at all
  let file = yield OS.File.open(pathSource);
  yield file.stat();
  yield file.close();
});

function run_test() {
  run_next_test();
}
