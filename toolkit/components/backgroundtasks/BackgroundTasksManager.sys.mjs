/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.sys.mjs for details.
    maxLogLevel: "error",
    maxLogLevelPref: "toolkit.backgroundtasks.loglevel",
    prefix: "BackgroundTasksManager",
  };
  return new ConsoleAPI(consoleOptions);
});

ChromeUtils.defineLazyGetter(lazy, "DevToolsStartup", () => {
  return Cc["@mozilla.org/devtools/startup-clh;1"].getService(
    Ci.nsICommandLineHandler
  ).wrappedJSObject;
});

// The default timing settings can be overriden by the preferences
// toolkit.backgroundtasks.defaultTimeoutSec and
// toolkit.backgroundtasks.defaultMinTaskRuntimeMS for all background tasks
// and individually per module by
// export const backgroundTaskTimeoutSec = X;
// export const backgroundTaskMinRuntimeMS = Y;
let timingSettings = {
  minTaskRuntimeMS: 500,
  maxTaskRuntimeSec: 600, // 10 minutes.
};

// Map resource://testing-common/ to the shared test modules directory.  This is
// a transliteration of `register_modules_protocol_handler` from
// https://searchfox.org/mozilla-central/rev/f081504642a115cb8236bea4d8250e5cb0f39b02/testing/xpcshell/head.js#358-389.
function registerModulesProtocolHandler() {
  let _TESTING_MODULES_URI = Services.env.get(
    "XPCSHELL_TESTING_MODULES_URI",
    ""
  );
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
  lazy.log.error(
    `Substitution set: resource://testing-common aliases ${_TESTING_MODULES_URI}`
  );

  return true;
}

function locationsForBackgroundTaskNamed(name) {
  const subModules = [
    "resource:///modules", // App-specific first.
    "resource://gre/modules", // Toolkit/general second.
  ];

  if (registerModulesProtocolHandler()) {
    subModules.push("resource://testing-common"); // Test-only third.
  }

  let locations = [];
  for (const subModule of subModules) {
    let URI = `${subModule}/backgroundtasks/BackgroundTask_${name}.sys.mjs`;
    locations.push(URI);
  }

  return locations;
}

/**
 * Find an ES module named like `backgroundtasks/BackgroundTask_${name}.sys.mjs`,
 * import it, and return the whole module.
 *
 * When testing, allow to load from `XPCSHELL_TESTING_MODULES_URI`,
 * which is registered at `resource://testing-common`, the standard
 * location for test-only modules.
 *
 * @return {Object} The imported module.
 * @throws NS_ERROR_NOT_AVAILABLE if a background task with the given `name` is
 * not found.
 */
function findBackgroundTaskModule(name) {
  for (const URI of locationsForBackgroundTaskNamed(name)) {
    lazy.log.debug(`Looking for background task at URI: ${URI}`);

    try {
      const taskModule = ChromeUtils.importESModule(URI);
      lazy.log.info(`Found background task at URI: ${URI}`);
      return taskModule;
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw ex;
      }
    }
  }

  lazy.log.warn(`No backgroundtask named '${name}' registered`);
  throw new Components.Exception(
    `No backgroundtask named '${name}' registered`,
    Cr.NS_ERROR_NOT_AVAILABLE
  );
}

export class BackgroundTasksManager {
  get helpInfo() {
    const bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
      Ci.nsIBackgroundTasks
    );

    if (bts.isBackgroundTaskMode) {
      return lazy.DevToolsStartup.jsdebuggerHelpInfo;
    }

