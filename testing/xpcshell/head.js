/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains common code that is loaded before each test file(s).
 * See https://developer.mozilla.org/en-US/docs/Mozilla/QA/Writing_xpcshell-based_unit_tests
 * for more information.
 */

/* defined by the harness */
/* globals _HEAD_FILES, _HEAD_JS_PATH, _JSDEBUGGER_PORT, _JSCOV_DIR,
    _MOZINFO_JS_PATH, _TEST_FILE, _TEST_NAME, _TEST_CWD, _TESTING_MODULES_DIR:true,
    _PREFS_FILE */

/* defined by XPCShellImpl.cpp */
/* globals load, sendCommand, changeTestShellDir */

/* must be defined by tests using do_await_remote_message/do_send_remote_message */
/* globals Cc, Ci */

/* defined by this file but is defined as read-only for tests */
// eslint-disable-next-line no-redeclare
/* globals runningInParent: true */

/* may be defined in test files */
/* globals run_test */

var _quit = false;
var _passed = true;
var _tests_pending = 0;
var _cleanupFunctions = [];
var _pendingTimers = [];
var _profileInitialized = false;

// Assigned in do_load_child_test_harness.
var _XPCSHELL_PROCESS;

// Register the testing-common resource protocol early, to have access to its
// modules.
let _Services = Services;
_register_modules_protocol_handler();

let { AppConstants: _AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

let { PromiseTestUtils: _PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);

let { NetUtil: _NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

let { XPCOMUtils: _XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// Support a common assertion library, Assert.sys.mjs.
var { Assert: AssertCls } = ChromeUtils.importESModule(
  "resource://testing-common/Assert.sys.mjs"
);

// Pass a custom report function for xpcshell-test style reporting.
var Assert = new AssertCls(function (err, message, stack) {
  if (err) {
    do_report_result(false, err.message, err.stack);
  } else {
    do_report_result(true, message, stack);
  }
}, true);

// Bug 1506134 for followup.  Some xpcshell tests use ContentTask.sys.mjs, which
// expects browser-test.js to have set a testScope that includes record.
function record(condition, name, diag, stack) {
  do_report_result(condition, name, stack);
}

var _add_params = function (params) {
  if (typeof _XPCSHELL_PROCESS != "undefined") {
    params.xpcshell_process = _XPCSHELL_PROCESS;
  }
};

var _dumpLog = function (raw_msg) {
  dump("\n" + JSON.stringify(raw_msg) + "\n");
};

var { StructuredLogger: _LoggerClass } = ChromeUtils.importESModule(
  "resource://testing-common/StructuredLog.sys.mjs"
);
var _testLogger = new _LoggerClass("xpcshell/head.js", _dumpLog, [_add_params]);

// Disable automatic network detection, so tests work correctly when
// not connected to a network.
_Services.io.manageOfflineStatus = false;
_Services.io.offline = false;

// Determine if we're running on parent or child
var runningInParent = true;
try {
  // Don't use Services.appinfo here as it disables replacing appinfo with stubs
  // for test usage.
  runningInParent =
    // eslint-disable-next-line mozilla/use-services
    Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime).processType ==
    Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
} catch (e) {}

// Only if building of places is enabled.
if (runningInParent && "mozIAsyncHistory" in Ci) {
  // Ensure places history is enabled for xpcshell-tests as some non-FF
  // apps disable it.
  _Services.prefs.setBoolPref("places.history.enabled", true);
}

// Configure crash reporting, if possible
// We rely on the Python harness to set MOZ_CRASHREPORTER,
// MOZ_CRASHREPORTER_NO_REPORT, and handle checking for minidumps.
// Note that if we're in a child process, we don't want to init the
// crashreporter component.
try {
  if (runningInParent && "@mozilla.org/toolkit/crash-reporter;1" in Cc) {
    // Intentially access the crash reporter service directly for this.
    // eslint-disable-next-line mozilla/use-services
    let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
      Ci.nsICrashReporter
    );
    crashReporter.UpdateCrashEventsDir();
    crashReporter.minidumpPath = do_get_minidumpdir();

    // Tell the test harness that the crash reporter is set up, and any crash
    // after this point is supposed to be caught by the crash reporter.
    //
    // Due to the limitation on the remote xpcshell test, the process return
    // code does not represent the process crash. Any crash before this point
    // can be caught by the absence of this log.
    _testLogger.logData("crash_reporter_init");
  }
} catch (e) {}

if (runningInParent) {
  _Services.prefs.setBoolPref("dom.push.connection.enabled", false);
}

// Configure a console listener so messages sent to it are logged as part
// of the test.
try {
  let levelNames = {};
  for (let level of ["debug", "info", "warn", "error"]) {
    levelNames[Ci.nsIConsoleMessage[level]] = level;
  }

  let listener = {
    QueryInterface: ChromeUtils.generateQI(["nsIConsoleListener"]),
    observe(msg) {
      if (typeof info === "function") {
        info(
          "CONSOLE_MESSAGE: (" +
            levelNames[msg.logLevel] +
            ") " +
            msg.toString()
        );
      }
    },
  };
  // Don't use _Services.console here as it causes one of the devtools tests
  // to fail, probably due to initializing Services.console too early.
  // eslint-disable-next-line mozilla/use-services
  Cc["@mozilla.org/consoleservice;1"]
    .getService(Ci.nsIConsoleService)
    .registerListener(listener);
} catch (e) {}
/**
 * Date.now() is not necessarily monotonically increasing (insert sob story
 * about times not being the right tool to use for measuring intervals of time,
 * robarnold can tell all), so be wary of error by erring by at least
 * _timerFuzz ms.
 */
const _timerFuzz = 15;

function _Timer(func, delay) {
  delay = Number(delay);
  if (delay < 0) {
    do_throw("do_timeout() delay must be nonnegative");
  }

  if (typeof func !== "function") {
    do_throw("string callbacks no longer accepted; use a function!");
  }

  this._func = func;
  this._start = Date.now();
  this._delay = delay;

  var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(this, delay + _timerFuzz, timer.TYPE_ONE_SHOT);

  // Keep timer alive until it fires
  _pendingTimers.push(timer);
}
_Timer.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsITimerCallback"]),

  notify(timer) {
    _pendingTimers.splice(_pendingTimers.indexOf(timer), 1);

    // The current nsITimer implementation can undershoot, but even if it
    // couldn't, paranoia is probably a virtue here given the potential for
    // random orange on tinderboxen.
    var end = Date.now();
    var elapsed = end - this._start;
    if (elapsed >= this._delay) {
      try {
        this._func.call(null);
      } catch (e) {
        do_throw("exception thrown from do_timeout callback: " + e);
      }
      return;
    }

    // Timer undershot, retry with a little overshoot to try to avoid more
    // undershoots.
    var newDelay = this._delay - elapsed;
    do_timeout(newDelay, this._func);
  },
};

