/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BackgroundTasksManager"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {})
    .ConsoleAPI;
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: "toolkit.backgroundtasks.loglevel",
    prefix: "BackgroundTasksManager",
  };
  return new ConsoleAPI(consoleOptions);
});

// Map resource://testing-common/ to the shared test modules directory.  This is
// a transliteration of `register_modules_protocol_handler` from
// https://searchfox.org/mozilla-central/rev/f081504642a115cb8236bea4d8250e5cb0f39b02/testing/xpcshell/head.js#358-389.
function registerModulesProtocolHandler() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let _TESTING_MODULES_URI = env.get("XPCSHELL_TESTING_MODULES_URI", "");
  if (!_TESTING_MODULES_URI) {
    return false;
  }

  let protocolHandler = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  protocolHandler.setSubstitution(
    "testing-common",
    Services.io.newURI(_TESTING_MODULES_URI)
  );
  // Log loudly so that when testing, we always actually use the
  // console logging mechanism and therefore deterministically load that code.
  log.error(
    `Substitution set: resource://testing-common aliases ${_TESTING_MODULES_URI}`
  );

  return true;
}

/**
 * Find a JSM named like `backgroundtasks/BackgroundTask_${name}.jsm`
 * and return its `runBackgroundTask` function.
 *
 * When testing, allow to load from `XPCSHELL_TESTING_MODULES_URI`,
 * which is registered at `resource://testing-common`, the standard
 * location for test-only modules.
 *
 * @return {function} `runBackgroundTask` function.
 * @throws NS_ERROR_NOT_AVAILABLE if a background task with the given `name` is
 * not found.
 */
function findRunBackgroundTask(name) {
  const subModules = [
    "resource:///modules", // App-specific first.
    "resource://gre/modules", // Toolkit/general second.
  ];

  if (registerModulesProtocolHandler()) {
    subModules.push("resource://testing-common"); // Test-only third.
  }

  for (const subModule of subModules) {
    let URI = `${subModule}/backgroundtasks/BackgroundTask_${name}.jsm`;
    log.debug(`Looking for background task at URI: ${URI}`);

    try {
      const { runBackgroundTask } = ChromeUtils.import(URI);
      log.info(`Found background task at URI: ${URI}`);
      return runBackgroundTask;
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw ex;
      }
    }
  }

  log.warn(`No backgroundtask named '${name}' registered`);
  throw new Components.Exception(
    `No backgroundtask named '${name}' registered`,
    Cr.NS_ERROR_NOT_AVAILABLE
  );
}

var BackgroundTasksManager = {
  async runBackgroundTaskNamed(name, commandLine) {
    function addMarker(markerName) {
      return ChromeUtils.addProfilerMarker(markerName, undefined, name);
    }
    addMarker("BackgroundTasksManager:AfterRunBackgroundTaskNamed");

    log.info(
      `Running background task named '${name}' (with ${commandLine.length} arguments)`
    );

    let exitCode = BackgroundTasksManager.EXIT_CODE.NOT_FOUND;
    try {
      let runBackgroundTask = findRunBackgroundTask(name);
      addMarker("BackgroundTasksManager:AfterFindRunBackgroundTask");

      try {
        // TODO: timeout tasks that run too long.
        exitCode = await runBackgroundTask(commandLine);
        log.info(
          `Backgroundtask named '${name}' completed with exit code ${exitCode}`
        );
      } catch (e) {
        log.error(`Backgroundtask named '${name}' threw exception`, e);
        exitCode = BackgroundTasksManager.EXIT_CODE.EXCEPTION;
      }
    } finally {
      addMarker("BackgroundTasksManager:AfterAwaitRunBackgroundTask");

      log.info(`Invoking Services.startup.quit(..., ${exitCode})`);
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit, exitCode);
    }

    return exitCode;
  },
};

/**
 * Background tasks should standard exit code conventions where 0 denotes
 * success and non-zero denotes failure and/or an error.  In addition, since
 * background tasks have limited channels to communicate with consumers, the
 * special values `NOT_FOUND` (integer 2) and `THREW_EXCEPTION` (integer 3) are
 * distinguished.
 *
 * If you extend this to add background task-specific exit codes, use exit codes
 * greater than 10 to allow for additional shared exit codes to be added here.
 * Exit codes should be between 0 and 127 to be safe across platforms.
 */
BackgroundTasksManager.EXIT_CODE = {
  /**
   * The task succeeded.
   *
   * The `runBackgroundTask(...)` promise resolved to 0.
   */
  SUCCESS: 0,

  /**
   * The task with the specified name could not be found or imported.
   *
   * The corresponding `runBackgroundTask` method could not be found.
   */
  NOT_FOUND: 2,

  /**
   * The task failed with an uncaught exception.
   *
   * The `runBackgroundTask(...)` promise rejected with an exception.
   */
  EXCEPTION: 3,

  /**
   * The last exit code reserved by this structure.  Use codes larger than this
   * code for background task-specific exit codes.
   */
  LAST_RESERVED: 10,
};
