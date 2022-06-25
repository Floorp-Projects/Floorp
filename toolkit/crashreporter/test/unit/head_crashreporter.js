var { OS, require } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { makeFakeAppDir } = ChromeUtils.import(
  "resource://testing-common/AppData.jsm"
);
var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

function getEventDir() {
  return OS.Path.join(do_get_tempdir().path, "crash-events");
}

function sendCommandAsync(command) {
  return new Promise(resolve => {
    sendCommand(command, resolve);
  });
}

/*
 * Run an xpcshell subprocess and crash it.
 *
 * @param setup
 *        A string of JavaScript code to execute in the subprocess
 *        before crashing. If this is a function and not a string,
 *        it will have .toSource() called on it, and turned into
 *        a call to itself. (for programmer convenience)
 *        This code will be evaluted between crasher_subprocess_head.js
 *        and crasher_subprocess_tail.js, so it will have access
 *        to everything defined in crasher_subprocess_head.js,
 *        which includes "crashReporter", a variable holding
 *        the crash reporter service.
 *
 * @param callback
 *        A JavaScript function to be called after the subprocess
 *        crashes. It will be passed (minidump, extra, extrafile), where
 *         - minidump is an nsIFile of the minidump file produced,
 *         - extra is an object containing the key,value pairs from
 *           the .extra file.
 *         - extrafile is an nsIFile of the extra file
 *
 * @param canReturnZero
 *       If true, the subprocess may return with a zero exit code.
 *       Certain types of crashes may not cause the process to
 *       exit with an error.
 *
 */
async function do_crash(setup, callback, canReturnZero) {
  // get current process filename (xpcshell)
  let bin = Services.dirsvc.get("XREExeF", Ci.nsIFile);
  if (!bin.exists()) {
    // weird, can't find xpcshell binary?
    do_throw("Can't find xpcshell binary!");
  }
  // get Gre dir (GreD)
  let greD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let headfile = do_get_file("crasher_subprocess_head.js");
  let tailfile = do_get_file("crasher_subprocess_tail.js");
  // run xpcshell -g GreD -f head -e "some setup code" -f tail
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(bin);
  let args = ["-g", greD.path, "-f", headfile.path];
  if (setup) {
    if (typeof setup == "function") {
      // funky, but convenient
      setup = "(" + setup.toSource() + ")();";
    }
    args.push("-e", setup);
  }
  args.push("-f", tailfile.path);

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  let crashD = do_get_tempdir();
  crashD.append("crash-events");
  if (!crashD.exists()) {
    crashD.create(crashD.DIRECTORY_TYPE, 0o700);
  }

  env.set("CRASHES_EVENTS_DIR", crashD.path);

  try {
    process.run(true, args, args.length);
  } catch (ex) {
    // on Windows we exit with a -1 status when crashing.
  } finally {
    env.set("CRASHES_EVENTS_DIR", "");
  }

  if (!canReturnZero) {
    // should exit with an error (should have crashed)
    Assert.notEqual(process.exitValue, 0);
  }

  await handleMinidump(callback);
}

function getMinidump() {
  let en = do_get_tempdir().directoryEntries;
  while (en.hasMoreElements()) {
    let f = en.nextFile;
    if (f.leafName.substr(-4) == ".dmp") {
      return f;
    }
  }

  return null;
}

function getMinidumpAnalyzerPath() {
  const binSuffix = AppConstants.platform === "win" ? ".exe" : "";
  const exeName = "minidump-analyzer" + binSuffix;

  let exe = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  exe.append(exeName);

  return exe;
}

function runMinidumpAnalyzer(dumpFile, additionalArgs) {
  let bin = getMinidumpAnalyzerPath();
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(bin);
  let args = [];
  if (additionalArgs) {
    args = args.concat(additionalArgs);
  }
  args.push(dumpFile.path);
  process.run(true /* blocking */, args, args.length);
}

async function handleMinidump(callback) {
  // find minidump
  let minidump = getMinidump();

  if (minidump == null) {
    do_throw("No minidump found!");
  }

  let extrafile = minidump.clone();
  extrafile.leafName = extrafile.leafName.slice(0, -4) + ".extra";

  let memoryfile = minidump.clone();
  memoryfile.leafName = memoryfile.leafName.slice(0, -4) + ".memory.json.gz";

  let cleanup = function() {
    [minidump, extrafile, memoryfile].forEach(file => {
      if (file.exists()) {
        file.remove(false);
      }
    });
  };

  // Just in case, don't let these files linger.
  registerCleanupFunction(cleanup);

  Assert.ok(extrafile.exists());
  let data = await OS.File.read(extrafile.path);
  let decoder = new TextDecoder();
  let extra = JSON.parse(decoder.decode(data));

  if (callback) {
    await callback(minidump, extra, extrafile, memoryfile);
  }

  cleanup();
}

function spinEventLoop() {
  return new Promise(resolve => {
    executeSoon(resolve);
  });
}

/**
 * Helper for testing a content process crash.
 *
 * This variant accepts a setup function which runs in the content process
 * to set data as needed _before_ the crash.  The tail file triggers a generic
 * crash after setup.
 */