    return "";
  }

  handle(commandLine) {
    const bts = Cc["@mozilla.org/backgroundtasks;1"].getService(
      Ci.nsIBackgroundTasks
    );

    if (!bts.isBackgroundTaskMode) {
      lazy.log.info(
        `${Services.appinfo.processID}: !isBackgroundTaskMode, exiting`
      );
      return;
    }

    const name = bts.backgroundTaskName();
    lazy.log.info(
      `${Services.appinfo.processID}: Preparing to run background task named '${name}'` +
        ` (with ${commandLine.length} arguments)`
    );

    if (!("@mozilla.org/devtools/startup-clh;1" in Cc)) {
      return;
    }

    // Check this before the devtools startup flow handles and removes it.
    const CASE_INSENSITIVE = false;
    if (
      commandLine.findFlag("jsdebugger", CASE_INSENSITIVE) < 0 &&
      commandLine.findFlag("start-debugger-server", CASE_INSENSITIVE) < 0
    ) {
      lazy.log.info(
        `${Services.appinfo.processID}: No devtools flag found; not preparing devtools thread`
      );
      return;
    }

    const waitFlag =
      commandLine.findFlag("wait-for-jsdebugger", CASE_INSENSITIVE) != -1;
    if (waitFlag) {
      function onDevtoolsThreadReady(subject, topic, data) {
        lazy.log.info(
          `${Services.appinfo.processID}: Setting breakpoints for background task named '${name}'` +
            ` (with ${commandLine.length} arguments)`
        );

        const threadActor = subject.wrappedJSObject;
        threadActor.setBreakpointOnLoad(locationsForBackgroundTaskNamed(name));

        Services.obs.removeObserver(onDevtoolsThreadReady, topic);
      }

      Services.obs.addObserver(onDevtoolsThreadReady, "devtools-thread-ready");
    }

    const DevToolsStartup = Cc[
      "@mozilla.org/devtools/startup-clh;1"
    ].getService(Ci.nsICommandLineHandler);
    DevToolsStartup.handle(commandLine);
  }

  async runBackgroundTaskNamed(name, commandLine) {
    function addMarker(markerName) {
      return ChromeUtils.addProfilerMarker(markerName, undefined, name);
    }
    addMarker("BackgroundTasksManager:AfterRunBackgroundTaskNamed");

    lazy.log.info(
      `${Services.appinfo.processID}: Running background task named '${name}'` +
        ` (with ${commandLine.length} arguments)`
    );
    lazy.log.debug(
      `${Services.appinfo.processID}: Background task using profile` +
        ` '${Services.dirsvc.get("ProfD", Ci.nsIFile).path}'`
    );

    let exitCode = EXIT_CODE.NOT_FOUND;
    try {
      let taskModule = findBackgroundTaskModule(name);
      addMarker("BackgroundTasksManager:AfterFindRunBackgroundTask");

      // Get timing configuration. First check for default preferences,
      // then for per module overrides.
      timingSettings.minTaskRuntimeMS = Services.prefs.getIntPref(
        "toolkit.backgroundtasks.defaultMinTaskRuntimeMS",
        timingSettings.minTaskRuntimeMS
      );
      if (taskModule.backgroundTaskMinRuntimeMS) {
        timingSettings.minTaskRuntimeMS = taskModule.backgroundTaskMinRuntimeMS;
      }
      timingSettings.maxTaskRuntimeSec = Services.prefs.getIntPref(
        "toolkit.backgroundtasks.defaultTimeoutSec",
        timingSettings.maxTaskRuntimeSec
      );
      if (taskModule.backgroundTaskTimeoutSec) {
        timingSettings.maxTaskRuntimeSec = taskModule.backgroundTaskTimeoutSec;
      }

      try {
        let minimumReached = false;
        let minRuntime = new Promise(resolve =>
          lazy.setTimeout(() => {
            minimumReached = true;
            resolve(true);
          }, timingSettings.minTaskRuntimeMS)
        );
        exitCode = await Promise.race([
          new Promise(resolve =>
            lazy.setTimeout(() => {
              lazy.log.error(`Background task named '${name}' timed out`);
              resolve(EXIT_CODE.TIMEOUT);
            }, timingSettings.maxTaskRuntimeSec * 1000)
          ),
          taskModule.runBackgroundTask(commandLine),
        ]);
        if (!minimumReached) {
          lazy.log.debug(
            `Backgroundtask named '${name}' waiting for minimum runtime.`
          );
          await minRuntime;
        }
        lazy.log.info(
          `Backgroundtask named '${name}' completed with exit code ${exitCode}`
        );
      } catch (e) {
        lazy.log.error(`Backgroundtask named '${name}' threw exception`, e);
        exitCode = EXIT_CODE.EXCEPTION;
      }
    } finally {
      addMarker("BackgroundTasksManager:AfterAwaitRunBackgroundTask");

      lazy.log.info(`Invoking Services.startup.quit(..., ${exitCode})`);
      Services.startup.quit(Ci.nsIAppStartup.eForceQuit, exitCode);
    }

    return exitCode;
  }

  classID = Components.ID("{4d48c536-e16f-4699-8f9c-add4f28f92f0}");
  QueryInterface = ChromeUtils.generateQI([
    "nsIBackgroundTasksManager",
    "nsICommandLineHandler",
  ]);
}

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
export const EXIT_CODE = {
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
   * The task took too long and timed out.
   *
   * The default timeout is controlled by the pref:
   * "toolkit.backgroundtasks.defaultTimeoutSec", but tasks can override this
   * by exporting a non-zero `backgroundTaskTimeoutSec` value.
   */
  TIMEOUT: 4,

  /**
   * The last exit code reserved by this structure.  Use codes larger than this
   * code for background task-specific exit codes.
   */
  LAST_RESERVED: 10,
};