function _isGenerator(val) {
  return typeof val === "object" && val && typeof val.next === "function";
}

function _do_main() {
  if (_quit) {
    return;
  }

  _testLogger.info("running event loop");

  var tm = Cc["@mozilla.org/thread-manager;1"].getService();

  tm.spinEventLoopUntil("Test(xpcshell/head.js:_do_main)", () => _quit);

  tm.spinEventLoopUntilEmpty();
}

function _do_quit() {
  _testLogger.info("exiting test");
  _quit = true;
}

// This is useless, except to the extent that it has the side-effect of
// initializing the widget module, which some tests unfortunately
// accidentally rely on.
void Cc["@mozilla.org/widget/transferable;1"].createInstance();

/**
 * Overrides idleService with a mock.  Idle is commonly used for maintenance
 * tasks, thus if a test uses a service that requires the idle service, it will
 * start handling them.
 * This behaviour would cause random failures and slowdown tests execution,
 * for example by running database vacuum or cleanups for each test.
 *
 * @note Idle service is overridden by default.  If a test requires it, it will
 *       have to call do_get_idle() function at least once before use.
 */
var _fakeIdleService = {
  get registrar() {
    delete this.registrar;
    return (this.registrar = Components.manager.QueryInterface(
      Ci.nsIComponentRegistrar
    ));
  },
  contractID: "@mozilla.org/widget/useridleservice;1",
  CID: Components.ID("{9163a4ae-70c2-446c-9ac1-bbe4ab93004e}"),

  activate: function FIS_activate() {
    if (!this.originalCID) {
      this.originalCID = this.registrar.contractIDToCID(this.contractID);
      // Replace with the mock.
      this.registrar.registerFactory(
        this.CID,
        "Fake Idle Service",
        this.contractID,
        this.factory
      );
    }
  },

  deactivate: function FIS_deactivate() {
    if (this.originalCID) {
      // Unregister the mock.
      this.registrar.unregisterFactory(this.CID, this.factory);
      // Restore original factory.
      this.registrar.registerFactory(
        this.originalCID,
        "Idle Service",
        this.contractID,
        null
      );
      delete this.originalCID;
    }
  },

  factory: {
    // nsIFactory
    createInstance(aIID) {
      return _fakeIdleService.QueryInterface(aIID);
    },
    QueryInterface: ChromeUtils.generateQI(["nsIFactory"]),
  },

  // nsIUserIdleService
  get idleTime() {
    return 0;
  },
  addIdleObserver() {},
  removeIdleObserver() {},

  // eslint-disable-next-line mozilla/use-chromeutils-generateqi
  QueryInterface(aIID) {
    // Useful for testing purposes, see test_get_idle.js.
    if (aIID.equals(Ci.nsIFactory)) {
      return this.factory;
    }
    if (aIID.equals(Ci.nsIUserIdleService) || aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },
};

/**
 * Restores the idle service factory if needed and returns the service's handle.
 * @return A handle to the idle service.
 */
function do_get_idle() {
  _fakeIdleService.deactivate();
  return Cc[_fakeIdleService.contractID].getService(Ci.nsIUserIdleService);
}

// Map resource://test/ to current working directory and
// resource://testing-common/ to the shared test modules directory.
function _register_protocol_handlers() {
  let protocolHandler = _Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  let curDirURI = _Services.io.newFileURI(do_get_cwd());
  protocolHandler.setSubstitution("test", curDirURI);

  _register_modules_protocol_handler();
}

function _register_modules_protocol_handler() {
  if (!_TESTING_MODULES_DIR) {
    throw new Error(
      "Please define a path where the testing modules can be " +
        "found in a variable called '_TESTING_MODULES_DIR' before " +
        "head.js is included."
    );
  }

  let protocolHandler = _Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  let modulesFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  modulesFile.initWithPath(_TESTING_MODULES_DIR);

  if (!modulesFile.exists()) {
    throw new Error(
      "Specified modules directory does not exist: " + _TESTING_MODULES_DIR
    );
  }

  if (!modulesFile.isDirectory()) {
    throw new Error(
      "Specified modules directory is not a directory: " + _TESTING_MODULES_DIR
    );
  }

  let modulesURI = _Services.io.newFileURI(modulesFile);

  protocolHandler.setSubstitution("testing-common", modulesURI);
}

/* Debugging support */
// Used locally and by our self-tests.
function _setupDevToolsServer(breakpointFiles, callback) {
  // Always allow remote debugging.
  _Services.prefs.setBoolPref("devtools.debugger.remote-enabled", true);

  // for debugging-the-debugging, let an env var cause log spew.
  if (_Services.env.get("DEVTOOLS_DEBUGGER_LOG")) {
    _Services.prefs.setBoolPref("devtools.debugger.log", true);
  }
  if (_Services.env.get("DEVTOOLS_DEBUGGER_LOG_VERBOSE")) {
    _Services.prefs.setBoolPref("devtools.debugger.log.verbose", true);
  }

  let require;
  try {
    ({ require } = ChromeUtils.importESModule(
      "resource://devtools/shared/loader/Loader.sys.mjs"
    ));
  } catch (e) {
    throw new Error(
      "resource://devtools appears to be inaccessible from the " +
        "xpcshell environment.\n" +
        "This can usually be resolved by adding:\n" +
        "  firefox-appdir = browser\n" +
        "to the xpcshell.ini manifest.\n" +
        "It is possible for this to alter test behevior by " +
        "triggering additional browser code to run, so check " +
        "test behavior after making this change.\n" +
        "See also https://bugzil.la/1215378."
    );
  }
  let { DevToolsServer } = require("devtools/server/devtools-server");
  DevToolsServer.init();
  DevToolsServer.registerAllActors();
  let { createRootActor } = require("resource://testing-common/dbg-actors.js");
  DevToolsServer.setRootActor(createRootActor);
  DevToolsServer.allowChromeProcess = true;

  const TOPICS = [
    // An observer notification that tells us when the thread actor is ready
    // and can accept breakpoints.
    "devtools-thread-ready",
    // Or when devtools are destroyed and we should stop observing.
    "xpcshell-test-devtools-shutdown",
  ];
  let observe = function (subject, topic, data) {
    if (topic === "devtools-thread-ready") {
      const threadActor = subject.wrappedJSObject;
      threadActor.setBreakpointOnLoad(breakpointFiles);
    }

    for (let topicToRemove of TOPICS) {
      _Services.obs.removeObserver(observe, topicToRemove);
    }
    callback();
  };

  for (let topic of TOPICS) {
    _Services.obs.addObserver(observe, topic);
  }

  const { SocketListener } = require("devtools/shared/security/socket");

  return { DevToolsServer, SocketListener };
}

function _initDebugging(port) {
  let initialized = false;
  const { DevToolsServer, SocketListener } = _setupDevToolsServer(
    _TEST_FILE,
    () => {
      initialized = true;
    }
  );

  info("");
  info("*******************************************************************");
  info("Waiting for the debugger to connect on port " + port);
  info("");
  info("To connect the debugger, open a Firefox instance, select 'Connect'");
  info("from the Developer menu and specify the port as " + port);
  info("*******************************************************************");
  info("");

  const AuthenticatorType = DevToolsServer.Authenticators.get("PROMPT");
  const authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };
  const socketOptions = {
    authenticator,
    portOrPath: port,
  };

  const listener = new SocketListener(DevToolsServer, socketOptions);
  listener.open();

  // spin an event loop until the debugger connects.
  const tm = Cc["@mozilla.org/thread-manager;1"].getService();
  let lastUpdate = Date.now();
  tm.spinEventLoopUntil("Test(xpcshell/head.js:_initDebugging)", () => {
    if (initialized) {
      return true;
    }
    if (Date.now() - lastUpdate > 5000) {
      info("Still waiting for debugger to connect...");
      lastUpdate = Date.now();
    }
    return false;
  });
  // NOTE: if you want to debug the harness itself, you can now add a 'debugger'
  // statement anywhere and it will stop - but we've already added a breakpoint
  // for the first line of the test scripts, so we just continue...
  info("Debugger connected, starting test execution");
}