async function do_content_crash(setup, callback) {
  do_load_child_test_harness();

  // Setting the minidump path won't work in the child, so we need to do
  // that here.
  let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
    Ci.nsICrashReporter
  );
  crashReporter.minidumpPath = do_get_tempdir();

  /* import-globals-from ../unit/crasher_subprocess_head.js */
  /* import-globals-from ../unit/crasher_subprocess_tail.js */

  let headfile = do_get_file("../unit/crasher_subprocess_head.js");
  let tailfile = do_get_file("../unit/crasher_subprocess_tail.js");
  if (setup) {
    if (typeof setup == "function") {
      // funky, but convenient
      setup = "(" + setup.toSource() + ")();";
    }
  }

  do_get_profile();
  await makeFakeAppDir();
  await sendCommandAsync('load("' + headfile.path.replace(/\\/g, "/") + '");');
  if (setup) {
    await sendCommandAsync(setup);
  }
  await sendCommandAsync('load("' + tailfile.path.replace(/\\/g, "/") + '");');
  await spinEventLoop();

  let minidump = getMinidump();
  let id = minidump.leafName.slice(0, -4);
  await Services.crashmanager.ensureCrashIsPresent(id);
  try {
    await handleMinidump(callback);
  } catch (x) {
    do_report_unexpected_exception(x);
  }
}

/**
 * Helper for testing a content process crash.
 *
 * This variant accepts a trigger function which runs in the content process
 * and does something to _trigger_ the crash.
 */
async function do_triggered_content_crash(trigger, callback) {
  do_load_child_test_harness();

  // Setting the minidump path won't work in the child, so we need to do
  // that here.
  let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
    Ci.nsICrashReporter
  );
  crashReporter.minidumpPath = do_get_tempdir();

  /* import-globals-from ../unit/crasher_subprocess_head.js */

  let headfile = do_get_file("../unit/crasher_subprocess_head.js");
  if (trigger) {
    if (typeof trigger == "function") {
      // funky, but convenient
      trigger = "(" + trigger.toSource() + ")();";
    }
  }

  do_get_profile();
  await makeFakeAppDir();
  await sendCommandAsync('load("' + headfile.path.replace(/\\/g, "/") + '");');
  await sendCommandAsync(trigger);
  await spinEventLoop();
  let id = getMinidump().leafName.slice(0, -4);
  await Services.crashmanager.ensureCrashIsPresent(id);
  try {
    await handleMinidump(callback);
  } catch (x) {
    do_report_unexpected_exception(x);
  }
}

/*
 * Run the `crash` backgroundtask subprocess, crashing it in the
 * specified manner.
 *
 * @param crashType Integer `CrashTestUtils.CRASH_...` code.
 * @param crashExtras Dictionary of key-value pairs to include in
 *                    minidump extras.
 *
 * @param callback
 *        A JavaScript function to be called after the subprocess
 *        crashes. It will be passed (minidump, extra, extrafile), where
 *         - minidump is an nsIFile of the minidump file produced,
 *         - extra is an object containing the key,value pairs from
 *           the .extra file.
 *         - extrafile is an nsIFile of the extra file
 *
 * @param canReturnZero
 *       If true, the subprocess may return with a zero exit code.
 *       Certain types of crashes may not cause the process to
 *       exit with an error.
 *
 */
async function do_backgroundtask_crash(
  crashType,
  crashExtras,
  callback,
  canReturnZero
) {
  Assert.ok(AppConstants.MOZ_BACKGROUNDTASKS);

  // Get full path to application (not xpcshell)
  let bin = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  if (AppConstants.platform === "win") {
    bin.append(AppConstants.MOZ_APP_NAME + ".exe");
  } else {
    bin.append(AppConstants.MOZ_APP_NAME);
  }

  // run `application --backgroundtask crash ...`.
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(bin);

  let args = ["--backgroundtask", "crash"];
  args.push(crashType.toString());

  // Sorted to be deterministic.
  let sorted = Object.entries(crashExtras).sort((a, b) => a[0] < b[0]);
  for (let [key, value] of sorted) {
    args.push(key);
    args.push(value);
  }

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  let crashD = do_get_tempdir();
  crashD.append("crash-events");
  if (!crashD.exists()) {
    crashD.create(crashD.DIRECTORY_TYPE, 0o700);
  }

  env.set("CRASHES_EVENTS_DIR", crashD.path);

  // Ensure `resource://testing-common` gets mapped.
  let protocolHandler = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  let uri = protocolHandler.getSubstitution("testing-common");
  Assert.ok(uri, "resource://testing-common is not substituted");

  // The equivalent of _TESTING_MODULES_DIR in xpcshell.
  env.set("XPCSHELL_TESTING_MODULES_URI", uri.spec);

  try {
    process.run(true, args, args.length);
  } catch (ex) {
    // on Windows we exit with a -1 status when crashing.
  } finally {
    env.set("CRASHES_EVENTS_DIR", "");
    env.set("XPCSHELL_TESTING_MODULES_URI", "");
  }

  if (!canReturnZero) {
    // should exit with an error (should have crashed)
    Assert.notEqual(process.exitValue, 0);
  }

  await handleMinidump(callback);
}

// Import binary APIs via js-ctypes.
var { CrashTestUtils } = ChromeUtils.import(
  "resource://test/CrashTestUtils.jsm"
);
