// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// bug 1360493
// Launch the browser a number of times, testing startup hangs.

"use strict";

const Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");


const APP_TIMER_TIMEOUT_MS = 1000 * 15;
const TRY_COUNT = 50;


// Sets a group of environment variables, returning the old values.
// newVals AND return value is an array of { key: "", value: "" }
function setEnvironmentVariables(newVals) {
  let oldVals = [];
  let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  for (let i = 0; i < newVals.length; ++i) {
    let key = newVals[i].key;
    let value = newVals[i].value;
    let oldObj = { key };
    if (env.exists(key)) {
      oldObj.value = env.get(key);
    } else {
      oldObj.value = null;
    }

    env.set(key, value);
    oldVals.push(oldObj);
  }
  return oldVals;
}


function getFirefoxExecutableFilename() {
  if (AppConstants.platform === "win") {
      return AppConstants.MOZ_APP_NAME + ".exe";
  }
  return AppConstants.MOZ_APP_NAME;
}


// Returns a nsIFile to the firefox.exe executable file
function getFirefoxExecutableFile() {
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file = Services.dirsvc.get("GreBinD", Ci.nsIFile);

  file.append(getFirefoxExecutableFilename());
  return file;
}


// Takes an executable and arguments, and wraps it in a call to the system shell.
// Technique adapted from \toolkit\mozapps\update\tests\unit_service_updater\xpcshellUtilsAUS.js
// to avoid child process console output polluting the xpcshell log.
// returns { file: (nsIFile), args: [] }
function wrapLaunchInShell(file, args) {
  let ret = { };

  if (AppConstants.platform === "win") {
    ret.file = Services.dirsvc.get("WinD", Ci.nsIFile);
    ret.file.append("System32");
    ret.file.append("cmd.exe");
    ret.args = ["/D", "/Q", "/C", file.path].concat(args).concat([">nul"]);
  } else {
    ret.file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    ret.file.initWithPath("/usr/bin/env");
    ret.args = [file.path].concat(args).concat(["> /dev/null"]);
  }

  Assert.ok(ret.file.exists(), "Executable file should exist: " + ret.file.path);

  return ret;
}


// Needed because process.kill() kills the console, not its child process, firefox.
function terminateFirefox(completion) {
  let executableName = getFirefoxExecutableFilename();
  let file;
  let args;

  if (AppConstants.platform === "win") {
    file = Services.dirsvc.get("WinD", Ci.nsIFile);
    file.append("System32");
    file.append("taskkill.exe");
    args = ["/F", "/IM", executableName];
  } else {
    file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath("/usr/bin/killall");
    args = [executableName];
  }

  info("launching application: " + file.path);
  info("            with args: " + args.join(" "));

  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(file);

  let processObserver = {
    observe: function PO_observe(aSubject, aTopic, aData) {
      info("topic: " + aTopic + ", process exitValue: " + process.exitValue);

      Assert.equal(process.exitValue, 0,
                   "Terminate firefox process exit value should be 0");
      Assert.equal(aTopic, "process-finished",
                   "Terminate firefox observer topic should be " +
                   "process-finished");

      if (completion) {
        completion();
      }
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver])
  };

  process.runAsync(args, args.length, processObserver);

  info("             with pid: " + process.pid);
}


// Launches file with args asynchronously, failing if the process did not
// exit within timeoutMS milliseconds. If a timeout occurs, handler()
// is called.
function launchProcess(file, args, env, timeoutMS, handler, attemptCount) {
  let state = { };

  state.attempt = attemptCount;

  state.processObserver = {
    observe: function PO_observe(aSubject, aTopic, aData) {
      if (!state.appTimer) {
        // the app timer has been canceled; this process has timed out already so don't process further.
        handler(false);
        return;
      }

      info("topic: " + aTopic + ", process exitValue: " + state.process.exitValue);

      info("Restoring environment variables");
      setEnvironmentVariables(state.oldEnv);

      state.appTimer.cancel();
      state.appTimer = null;

      Assert.equal(state.process.exitValue, 0,
                   "the application process exit value should be 0");
      Assert.equal(aTopic, "process-finished",
                   "the application process observer topic should be " +
                   "process-finished");

      handler(true);
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver])
  };

  // The timer callback to kill the process if it takes too long.
  state.appTimerCallback = {
    notify: function TC_notify(aTimer) {
      state.appTimer = null;

      info("Restoring environment variables");
      setEnvironmentVariables(state.oldEnv);

      if (state.process.isRunning) {
        info("attempting to kill process");

        // This will cause the shell process to exit as well, triggering our process observer.
        terminateFirefox(function terminateFirefoxCompletion() {
          Assert.ok(false, "Launch application timer expired");
        });
      }
    },
    QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback])
  };

  info("launching application: " + file.path);
  info("            with args: " + args.join(" "));
  info("     with environment: ");
  for (let i = 0; i < env.length; ++i) {
    info("             " + env[i].key + "=" + env[i].value);
  }

  state.process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  state.process.init(file);

  state.appTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  state.appTimer.initWithCallback(state.appTimerCallback, timeoutMS, Ci.nsITimer.TYPE_ONE_SHOT);

  state.oldEnv = setEnvironmentVariables(env);

  state.process.runAsync(args, args.length, state.processObserver);

  info("             with pid: " + state.process.pid);
}


function run_test() {
  do_test_pending();

  let env = [
    { key: "MOZ_CRASHREPORTER_DISABLE", value: null },
    { key: "MOZ_CRASHREPORTER", value: "1" },
    { key: "MOZ_CRASHREPORTER_NO_REPORT", value: "1" },
    { key: "MOZ_CRASHREPORTER_SHUTDOWN", value: "1" },
    { key: "XPCOM_DEBUG_BREAK", value: "stack-and-abort" }
  ];

  let triesStarted = 1;

  let handler = function launchFirefoxHandler(okToContinue) {
    triesStarted++;
    if ((triesStarted <= TRY_COUNT) && okToContinue) {
      testTry();
    } else {
      do_test_finished();
    }
  };

  let testTry = function testTry() {
    let shell = wrapLaunchInShell(getFirefoxExecutableFile(), ["-no-remote", "-test-launch-without-hang"]);
    info("Try attempt #" + triesStarted);
    launchProcess(shell.file, shell.args, env, APP_TIMER_TIMEOUT_MS, handler, triesStarted);
  };

  testTry();
}