function _execute_test() {
  if (typeof _TEST_CWD != "undefined") {
    try {
      changeTestShellDir(_TEST_CWD);
    } catch (e) {
      _testLogger.error(_exception_message(e));
    }
  }
  if (runningInParent && _AppConstants.platform == "android") {
    try {
      // GeckoView initialization needs the profile
      do_get_profile(true);
      // Wake up GeckoViewStartup
      let geckoViewStartup = Cc["@mozilla.org/geckoview/startup;1"].getService(
        Ci.nsIObserver
      );
      geckoViewStartup.observe(null, "profile-after-change", null);
      geckoViewStartup.observe(null, "app-startup", null);

      // Glean needs to be initialized for metric recording & tests to work.
      // Usually this happens through Glean Kotlin,
      // but for xpcshell tests we initialize it from here.
      _Services.fog.initializeFOG();
    } catch (ex) {
      do_throw(`Failed to initialize GeckoView: ${ex}`, ex.stack);
    }
  }

  // _JSDEBUGGER_PORT is dynamically defined by <runxpcshelltests.py>.
  if (_JSDEBUGGER_PORT) {
    try {
      _initDebugging(_JSDEBUGGER_PORT);
    } catch (ex) {
      // Fail the test run immediately if debugging is requested but fails, so
      // that the failure state is more obvious.
      do_throw(`Failed to initialize debugging: ${ex}`, ex.stack);
    }
  }

  _register_protocol_handlers();

  // Override idle service by default.
  // Call do_get_idle() to restore the factory and get the service.
  _fakeIdleService.activate();

  _PromiseTestUtils.init();

  let coverageCollector = null;
  if (typeof _JSCOV_DIR === "string") {
    let _CoverageCollector = ChromeUtils.importESModule(
      "resource://testing-common/CoverageUtils.sys.mjs"
    ).CoverageCollector;
    coverageCollector = new _CoverageCollector(_JSCOV_DIR);
  }

  let startTime = Cu.now();

  // _HEAD_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_HEAD_FILES);
  // _TEST_FILE is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TEST_FILE);

  // Tack Assert.sys.mjs methods to the current scope.
  this.Assert = Assert;
  for (let func in Assert) {
    this[func] = Assert[func].bind(Assert);
  }

  const { PerTestCoverageUtils } = ChromeUtils.importESModule(
    "resource://testing-common/PerTestCoverageUtils.sys.mjs"
  );

  if (runningInParent) {
    PerTestCoverageUtils.beforeTestSync();
  }

  try {
    do_test_pending("MAIN run_test");
    // Check if run_test() is defined. If defined, run it.
    // Else, call run_next_test() directly to invoke tests
    // added by add_test() and add_task().
    if (typeof run_test === "function") {
      run_test();
    } else {
      run_next_test();
    }

    do_test_finished("MAIN run_test");
    _do_main();
    _PromiseTestUtils.assertNoUncaughtRejections();

    if (coverageCollector != null) {
      coverageCollector.recordTestCoverage(_TEST_FILE[0]);
    }

    if (runningInParent) {
      PerTestCoverageUtils.afterTestSync();
    }
  } catch (e) {
    _passed = false;
    // do_check failures are already logged and set _quit to true and throw
    // NS_ERROR_ABORT. If both of those are true it is likely this exception
    // has already been logged so there is no need to log it again. It's
    // possible that this will mask an NS_ERROR_ABORT that happens after a
    // do_check failure though.

    if (!_quit || e.result != Cr.NS_ERROR_ABORT) {
      let extra = {};
      if (e.fileName) {
        extra.source_file = e.fileName;
        if (e.lineNumber) {
          extra.line_number = e.lineNumber;
        }
      } else {
        extra.source_file = "xpcshell/head.js";
      }
      let message = _exception_message(e);
      if (e.stack) {
        extra.stack = _format_stack(e.stack);
      }
      _testLogger.error(message, extra);
    }
  } finally {
    if (coverageCollector != null) {
      coverageCollector.finalize();
    }
  }

  // Execute all of our cleanup functions.
  let reportCleanupError = function (ex) {
    let stack, filename;
    if (ex && typeof ex == "object" && "stack" in ex) {
      stack = ex.stack;
    } else {
      stack = Components.stack.caller;
    }
    if (stack instanceof Ci.nsIStackFrame) {
      filename = stack.filename;
    } else if (ex.fileName) {
      filename = ex.fileName;
    }
    _testLogger.error(_exception_message(ex), {
      stack: _format_stack(stack),
      source_file: filename,
    });
  };

  let complete = !_cleanupFunctions.length;
  let cleanupStartTime = complete ? 0 : Cu.now();
  (async () => {
    for (let func of _cleanupFunctions.reverse()) {
      try {
        let result = await func();
        if (_isGenerator(result)) {
          Assert.ok(false, "Cleanup function returned a generator");
        }
      } catch (ex) {
        reportCleanupError(ex);
      }
    }
    _cleanupFunctions = [];
  })()
    .catch(reportCleanupError)
    .then(() => (complete = true));
  _Services.tm.spinEventLoopUntil(
    "Test(xpcshell/head.js:_execute_test)",
    () => complete
  );
  if (cleanupStartTime) {
    ChromeUtils.addProfilerMarker(
      "xpcshell-test",
      { category: "Test", startTime: cleanupStartTime },
      "Cleanup functions"
    );
  }

  ChromeUtils.addProfilerMarker(
    "xpcshell-test",
    { category: "Test", startTime },
    _TEST_NAME
  );
  _Services.obs.notifyObservers(null, "test-complete");

  // Restore idle service to avoid leaks.
  _fakeIdleService.deactivate();

  if (
    globalThis.hasOwnProperty("storage") &&
    StorageManager.isInstance(globalThis.storage)
  ) {
    globalThis.storage.shutdown();
  }

  if (_profileInitialized) {
    // Since we have a profile, we will notify profile shutdown topics at
    // the end of the current test, to ensure correct cleanup on shutdown.
    _Services.startup.advanceShutdownPhase(
      _Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNNETTEARDOWN
    );
    _Services.startup.advanceShutdownPhase(
      _Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNTEARDOWN
    );
    _Services.startup.advanceShutdownPhase(
      _Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN
    );
    _Services.startup.advanceShutdownPhase(
      _Services.startup.SHUTDOWN_PHASE_APPSHUTDOWNQM
    );

    _profileInitialized = false;
  }

  try {
    _PromiseTestUtils.ensureDOMPromiseRejectionsProcessed();
    _PromiseTestUtils.assertNoUncaughtRejections();
    _PromiseTestUtils.assertNoMoreExpectedRejections();
  } finally {
    // It's important to terminate the module to avoid crashes on shutdown.
    _PromiseTestUtils.uninit();
  }

  // Skip the normal shutdown path for optimized builds that don't do leak checking.
  if (
    runningInParent &&
    !_AppConstants.RELEASE_OR_BETA &&
    !_AppConstants.DEBUG &&
    !_AppConstants.MOZ_CODE_COVERAGE &&
    !_AppConstants.ASAN &&
    !_AppConstants.TSAN
  ) {
    Cu.exitIfInAutomation();
  }
}

/**
 * Loads files.
 *
 * @param aFiles Array of files to load.
 */
function _load_files(aFiles) {
  function load_file(element, index, array) {
    try {
      let startTime = Cu.now();
      load(element);
      ChromeUtils.addProfilerMarker(
        "load_file",
        { category: "Test", startTime },
        element.replace(/.*\/_?tests\/xpcshell\//, "")
      );
    } catch (e) {
      let extra = {
        source_file: element,
      };
      if (e.stack) {
        extra.stack = _format_stack(e.stack);
      }
      _testLogger.error(_exception_message(e), extra);
    }
  }

  aFiles.forEach(load_file);
}

function _wrap_with_quotes_if_necessary(val) {
  return typeof val == "string" ? '"' + val + '"' : val;
}

/* ************* Functions to be used from the tests ************* */

/**
 * Prints a message to the output log.
 */
function info(msg, data) {
  ChromeUtils.addProfilerMarker("INFO", { category: "Test" }, msg);
  msg = _wrap_with_quotes_if_necessary(msg);
  data = data ? data : null;
  _testLogger.info(msg, data);
}

/**
 * Calls the given function at least the specified number of milliseconds later.
 * The callback will not undershoot the given time, but it might overshoot --
 * don't expect precision!
 *
 * @param delay : uint
 *   the number of milliseconds to delay
 * @param callback : function() : void
 *   the function to call
 */
function do_timeout(delay, func) {
  new _Timer(func, Number(delay));
}

function executeSoon(callback, aName) {
  let funcName = aName ? aName : callback.name;
  do_test_pending(funcName);

  _Services.tm.dispatchToMainThread({
    run() {
      try {
        callback();
      } catch (e) {
        // do_check failures are already logged and set _quit to true and throw
        // NS_ERROR_ABORT. If both of those are true it is likely this exception
        // has already been logged so there is no need to log it again. It's
        // possible that this will mask an NS_ERROR_ABORT that happens after a
        // do_check failure though.
        if (!_quit || e.result != Cr.NS_ERROR_ABORT) {
          let stack = e.stack ? _format_stack(e.stack) : null;
          _testLogger.testStatus(
            _TEST_NAME,
            funcName,
            "FAIL",
            "PASS",
            _exception_message(e),
            stack
          );
          _do_quit();
        }
      } finally {
        do_test_finished(funcName);
      }
    },
  });
}

/**
 * Shows an error message and the current stack and aborts the test.
 *
 * @param error  A message string or an Error object.
 * @param stack  null or nsIStackFrame object or a string containing
 *               \n separated stack lines (as in Error().stack).
 */
function do_throw(error, stack) {
  let filename = "";
  // If we didn't get passed a stack, maybe the error has one
  // otherwise get it from our call context
  stack = stack || error.stack || Components.stack.caller;

  if (stack instanceof Ci.nsIStackFrame) {
    filename = stack.filename;
  } else if (error.fileName) {
    filename = error.fileName;
  }

  _testLogger.error(_exception_message(error), {
    source_file: filename,
    stack: _format_stack(stack),
  });
  ChromeUtils.addProfilerMarker(
    "ERROR",
    { category: "Test", captureStack: true },
    _exception_message(error)
  );
  _abort_failed_test();
}

function _abort_failed_test() {
  // Called to abort the test run after all failures are logged.
  _passed = false;
  _do_quit();
  throw Components.Exception("", Cr.NS_ERROR_ABORT);
}

function _format_stack(stack) {
  let normalized;
  if (stack instanceof Ci.nsIStackFrame) {
    let frames = [];
    for (let frame = stack; frame; frame = frame.caller) {
      frames.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
    }
    normalized = frames.join("\n");
  } else {
    normalized = "" + stack;
  }
  return normalized;
}

// Make a nice display string from an object that behaves
// like Error
function _exception_message(ex) {
  if (ex === undefined) {
    return "`undefined` exception, maybe from an empty reject()?";
  }
  let message = "";
  if (ex.name) {
    message = ex.name + ": ";
  }
  if (ex.message) {
    message += ex.message;
  }
  if (ex.fileName) {
    message += " at " + ex.fileName;
    if (ex.lineNumber) {
      message += ":" + ex.lineNumber;
    }
  }
  if (message !== "") {
    return message;
  }
  // Force ex to be stringified
  return "" + ex;
}

function do_report_unexpected_exception(ex, text) {
  let filename = Components.stack.caller.filename;
  text = text ? text + " - " : "";

  _passed = false;
  _testLogger.error(text + "Unexpected exception " + _exception_message(ex), {
    source_file: filename,
    stack: _format_stack(ex?.stack),
  });
  _do_quit();
  throw Components.Exception("", Cr.NS_ERROR_ABORT);
}

function do_note_exception(ex, text) {
  let filename = Components.stack.caller.filename;
  _testLogger.info(text + "Swallowed exception " + _exception_message(ex), {
    source_file: filename,
    stack: _format_stack(ex?.stack),
  });
}

function do_report_result(passed, text, stack, todo) {
  // Match names like head.js, head_foo.js, and foo_head.js, but not
  // test_headache.js
  while (/(\/head(_.+)?|head)\.js$/.test(stack.filename) && stack.caller) {
    stack = stack.caller;
  }

  let name = _gRunningTest ? _gRunningTest.name : stack.name;
  let message;
  if (name) {
    message = "[" + name + " : " + stack.lineNumber + "] " + text;
  } else {
    message = text;
  }

  if (passed) {
    if (todo) {
      _testLogger.testStatus(
        _TEST_NAME,
        name,
        "PASS",
        "FAIL",
        message,
        _format_stack(stack)
      );
      ChromeUtils.addProfilerMarker(
        "UNEXPECTED-PASS",
        { category: "Test" },
        message
      );
      _abort_failed_test();
    } else {
      _testLogger.testStatus(_TEST_NAME, name, "PASS", "PASS", message);
      ChromeUtils.addProfilerMarker("PASS", { category: "Test" }, message);
    }
  } else if (todo) {
    _testLogger.testStatus(_TEST_NAME, name, "FAIL", "FAIL", message);
    ChromeUtils.addProfilerMarker("TODO", { category: "Test" }, message);
  } else {
    _testLogger.testStatus(
      _TEST_NAME,
      name,
      "FAIL",
      "PASS",
      message,
      _format_stack(stack)
    );
    ChromeUtils.addProfilerMarker("FAIL", { category: "Test" }, message);
    _abort_failed_test();
  }
}

function _do_check_eq(left, right, stack, todo) {
  if (!stack) {
    stack = Components.stack.caller;
  }

  var text =
    _wrap_with_quotes_if_necessary(left) +
    " == " +
    _wrap_with_quotes_if_necessary(right);
  do_report_result(left == right, text, stack, todo);
}

function todo_check_eq(left, right, stack) {
  if (!stack) {
    stack = Components.stack.caller;
  }

  _do_check_eq(left, right, stack, true);
}

function todo_check_true(condition, stack) {
  if (!stack) {
    stack = Components.stack.caller;
  }

  todo_check_eq(condition, true, stack);
}

function todo_check_false(condition, stack) {
  if (!stack) {
    stack = Components.stack.caller;
  }

  todo_check_eq(condition, false, stack);
}

function todo_check_null(condition, stack = Components.stack.caller) {
  todo_check_eq(condition, null, stack);
}

// Check that |func| throws an nsIException that has
// |Components.results[resultName]| as the value of its 'result' property.
function do_check_throws_nsIException(
  func,
  resultName,
  stack = Components.stack.caller,
  todo = false
) {
  let expected = Cr[resultName];
  if (typeof expected !== "number") {
    do_throw(
      "do_check_throws_nsIException requires a Components.results" +
        " property name, not " +
        uneval(resultName),
      stack
    );
  }

  let msg =
    "do_check_throws_nsIException: func should throw" +
    " an nsIException whose 'result' is Components.results." +
    resultName;

  try {
    func();
  } catch (ex) {
    if (!(ex instanceof Ci.nsIException) || ex.result !== expected) {
      do_report_result(
        false,
        msg + ", threw " + legible_exception(ex) + " instead",
        stack,
        todo
      );
    }

    do_report_result(true, msg, stack, todo);
    return;
  }

  // Call this here, not in the 'try' clause, so do_report_result's own
  // throw doesn't get caught by our 'catch' clause.
  do_report_result(false, msg + ", but returned normally", stack, todo);
}

// Produce a human-readable form of |exception|. This looks up
// Components.results values, tries toString methods, and so on.
function legible_exception(exception) {
  switch (typeof exception) {
    case "object":
      if (exception instanceof Ci.nsIException) {
        return "nsIException instance: " + uneval(exception.toString());
      }
      return exception.toString();

    case "number":
      for (let name in Cr) {
        if (exception === Cr[name]) {
          return "Components.results." + name;
        }
      }

    // Fall through.
    default:
      return uneval(exception);
  }
}

function do_check_instanceof(
  value,
  constructor,
  stack = Components.stack.caller,
  todo = false
) {
  do_report_result(
    value instanceof constructor,
    "value should be an instance of " + constructor.name,
    stack,
    todo
  );
}

function todo_check_instanceof(
  value,
  constructor,
  stack = Components.stack.caller
) {
  do_check_instanceof(value, constructor, stack, true);
}

function do_test_pending(aName) {
  ++_tests_pending;

  _testLogger.info(
    "(xpcshell/head.js) | test" +
      (aName ? " " + aName : "") +
      " pending (" +
      _tests_pending +
      ")"
  );
}

function do_test_finished(aName) {
  _testLogger.info(
    "(xpcshell/head.js) | test" +
      (aName ? " " + aName : "") +
      " finished (" +
      _tests_pending +
      ")"
  );
  if (--_tests_pending == 0) {
    _do_quit();
  }
}

function do_get_file(path, allowNonexistent) {
  try {
    let lf = _Services.dirsvc.get("CurWorkD", Ci.nsIFile);

    let bits = path.split("/");
    for (let i = 0; i < bits.length; i++) {
      if (bits[i]) {
        if (bits[i] == "..") {
          lf = lf.parent;
        } else {
          lf.append(bits[i]);
        }
      }
    }

    if (!allowNonexistent && !lf.exists()) {
      // Not using do_throw(): caller will continue.
      _passed = false;
      var stack = Components.stack.caller;
      _testLogger.error(
        "[" +
          stack.name +
          " : " +
          stack.lineNumber +
          "] " +
          lf.path +
          " does not exist"
      );
    }

    return lf;
  } catch (ex) {
    do_throw(ex.toString(), Components.stack.caller);
  }

  return null;
}

// do_get_cwd() isn't exactly self-explanatory, so provide a helper
function do_get_cwd() {
  return do_get_file("");
}

function do_load_manifest(path) {
  var lf = do_get_file(path);
  const nsIComponentRegistrar = Ci.nsIComponentRegistrar;
  Assert.ok(Components.manager instanceof nsIComponentRegistrar);
  // Previous do_check_true() is not a test check.
  Components.manager.autoRegister(lf);
}

/**
 * Parse a DOM document.
 *
 * @param aPath File path to the document.
 * @param aType Content type to use in DOMParser.
 *
 * @return Document from the file.
 */
function do_parse_document(aPath, aType) {
  switch (aType) {
    case "application/xhtml+xml":
    case "application/xml":
    case "text/xml":
      break;

    default:
      do_throw(
        "type: expected application/xhtml+xml, application/xml or text/xml," +
          " got '" +
          aType +
          "'",
        Components.stack.caller
      );
  }

  let file = do_get_file(aPath);
  let url = _Services.io.newFileURI(file).spec;
  file = null;
  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", url);
    xhr.responseType = "document";
    xhr.onerror = reject;
    xhr.onload = () => {
      resolve(xhr.response);
    };
    xhr.send();
  });
}

/**
 * Registers a function that will run when the test harness is done running all
 * tests.
 *
 * @param aFunction
 *        The function to be called when the test harness has finished running.
 */
function registerCleanupFunction(aFunction) {
  _cleanupFunctions.push(aFunction);
}

/**
 * Returns the directory for a temp dir, which is created by the
 * test harness. Every test gets its own temp dir.
 *
 * @return nsIFile of the temporary directory
 */
function do_get_tempdir() {
  // the python harness sets this in the environment for us
  let path = _Services.env.get("XPCSHELL_TEST_TEMP_DIR");
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);
  return file;
}

/**
 * Returns the directory for crashreporter minidumps.
 *
 * @return nsIFile of the minidump directory
 */
function do_get_minidumpdir() {
  // the python harness may set this in the environment for us
  let path = _Services.env.get("XPCSHELL_MINIDUMP_DIR");
  if (path) {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(path);
    return file;
  }
  return do_get_tempdir();
}

/**
 * Registers a directory with the profile service,
 * and return the directory as an nsIFile.
 *
 * @param notifyProfileAfterChange Whether to notify for "profile-after-change".
 * @return nsIFile of the profile directory.
 */
function do_get_profile(notifyProfileAfterChange = false) {
  if (!runningInParent) {
    _testLogger.info("Ignoring profile creation from child process.");
    return null;
  }

  // the python harness sets this in the environment for us
  let profd = Services.env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(profd);

  let provider = {
    getFile(prop, persistent) {
      persistent.value = true;
      if (
        prop == "ProfD" ||
        prop == "ProfLD" ||
        prop == "ProfDS" ||
        prop == "ProfLDS" ||
        prop == "TmpD"
      ) {
        return file.clone();
      }
      return null;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
  };
  _Services.dirsvc
    .QueryInterface(Ci.nsIDirectoryService)
    .registerProvider(provider);

  try {
    _Services.dirsvc.undefine("TmpD");
  } catch (e) {
    // This throws if the key is not already registered, but that
    // doesn't matter.
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  // We need to update the crash events directory when the profile changes.
  if (runningInParent && "@mozilla.org/toolkit/crash-reporter;1" in Cc) {
    // Intentially access the crash reporter service directly for this.
    // eslint-disable-next-line mozilla/use-services
    let crashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
      Ci.nsICrashReporter
    );
    crashReporter.UpdateCrashEventsDir();
  }

  if (!_profileInitialized) {
    _Services.obs.notifyObservers(
      null,
      "profile-do-change",
      "xpcshell-do-get-profile"
    );
    _profileInitialized = true;
    if (notifyProfileAfterChange) {
      _Services.obs.notifyObservers(
        null,
        "profile-after-change",
        "xpcshell-do-get-profile"
      );
    }
  }

  // The methods of 'provider' will retain this scope so null out everything
  // to avoid spurious leak reports.
  profd = null;
  provider = null;
  return file.clone();
}

/**
 * This function loads head.js (this file) in the child process, so that all
 * functions defined in this file (do_throw, etc) are available to subsequent
 * sendCommand calls.  It also sets various constants used by these functions.
 *
 * (Note that you may use sendCommand without calling this function first;  you
 * simply won't have any of the functions in this file available.)
 */
function do_load_child_test_harness() {
  // Make sure this isn't called from child process
  if (!runningInParent) {
    do_throw("run_test_in_child cannot be called from child!");
  }

  // Allow to be called multiple times, but only run once
  if (typeof do_load_child_test_harness.alreadyRun != "undefined") {
    return;
  }
  do_load_child_test_harness.alreadyRun = 1;

  _XPCSHELL_PROCESS = "parent";

  let command =
    "const _HEAD_JS_PATH=" +
    uneval(_HEAD_JS_PATH) +
    "; " +
    "const _HEAD_FILES=" +
    uneval(_HEAD_FILES) +
    "; " +
    "const _MOZINFO_JS_PATH=" +
    uneval(_MOZINFO_JS_PATH) +
    "; " +
    "const _TEST_NAME=" +
    uneval(_TEST_NAME) +
    "; " +
    // We'll need more magic to get the debugger working in the child
    "const _JSDEBUGGER_PORT=0; " +
    "_XPCSHELL_PROCESS='child';";

  if (typeof _JSCOV_DIR === "string") {
    command += " const _JSCOV_DIR=" + uneval(_JSCOV_DIR) + ";";
  }

  if (typeof _TEST_CWD != "undefined") {
    command += " const _TEST_CWD=" + uneval(_TEST_CWD) + ";";
  }

  if (_TESTING_MODULES_DIR) {
    command +=
      " const _TESTING_MODULES_DIR=" + uneval(_TESTING_MODULES_DIR) + ";";
  }

  command += " load(_HEAD_JS_PATH);";
  sendCommand(command);
}

/**
 * Runs an entire xpcshell unit test in a child process (rather than in chrome,
 * which is the default).
 *
 * This function returns immediately, before the test has completed.
 *
 * @param testFile
 *        The name of the script to run.  Path format same as load().
 * @param optionalCallback.
 *        Optional function to be called (in parent) when test on child is
 *        complete.  If provided, the function must call do_test_finished();
 * @return Promise Resolved when the test in the child is complete.
 */
function run_test_in_child(testFile, optionalCallback) {
  return new Promise(resolve => {
    var callback = () => {
      resolve();
      if (typeof optionalCallback == "undefined") {
        do_test_finished();
      } else {
        optionalCallback();
      }
    };

    do_load_child_test_harness();

    var testPath = do_get_file(testFile).path.replace(/\\/g, "/");
    do_test_pending("run in child");
    sendCommand(
      "_testLogger.info('CHILD-TEST-STARTED'); " +
        "const _TEST_FILE=['" +
        testPath +
        "']; " +
        "_execute_test(); " +
        "_testLogger.info('CHILD-TEST-COMPLETED');",
      callback
    );
  });
}

/**
 * Execute a given function as soon as a particular cross-process message is received.
 * Must be paired with do_send_remote_message or equivalent ProcessMessageManager calls.
 *
 * @param optionalCallback
 *        Optional callback that is invoked when the message is received.  If provided,
 *        the function must call do_test_finished().
 * @return Promise Promise that is resolved when the message is received.
 */
function do_await_remote_message(name, optionalCallback) {
  return new Promise(resolve => {
    var listener = {
      receiveMessage(message) {
        if (message.name == name) {
          mm.removeMessageListener(name, listener);
          resolve(message.data);
          if (optionalCallback) {
            optionalCallback(message.data);
          } else {
            do_test_finished();
          }
        }
      },
    };

    var mm;
    if (runningInParent) {
      mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
    } else {
      mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
    }
    do_test_pending();
    mm.addMessageListener(name, listener);
  });
}

/**
 * Asynchronously send a message to all remote processes. Pairs with do_await_remote_message
 * or equivalent ProcessMessageManager listeners.
 */
function do_send_remote_message(name, data) {
  var mm;
  var sender;
  if (runningInParent) {
    mm = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService();
    sender = "broadcastAsyncMessage";
  } else {
    mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService();
    sender = "sendAsyncMessage";
  }
  mm[sender](name, data);
}

/**
 * Schedules and awaits a precise GC, and forces CC, `maxCount` number of times.
 * @param maxCount
 *        How many times GC and CC should be scheduled.
 */
async function schedulePreciseGCAndForceCC(maxCount) {
  for (let count = 0; count < maxCount; count++) {
    await new Promise(resolve => Cu.schedulePreciseGC(resolve));
    Cu.forceCC();
  }
}

/**
 * Add a test function to the list of tests that are to be run asynchronously.
 *
 * @param funcOrProperties
 *        A function to be run or an object represents test properties.
 *        Supported properties:
 *          skip_if : An arrow function which has an expression to be
 *                    evaluated whether the test is skipped or not.
 *          pref_set: An array of preferences to set for the test, reset at end of test.
 * @param func
 *        A function to be run only if the funcOrProperies is not a function.
 * @param isTask
 *        Optional flag that indicates whether `func` is a task. Defaults to `false`.
 * @param isSetup
 *        Optional flag that indicates whether `func` is a setup task. Defaults to `false`.
 *        Implies isTask.
 *
 * Each test function must call run_next_test() when it's done. Test files
 * should call run_next_test() in their run_test function to execute all
 * async tests.
 *
 * @return the test function that was passed in.
 */
var _gSupportedProperties = ["skip_if", "pref_set"];
var _gTests = [];
var _gRunOnlyThisTest = null;
function add_test(
  properties,
  func = properties,
  isTask = false,
  isSetup = false
) {
  if (isSetup) {
    isTask = true;
  }
  if (typeof properties == "function") {
    properties = { isTask, isSetup };
    _gTests.push([properties, func]);
  } else if (typeof properties == "object") {
    // Ensure only documented properties are in the object.
    for (let prop of Object.keys(properties)) {
      if (!_gSupportedProperties.includes(prop)) {
        do_throw(`Task property is not supported: ${prop}`);
      }
    }
    properties.isTask = isTask;
    properties.isSetup = isSetup;
    _gTests.push([properties, func]);
  } else {
    do_throw("add_test() should take a function or an object and a function");
  }
  func.skip = () => (properties.skip_if = () => true);
  func.only = () => (_gRunOnlyThisTest = func);
  return func;
}

/**
 * Add a test function which is a Task function.
 *
 * @param funcOrProperties
 *        An async function to be run or an object represents test properties.
 *        Supported properties:
 *          skip_if : An arrow function which has an expression to be
 *                    evaluated whether the test is skipped or not.
 *          pref_set: An array of preferences to set for the test, reset at end of test.
 * @param func
 *        An async function to be run only if the funcOrProperies is not a function.
 *
 * Task functions are functions fed into Task.jsm's Task.spawn(). They are async
 * functions that emit promises.
 *
 * If an exception is thrown, a do_check_* comparison fails, or if a rejected
 * promise is yielded, the test function aborts immediately and the test is
 * reported as a failure.
 *
 * Unlike add_test(), there is no need to call run_next_test(). The next test
 * will run automatically as soon the task function is exhausted. To trigger
 * premature (but successful) termination of the function or simply return.
 *
 * Example usage:
 *
 * add_task(async function test() {
 *   let result = await Promise.resolve(true);
 *
 *   do_check_true(result);
 *
 *   let secondary = await someFunctionThatReturnsAPromise(result);
 *   do_check_eq(secondary, "expected value");
 * });
 *
 * add_task(async function test_early_return() {
 *   let result = await somethingThatReturnsAPromise();
 *
 *   if (!result) {
 *     // Test is ended immediately, with success.
 *     return;
 *   }
 *
 *   do_check_eq(result, "foo");
 * });
 *
 * add_task({
 *   skip_if: () => !("@mozilla.org/telephony/volume-service;1" in Components.classes),
 *   pref_set: [["some.pref", "value"], ["another.pref", true]],
 * }, async function test_volume_service() {
 *   let volumeService = Cc["@mozilla.org/telephony/volume-service;1"]
 *     .getService(Ci.nsIVolumeService);
 *   ...
 * });
 */
function add_task(properties, func = properties) {
  return add_test(properties, func, true);
}

/**
 * add_setup is like add_task, but creates setup tasks.
 */
function add_setup(properties, func = properties) {
  return add_test(properties, func, true, true);
}

const _setTaskPrefs = prefs => {
  for (let [pref, value] of prefs) {
    if (value === undefined) {
      // Clear any pref that didn't have a user value.
      info(`Clearing pref "${pref}"`);
      _Services.prefs.clearUserPref(pref);
      continue;
    }

    info(`Setting pref "${pref}": ${value}`);
    switch (typeof value) {
      case "boolean":
        _Services.prefs.setBoolPref(pref, value);
        break;
      case "number":
        _Services.prefs.setIntPref(pref, value);
        break;
      case "string":
        _Services.prefs.setStringPref(pref, value);
        break;
      default:
        throw new Error("runWithPrefs doesn't support this pref type yet");
    }
  }
};

const _getTaskPrefs = prefs => {
  return prefs.map(([pref, value]) => {
    info(`Getting initial pref value for "${pref}"`);
    if (!_Services.prefs.prefHasUserValue(pref)) {
      // Check if the pref doesn't have a user value.
      return [pref, undefined];
    }
    switch (typeof value) {
      case "boolean":
        return [pref, _Services.prefs.getBoolPref(pref)];
      case "number":
        return [pref, _Services.prefs.getIntPref(pref)];
      case "string":
        return [pref, _Services.prefs.getStringPref(pref)];
      default:
        throw new Error("runWithPrefs doesn't support this pref type yet");
    }
  });
};

/**
 * Runs the next test function from the list of async tests.
 */
var _gRunningTest = null;
var _gTestIndex = 0; // The index of the currently running test.
var _gTaskRunning = false;
var _gSetupRunning = false;
function run_next_test() {
  if (_gTaskRunning) {
    throw new Error(
      "run_next_test() called from an add_task() test function. " +
        "run_next_test() should not be called from inside add_setup() or add_task() " +
        "under any circumstances!"
    );
  }

  if (_gSetupRunning) {
    throw new Error(
      "run_next_test() called from an add_setup() test function. " +
        "run_next_test() should not be called from inside add_setup() or add_task() " +
        "under any circumstances!"
    );
  }

  function _run_next_test() {
    if (_gTestIndex < _gTests.length) {
      // Check for uncaught rejections as early and often as possible.
      _PromiseTestUtils.assertNoUncaughtRejections();
      let _properties;
      [_properties, _gRunningTest] = _gTests[_gTestIndex++];

      // Must set to pending before we check for skip, so that we keep the
      // running counts correct.
      _testLogger.info(
        `${_TEST_NAME} | Starting ${_properties.isSetup ? "setup " : ""}${
          _gRunningTest.name
        }`
      );
      do_test_pending(_gRunningTest.name);

      if (
        (typeof _properties.skip_if == "function" && _properties.skip_if()) ||
        (_gRunOnlyThisTest &&
          _gRunningTest != _gRunOnlyThisTest &&
          !_properties.isSetup)
      ) {
        let _condition = _gRunOnlyThisTest
          ? "only one task may run."
          : _properties.skip_if.toSource().replace(/\(\)\s*=>\s*/, "");
        if (_condition == "true") {
          _condition = "explicitly skipped.";
        }
        let _message =
          _gRunningTest.name +
          " skipped because the following conditions were" +
          " met: (" +
          _condition +
          ")";
        _testLogger.testStatus(
          _TEST_NAME,
          _gRunningTest.name,
          "SKIP",
          "SKIP",
          _message
        );
        executeSoon(run_next_test);
        return;
      }

      let initialPrefsValues = [];
      if (_properties.pref_set) {
        initialPrefsValues = _getTaskPrefs(_properties.pref_set);
        _setTaskPrefs(_properties.pref_set);
      }

      if (_properties.isTask) {
        if (_properties.isSetup) {
          _gSetupRunning = true;
        } else {
          _gTaskRunning = true;
        }
        let startTime = Cu.now();
        (async () => _gRunningTest())().then(
          result => {
            _gTaskRunning = _gSetupRunning = false;
            ChromeUtils.addProfilerMarker(
              "task",
              { category: "Test", startTime },
              _gRunningTest.name || undefined
            );
            if (_isGenerator(result)) {
              Assert.ok(false, "Task returned a generator");
            }
            _setTaskPrefs(initialPrefsValues);
            run_next_test();
          },
          ex => {
            _gTaskRunning = _gSetupRunning = false;
            ChromeUtils.addProfilerMarker(
              "task",
              { category: "Test", startTime },
              _gRunningTest.name || undefined
            );
            _setTaskPrefs(initialPrefsValues);
            try {
              // Note `ex` at this point could be undefined, for example as
              // result of a bare call to reject().
              do_report_unexpected_exception(ex);
            } catch (error) {
              // The above throws NS_ERROR_ABORT and we don't want this to show
              // up as an unhandled rejection later. If any other exception
              // happened, something went wrong, so we abort.
              if (error.result != Cr.NS_ERROR_ABORT) {
                let extra = {};
                if (error.fileName) {
                  extra.source_file = error.fileName;
                  if (error.lineNumber) {
                    extra.line_number = error.lineNumber;
                  }
                } else {
                  extra.source_file = "xpcshell/head.js";
                }
                if (error.stack) {
                  extra.stack = _format_stack(error.stack);
                }
                _testLogger.error(_exception_message(error), extra);
                _do_quit();
                throw Components.Exception("", Cr.NS_ERROR_ABORT);
              }
            }
          }
        );
      } else {
        // Exceptions do not kill asynchronous tests, so they'll time out.
        let startTime = Cu.now();
        try {
          _gRunningTest();
        } catch (e) {
          do_throw(e);
        } finally {
          ChromeUtils.addProfilerMarker(
            "xpcshell-test",
            { category: "Test", startTime },
            _gRunningTest.name || undefined
          );
          _setTaskPrefs(initialPrefsValues);
        }
      }
    }
  }

  function frontLoadSetups() {
    _gTests.sort(([propsA, funcA], [propsB, funcB]) => {
      if (propsB.isSetup === propsA.isSetup) {
        return 0;
      }
      return propsB.isSetup ? 1 : -1;
    });
  }

  if (!_gTestIndex) {
    frontLoadSetups();
  }

  // For sane stacks during failures, we execute this code soon, but not now.
  // We do this now, before we call do_test_finished(), to ensure the pending
  // counter (_tests_pending) never reaches 0 while we still have tests to run
  // (executeSoon bumps that counter).
  executeSoon(_run_next_test, "run_next_test " + _gTestIndex);

  if (_gRunningTest !== null) {
    // Close the previous test do_test_pending call.
    do_test_finished(_gRunningTest.name);
  }
}

try {
  // Set global preferences
  if (runningInParent) {
    let prefsFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    prefsFile.initWithPath(_PREFS_FILE);
    _Services.prefs.readUserPrefsFromFile(prefsFile);
  }
} catch (e) {
  do_throw(e);
}

/**
 * Changing/Adding scalars or events to Telemetry is supported in build-faster/artifacts builds.
 * These need to be loaded explicitly at start.
 * It usually happens once all of Telemetry is initialized and set up.
 * However in xpcshell tests Telemetry is not necessarily fully loaded,
 * so we help out users by loading at least the dynamic-builtin probes.
 */
try {
  // We only need to run this in the parent process.
  // We only want to run this for local developer builds (which should have a "default" update channel).
  if (runningInParent && _AppConstants.MOZ_UPDATE_CHANNEL == "default") {
    let startTime = Cu.now();
    let { TelemetryController: _TelemetryController } =
      ChromeUtils.importESModule(
        "resource://gre/modules/TelemetryController.sys.mjs"
      );

    let complete = false;
    _TelemetryController.testRegisterJsProbes().finally(() => {
      ChromeUtils.addProfilerMarker(
        "xpcshell-test",
        { category: "Test", startTime },
        "TelemetryController.testRegisterJsProbes"
      );
      complete = true;
    });
    _Services.tm.spinEventLoopUntil(
      "Test(xpcshell/head.js:run_next-Test)",
      () => complete
    );
  }
} catch (e) {
  do_throw(e);
}

function _load_mozinfo() {
  let mozinfoFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  mozinfoFile.initWithPath(_MOZINFO_JS_PATH);
  let stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  stream.init(mozinfoFile, -1, 0, 0);
  let bytes = _NetUtil.readInputStream(stream, stream.available());
  let decoded = JSON.parse(new TextDecoder().decode(bytes));
  stream.close();
  return decoded;
}

Object.defineProperty(this, "mozinfo", {
  configurable: true,
  get() {
    let _mozinfo = _load_mozinfo();
    Object.defineProperty(this, "mozinfo", {
      configurable: false,
      value: _mozinfo,
    });
    return _mozinfo;
  },
});
