/* -*- Mode: JavaScript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file contains common code that is loaded before each test file(s).
 * See http://developer.mozilla.org/en/docs/Writing_xpcshell-based_unit_tests
 * for more information.
 */

const _XPCSHELL_TIMEOUT_MS = 5 * 60 * 1000;

var _quit = false;
var _passed = true;
var _tests_pending = 0;
var _passedChecks = 0, _falsePassedChecks = 0;
var _todoChecks = 0;
var _cleanupFunctions = [];
var _pendingTimers = [];
var _profileInitialized = false;

function _dump(str) {
  let start = /^TEST-/.test(str) ? "\n" : "";
  if (typeof _XPCSHELL_PROCESS == "undefined") {
    dump(start + str);
  } else {
    dump(start + _XPCSHELL_PROCESS + ": " + str);
  }
}

// Disable automatic network detection, so tests work correctly when
// not connected to a network.
let (ios = Components.classes["@mozilla.org/network/io-service;1"]
           .getService(Components.interfaces.nsIIOService2)) {
  ios.manageOfflineStatus = false;
  ios.offline = false;
}

// Determine if we're running on parent or child
let runningInParent = true;
try {
  runningInParent = Components.classes["@mozilla.org/xre/runtime;1"].
                    getService(Components.interfaces.nsIXULRuntime).processType
                    == Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
} 
catch (e) { }

try {
  if (runningInParent) {
    let prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);

    // disable necko IPC security checks for xpcshell, as they lack the
    // docshells needed to pass them
    prefs.setBoolPref("network.disable.ipc.security", true);

    // Disable IPv6 lookups for 'localhost' on windows.
    if ("@mozilla.org/windows-registry-key;1" in Components.classes) {
      prefs.setCharPref("network.dns.ipv4OnlyDomains", "localhost");
    }
  }
}
catch (e) { }

// Enable crash reporting, if possible
// We rely on the Python harness to set MOZ_CRASHREPORTER_NO_REPORT
// and handle checking for minidumps.
// Note that if we're in a child process, we don't want to init the
// crashreporter component.
try { // nsIXULRuntime is not available in some configurations.
  if (runningInParent &&
      "@mozilla.org/toolkit/crash-reporter;1" in Components.classes) {
    // Remember to update </toolkit/crashreporter/test/unit/test_crashreporter.js>
    // too if you change this initial setting.
    let (crashReporter =
          Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
          .getService(Components.interfaces.nsICrashReporter)) {
      crashReporter.enabled = true;
      crashReporter.minidumpPath = do_get_cwd();
    }
  }
}
catch (e) { }

/**
 * Date.now() is not necessarily monotonically increasing (insert sob story
 * about times not being the right tool to use for measuring intervals of time,
 * robarnold can tell all), so be wary of error by erring by at least
 * _timerFuzz ms.
 */
const _timerFuzz = 15;

function _Timer(func, delay) {
  delay = Number(delay);
  if (delay < 0)
    do_throw("do_timeout() delay must be nonnegative");

  if (typeof func !== "function")
    do_throw("string callbacks no longer accepted; use a function!");

  this._func = func;
  this._start = Date.now();
  this._delay = delay;

  var timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(this, delay + _timerFuzz, timer.TYPE_ONE_SHOT);

  // Keep timer alive until it fires
  _pendingTimers.push(timer);
}
_Timer.prototype = {
  QueryInterface: function(iid) {
    if (iid.Equals(Components.interfaces.nsITimerCallback) ||
        iid.Equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notify: function(timer) {
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
  }
};

function _do_main() {
  if (_quit)
    return;

  _dump("TEST-INFO | (xpcshell/head.js) | running event loop\n");

  var thr = Components.classes["@mozilla.org/thread-manager;1"]
                      .getService().currentThread;

  while (!_quit)
    thr.processNextEvent(true);

  while (thr.hasPendingEvents())
    thr.processNextEvent(true);
}

function _do_quit() {
  _dump("TEST-INFO | (xpcshell/head.js) | exiting test\n");

  _quit = true;
}

function _dump_exception_stack(stack) {
  stack.split("\n").forEach(function(frame) {
    if (!frame)
      return;
    // frame is of the form "fname(args)@file:line"
    let frame_regexp = new RegExp("(.*)\\(.*\\)@(.*):(\\d*)", "g");
    let parts = frame_regexp.exec(frame);
    if (parts)
        dump("JS frame :: " + parts[2] + " :: " + (parts[1] ? parts[1] : "anonymous")
             + " :: line " + parts[3] + "\n");
    else /* Could be a -e (command line string) style location. */
        dump("JS frame :: " + frame + "\n");
  });
}

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
_fakeIdleService = {
  get registrar() {
    delete this.registrar;
    return this.registrar =
      Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
  },
  contractID: "@mozilla.org/widget/idleservice;1",
  get CID() this.registrar.contractIDToCID(this.contractID),

  activate: function FIS_activate()
  {
    if (!this.originalFactory) {
      // Save original factory.
      this.originalFactory =
        Components.manager.getClassObject(Components.classes[this.contractID],
                                          Components.interfaces.nsIFactory);
      // Unregister original factory.
      this.registrar.unregisterFactory(this.CID, this.originalFactory);
      // Replace with the mock.
      this.registrar.registerFactory(this.CID, "Fake Idle Service",
                                     this.contractID, this.factory
      );
    }
  },

  deactivate: function FIS_deactivate()
  {
    if (this.originalFactory) {
      // Unregister the mock.
      this.registrar.unregisterFactory(this.CID, this.factory);
      // Restore original factory.
      this.registrar.registerFactory(this.CID, "Idle Service",
                                     this.contractID, this.originalFactory);
      delete this.originalFactory;
    }
  },

  factory: {
    // nsIFactory
    createInstance: function (aOuter, aIID)
    {
      if (aOuter) {
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      }
      return _fakeIdleService.QueryInterface(aIID);
    },
    lockFactory: function (aLock) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    },
    QueryInterface: function(aIID) {
      if (aIID.equals(Components.interfaces.nsIFactory) ||
          aIID.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  },

  // nsIIdleService
  get idleTime() 0,
  addIdleObserver: function () {},
  removeIdleObserver: function () {},

  QueryInterface: function(aIID) {
    // Useful for testing purposes, see test_get_idle.js.
    if (aIID.equals(Components.interfaces.nsIFactory)) {
      return this.factory;
    }
    if (aIID.equals(Components.interfaces.nsIIdleService) ||
        aIID.equals(Components.interfaces.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
}

/**
 * Restores the idle service factory if needed and returns the service's handle.
 * @return A handle to the idle service.
 */
function do_get_idle() {
  _fakeIdleService.deactivate();
  return Components.classes[_fakeIdleService.contractID]
                   .getService(Components.interfaces.nsIIdleService);
}

// Map resource://test/ to current working directory and
// resource://testing-common/ to the shared test modules directory.
function _register_protocol_handlers() {
  let (ios = Components.classes["@mozilla.org/network/io-service;1"]
             .getService(Components.interfaces.nsIIOService)) {
    let protocolHandler =
      ios.getProtocolHandler("resource")
         .QueryInterface(Components.interfaces.nsIResProtocolHandler);
    let curDirURI = ios.newFileURI(do_get_cwd());
    protocolHandler.setSubstitution("test", curDirURI);

    if (this._TESTING_MODULES_DIR) {
      let modulesFile = Components.classes["@mozilla.org/file/local;1"].
                        createInstance(Components.interfaces.nsILocalFile);
      modulesFile.initWithPath(_TESTING_MODULES_DIR);

      if (!modulesFile.exists()) {
        throw new Error("Specified modules directory does not exist: " +
                        _TESTING_MODULES_DIR);
      }

      if (!modulesFile.isDirectory()) {
        throw new Error("Specified modules directory is not a directory: " +
                        _TESTING_MODULES_DIR);
      }

      let modulesURI = ios.newFileURI(modulesFile);

      protocolHandler.setSubstitution("testing-common", modulesURI);
    }
  }
}

function _execute_test() {
  _register_protocol_handlers();

  // Override idle service by default.
  // Call do_get_idle() to restore the factory and get the service.
  _fakeIdleService.activate();

  // Terminate asynchronous tests when a global timeout occurs.
  do_timeout(_XPCSHELL_TIMEOUT_MS, function _do_main_timeout() {
    try {
      do_throw("test timed out");
    } catch (e if e == Components.results.NS_ERROR_ABORT) {
      // We don't want do_timeout to report the do_throw exception again.
    }
  });

  // _HEAD_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_HEAD_FILES);
  // _TEST_FILE is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TEST_FILE);

  try {
    do_test_pending("MAIN run_test");
    run_test();
    do_test_finished("MAIN run_test");
    _do_main();
  } catch (e) {
    _passed = false;
    // do_check failures are already logged and set _quit to true and throw
    // NS_ERROR_ABORT. If both of those are true it is likely this exception
    // has already been logged so there is no need to log it again. It's
    // possible that this will mask an NS_ERROR_ABORT that happens after a
    // do_check failure though.
    if (!_quit || e != Components.results.NS_ERROR_ABORT) {
      msg = "TEST-UNEXPECTED-FAIL | ";
      if (e.fileName) {
        msg += e.fileName;
        if (e.lineNumber) {
          msg += ":" + e.lineNumber;
        }
      } else {
        msg += "xpcshell/head.js";
      }
      msg += " | " + e;
      if (e.stack) {
        _dump(msg + " - See following stack:\n");
        _dump_exception_stack(e.stack);
      }
      else {
        _dump(msg + "\n");
      }
    }
  }

  // _TAIL_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TAIL_FILES);

  // Execute all of our cleanup functions.
  var func;
  while ((func = _cleanupFunctions.pop()))
    func();

  // Restore idle service to avoid leaks.
  _fakeIdleService.deactivate();

  if (!_passed)
    return;

  var truePassedChecks = _passedChecks - _falsePassedChecks;
  if (truePassedChecks > 0) {
    _dump("TEST-PASS | (xpcshell/head.js) | " + truePassedChecks + " (+ " +
            _falsePassedChecks + ") check(s) passed\n");
    _dump("TEST-INFO | (xpcshell/head.js) | " + _todoChecks +
            " check(s) todo\n");
  } else {
    // ToDo: switch to TEST-UNEXPECTED-FAIL when all tests have been updated. (Bug 496443)
    _dump("TEST-INFO | (xpcshell/head.js) | No (+ " + _falsePassedChecks + ") checks actually run\n");
  }
}

/**
 * Loads files.
 *
 * @param aFiles Array of files to load.
 */
function _load_files(aFiles) {
  function loadTailFile(element, index, array) {
    load(element);
  }

  aFiles.forEach(loadTailFile);
}


/************** Functions to be used from the tests **************/

/**
 * Prints a message to the output log.
 */
function do_print(msg) {
  var caller_stack = Components.stack.caller;
  _dump("TEST-INFO | " + caller_stack.filename + " | " + msg + "\n");
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

function do_execute_soon(callback, aName) {
  let funcName = (aName ? aName : callback.name);
  do_test_pending(funcName);
  var tm = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService(Components.interfaces.nsIThreadManager);

  tm.mainThread.dispatch({
    run: function() {
      try {
        callback();
      } catch (e) {
        // do_check failures are already logged and set _quit to true and throw
        // NS_ERROR_ABORT. If both of those are true it is likely this exception
        // has already been logged so there is no need to log it again. It's
        // possible that this will mask an NS_ERROR_ABORT that happens after a
        // do_check failure though.
        if (!_quit || e != Components.results.NS_ERROR_ABORT) {
          _dump("TEST-UNEXPECTED-FAIL | (xpcshell/head.js) | " + e);
          if (e.stack) {
            dump(" - See following stack:\n");
            _dump_exception_stack(e.stack);
          }
          else {
            dump("\n");
          }
          _do_quit();
        }
      }
      finally {
        do_test_finished(funcName);
      }
    }
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
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
  if (!stack) {
    if (error instanceof Error) {
      // |error| is an exception object
      filename = error.fileName;
      stack = error.stack;
    } else {
      stack = Components.stack.caller;
    }
  }

  if (stack instanceof Components.interfaces.nsIStackFrame)
    filename = stack.filename;

  _dump("TEST-UNEXPECTED-FAIL | " + filename + " | " + error +
        " - See following stack:\n");

  if (stack instanceof Components.interfaces.nsIStackFrame) {
    let frame = stack;
    while (frame != null) {
      _dump(frame + "\n");
      frame = frame.caller;
    }
  } else if (typeof stack == "string") {
    let stackLines = stack.split("\n");
    for (let line of stackLines) {
      _dump(line + "\n");
    }
  }

  _passed = false;
  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function do_throw_todo(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _passed = false;
  _dump("TEST-UNEXPECTED-PASS | " + stack.filename + " | " + text +
        " - See following stack:\n");
  var frame = Components.stack;
  while (frame != null) {
    _dump(frame + "\n");
    frame = frame.caller;
  }

  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function do_report_unexpected_exception(ex, text) {
  var caller_stack = Components.stack.caller;
  text = text ? text + " - " : "";

  _passed = false;
  _dump("TEST-UNEXPECTED-FAIL | " + caller_stack.filename + " | " + text +
        "Unexpected exception " + ex + ", see following stack:\n" + ex.stack +
        "\n");

  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function do_note_exception(ex, text) {
  var caller_stack = Components.stack.caller;
  text = text ? text + " - " : "";

  _dump("TEST-INFO | " + caller_stack.filename + " | " + text +
        "Swallowed exception " + ex + ", see following stack:\n" + ex.stack +
        "\n");
}

function _do_check_neq(left, right, stack, todo) {
  if (!stack)
    stack = Components.stack.caller;

  var text = left + " != " + right;
  if (left == right) {
    if (!todo) {
      do_throw(text, stack);
    } else {
      ++_todoChecks;
      _dump("TEST-KNOWN-FAIL | " + stack.filename + " | [" + stack.name +
            " : " + stack.lineNumber + "] " + text +"\n");
    }
  } else {
    if (!todo) {
      ++_passedChecks;
      _dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
            stack.lineNumber + "] " + text + "\n");
    } else {
      do_throw_todo(text, stack);
    }
  }
}

function do_check_neq(left, right, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _do_check_neq(left, right, stack, false);
}

function todo_check_neq(left, right, stack) {
  if (!stack)
      stack = Components.stack.caller;

  _do_check_neq(left, right, stack, true);
}

function do_report_result(passed, text, stack, todo) {
  if (passed) {
    if (todo) {
      do_throw_todo(text, stack);
    } else {
      ++_passedChecks;
      _dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
            stack.lineNumber + "] " + text + "\n");
    }
  } else {
    if (todo) {
      ++_todoChecks;
      _dump("TEST-KNOWN-FAIL | " + stack.filename + " | [" + stack.name +
            " : " + stack.lineNumber + "] " + text +"\n");
    } else {
      do_throw(text, stack);
    }
  }
}

function _do_check_eq(left, right, stack, todo) {
  if (!stack)
    stack = Components.stack.caller;

  var text = left + " == " + right;
  do_report_result(left == right, text, stack, todo);
}

function do_check_eq(left, right, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _do_check_eq(left, right, stack, false);
}

function todo_check_eq(left, right, stack) {
  if (!stack)
      stack = Components.stack.caller;

  _do_check_eq(left, right, stack, true);
}

function do_check_true(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  do_check_eq(condition, true, stack);
}

function todo_check_true(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  todo_check_eq(condition, true, stack);
}

function do_check_false(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  do_check_eq(condition, false, stack);
}

function todo_check_false(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  todo_check_eq(condition, false, stack);
}

function do_check_null(condition, stack=Components.stack.caller) {
  do_check_eq(condition, null, stack);
}

function todo_check_null(condition, stack=Components.stack.caller) {
  todo_check_eq(condition, null, stack);
}

/**
 * Check that |value| matches |pattern|.
 *
 * A |value| matches a pattern |pattern| if any one of the following is true:
 *
 * - |value| and |pattern| are both objects; |pattern|'s enumerable
 *   properties' values are valid patterns; and for each enumerable
 *   property |p| of |pattern|, plus 'length' if present at all, |value|
 *   has a property |p| whose value matches |pattern.p|. Note that if |j|
 *   has other properties not present in |p|, |j| may still match |p|.
 *
 * - |value| and |pattern| are equal string, numeric, or boolean literals
 *
 * - |pattern| is |undefined| (this is a wildcard pattern)
 *
 * - typeof |pattern| == "function", and |pattern(value)| is true.
 *
 * For example:
 *
 * do_check_matches({x:1}, {x:1})       // pass
 * do_check_matches({x:1}, {})          // fail: all pattern props required
 * do_check_matches({x:1}, {x:2})       // fail: values must match
 * do_check_matches({x:1}, {x:1, y:2})  // pass: extra props tolerated
 *
 * // Property order is irrelevant.
 * do_check_matches({x:"foo", y:"bar"}, {y:"bar", x:"foo"}) // pass
 *
 * do_check_matches({x:undefined}, {x:1}) // pass: 'undefined' is wildcard
 * do_check_matches({x:undefined}, {x:2})
 * do_check_matches({x:undefined}, {y:2}) // fail: 'x' must still be there
 *
 * // Patterns nest.
 * do_check_matches({a:1, b:{c:2,d:undefined}}, {a:1, b:{c:2,d:3}})
 *
 * // 'length' property counts, even if non-enumerable.
 * do_check_matches([3,4,5], [3,4,5])     // pass
 * do_check_matches([3,4,5], [3,5,5])     // fail; value doesn't match
 * do_check_matches([3,4,5], [3,4,5,6])   // fail; length doesn't match
 *
 * // functions in patterns get applied.
 * do_check_matches({foo:function (v) v.length == 2}, {foo:"hi"}) // pass
 * do_check_matches({foo:function (v) v.length == 2}, {bar:"hi"}) // fail
 * do_check_matches({foo:function (v) v.length == 2}, {foo:"hello"}) // fail
 *
 * // We don't check constructors, prototypes, or classes. However, if
 * // pattern has a 'length' property, we require values to match that as
 * // well, even if 'length' is non-enumerable in the pattern. So arrays
 * // are useful as patterns.
 * do_check_matches({0:0, 1:1, length:2}, [0,1])  // pass
 * do_check_matches({0:1}, [1,2])                 // pass
 * do_check_matches([0], {0:0, length:1})         // pass
 *
 * Notes:
 *
 * The 'length' hack gives us reasonably intuitive handling of arrays.
 *
 * This is not a tight pattern-matcher; it's only good for checking data
 * from well-behaved sources. For example:
 * - By default, we don't mind values having extra properties.
 * - We don't check for proxies or getters.
 * - We don't check the prototype chain.
 * However, if you know the values are, say, JSON, which is pretty
 * well-behaved, and if you want to tolerate additional properties
 * appearing on the JSON for backward-compatibility, then do_check_matches
 * is ideal. If you do want to be more careful, you can use function
 * patterns to implement more stringent checks.
 */
function do_check_matches(pattern, value, stack=Components.stack.caller, todo=false) {
  var matcher = pattern_matcher(pattern);
  var text = "VALUE: " + uneval(value) + "\nPATTERN: " + uneval(pattern) + "\n";
  var diagnosis = []
  if (matcher(value, diagnosis)) {
    do_report_result(true, "value matches pattern:\n" + text, stack, todo);
  } else {
    text = ("value doesn't match pattern:\n" +
            text +
            "DIAGNOSIS: " +
            format_pattern_match_failure(diagnosis[0]) + "\n");
    do_report_result(false, text, stack, todo);
  }
}

function todo_check_matches(pattern, value, stack=Components.stack.caller) {
  do_check_matches(pattern, value, stack, true);
}

// Return a pattern-matching function of one argument, |value|, that
// returns true if |value| matches |pattern|.
//
// If the pattern doesn't match, and the pattern-matching function was
// passed its optional |diagnosis| argument, the pattern-matching function
// sets |diagnosis|'s '0' property to a JSON-ish description of the portion
// of the pattern that didn't match, which can be formatted legibly by
// format_pattern_match_failure.
function pattern_matcher(pattern) {
  function explain(diagnosis, reason) {
    if (diagnosis) {
      diagnosis[0] = reason;
    }
    return false;
  }
  if (typeof pattern == "function") {
    return pattern;
  } else if (typeof pattern == "object" && pattern) {
    var matchers = [[p, pattern_matcher(pattern[p])] for (p in pattern)];
    // Kludge: include 'length', if not enumerable. (If it is enumerable,
    // we picked it up in the array comprehension, above.
    ld = Object.getOwnPropertyDescriptor(pattern, 'length');
    if (ld && !ld.enumerable) {
      matchers.push(['length', pattern_matcher(pattern.length)])
    }
    return function (value, diagnosis) {
      if (!(value && typeof value == "object")) {
        return explain(diagnosis, "value not object");
      }
      for (let [p, m] of matchers) {
        var element_diagnosis = [];
        if (!(p in value && m(value[p], element_diagnosis))) {
          return explain(diagnosis, { property:p,
                                      diagnosis:element_diagnosis[0] });
        }
      }
      return true;
    };
  } else if (pattern === undefined) {
    return function(value) { return true; };
  } else {
    return function (value, diagnosis) {
      if (value !== pattern) {
        return explain(diagnosis, "pattern " + uneval(pattern) + " not === to value " + uneval(value));
      }
      return true;
    };
  }
}

// Format an explanation for a pattern match failure, as stored in the
// second argument to a matching function.
function format_pattern_match_failure(diagnosis, indent="") {
  var a;
  if (!diagnosis) {
    a = "Matcher did not explain reason for mismatch.";
  } else if (typeof diagnosis == "string") {
    a = diagnosis;
  } else if (diagnosis.property) {
    a = "Property " + uneval(diagnosis.property) + " of object didn't match:\n";
    a += format_pattern_match_failure(diagnosis.diagnosis, indent + "  ");
  }
  return indent + a;
}

function do_test_pending(aName) {
  ++_tests_pending;

  _dump("TEST-INFO | (xpcshell/head.js) | test" + (aName ? " " + aName : "") +
         " pending (" + _tests_pending + ")\n");
}

function do_test_finished(aName) {
  _dump("TEST-INFO | (xpcshell/head.js) | test" + (aName ? " " + aName : "") +
         " finished (" + _tests_pending + ")\n");

  if (--_tests_pending == 0)
    _do_quit();
}

function do_get_file(path, allowNonexistent) {
  try {
    let lf = Components.classes["@mozilla.org/file/directory_service;1"]
      .getService(Components.interfaces.nsIProperties)
      .get("CurWorkD", Components.interfaces.nsILocalFile);

    let bits = path.split("/");
    for (let i = 0; i < bits.length; i++) {
      if (bits[i]) {
        if (bits[i] == "..")
          lf = lf.parent;
        else
          lf.append(bits[i]);
      }
    }

    if (!allowNonexistent && !lf.exists()) {
      // Not using do_throw(): caller will continue.
      _passed = false;
      var stack = Components.stack.caller;
      _dump("TEST-UNEXPECTED-FAIL | " + stack.filename + " | [" +
            stack.name + " : " + stack.lineNumber + "] " + lf.path +
            " does not exist\n");
    }

    return lf;
  }
  catch (ex) {
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
  const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;
  do_check_true(Components.manager instanceof nsIComponentRegistrar);
  // Previous do_check_true() is not a test check.
  ++_falsePassedChecks;
  Components.manager.autoRegister(lf);
}

/**
 * Parse a DOM document.
 *
 * @param aPath File path to the document.
 * @param aType Content type to use in DOMParser.
 *
 * @return nsIDOMDocument from the file.
 */
function do_parse_document(aPath, aType) {
  switch (aType) {
    case "application/xhtml+xml":
    case "application/xml":
    case "text/xml":
      break;

    default:
      do_throw("type: expected application/xhtml+xml, application/xml or text/xml," +
                 " got '" + aType + "'",
               Components.stack.caller);
  }

  var lf = do_get_file(aPath);
  const C_i = Components.interfaces;
  const parserClass = "@mozilla.org/xmlextras/domparser;1";
  const streamClass = "@mozilla.org/network/file-input-stream;1";
  var stream = Components.classes[streamClass]
                         .createInstance(C_i.nsIFileInputStream);
  stream.init(lf, -1, -1, C_i.nsIFileInputStream.CLOSE_ON_EOF);
  var parser = Components.classes[parserClass]
                         .createInstance(C_i.nsIDOMParser);
  var doc = parser.parseFromStream(stream, null, lf.fileSize, aType);
  parser = null;
  stream = null;
  lf = null;
  return doc;
}

/**
 * Registers a function that will run when the test harness is done running all
 * tests.
 *
 * @param aFunction
 *        The function to be called when the test harness has finished running.
 */
function do_register_cleanup(aFunction)
{
  _cleanupFunctions.push(aFunction);
}

/**
 * Registers a directory with the profile service,
 * and return the directory as an nsILocalFile.
 *
 * @return nsILocalFile of the profile directory.
 */
function do_get_profile() {
  if (!runningInParent) {
    _dump("TEST-INFO | (xpcshell/head.js) | Ignoring profile creation from child process.\n");
    return null;
  }

  if (!_profileInitialized) {
    // Since we have a profile, we will notify profile shutdown topics at
    // the end of the current test, to ensure correct cleanup on shutdown.
    do_register_cleanup(function() {
      let obsSvc = Components.classes["@mozilla.org/observer-service;1"].
                   getService(Components.interfaces.nsIObserverService);
      obsSvc.notifyObservers(null, "profile-change-net-teardown", null);
      obsSvc.notifyObservers(null, "profile-change-teardown", null);
      obsSvc.notifyObservers(null, "profile-before-change", null);
    });
  }

  let env = Components.classes["@mozilla.org/process/environment;1"]
                      .getService(Components.interfaces.nsIEnvironment);
  // the python harness sets this in the environment for us
  let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(profd);

  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == "ProfD" || prop == "ProfLD" || prop == "ProfDS" ||
          prop == "ProfLDS" || prop == "TmpD") {
        return file.clone();
      }
      throw Components.results.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsIDirectoryServiceProvider) ||
          iid.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Components.interfaces.nsIDirectoryService)
        .registerProvider(provider);

  let obsSvc = Components.classes["@mozilla.org/observer-service;1"].
        getService(Components.interfaces.nsIObserverService);

  if (!_profileInitialized) {
    obsSvc.notifyObservers(null, "profile-do-change", "xpcshell-do-get-profile");
    _profileInitialized = true;
  }

  // The methods of 'provider' will retain this scope so null out everything
  // to avoid spurious leak reports.
  env = null;
  profd = null;
  dirSvc = null;
  provider = null;
  obsSvc = null;
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
function do_load_child_test_harness()
{
  // Make sure this isn't called from child process
  if (!runningInParent) {
    do_throw("run_test_in_child cannot be called from child!");
  }

  // Allow to be called multiple times, but only run once
  if (typeof do_load_child_test_harness.alreadyRun != "undefined")
    return;
  do_load_child_test_harness.alreadyRun = 1;

  _XPCSHELL_PROCESS = "parent";

  let command =
        "const _HEAD_JS_PATH=" + uneval(_HEAD_JS_PATH) + "; "
      + "const _HTTPD_JS_PATH=" + uneval(_HTTPD_JS_PATH) + "; "
      + "const _HEAD_FILES=" + uneval(_HEAD_FILES) + "; "
      + "const _TAIL_FILES=" + uneval(_TAIL_FILES) + "; "
      + "const _XPCSHELL_PROCESS='child';";

  if (this._TESTING_MODULES_DIR) {
    command += " const _TESTING_MODULES_DIR=" + uneval(_TESTING_MODULES_DIR) + ";";
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
 */
function run_test_in_child(testFile, optionalCallback) 
{
  var callback = (typeof optionalCallback == 'undefined') ? 
                    do_test_finished : optionalCallback;

  do_load_child_test_harness();

  var testPath = do_get_file(testFile).path.replace(/\\/g, "/");
  do_test_pending("run in child");
  sendCommand("_dump('CHILD-TEST-STARTED'); "
              + "const _TEST_FILE=['" + testPath + "']; _execute_test(); "
              + "_dump('CHILD-TEST-COMPLETED');", 
              callback);
}


/**
 * Add a test function to the list of tests that are to be run asynchronously.
 *
 * Each test function must call run_next_test() when it's done. Test files
 * should call run_next_test() in their run_test function to execute all
 * async tests.
 *
 * @return the test function that was passed in.
 */
let _gTests = [];
function add_test(func) {
  _gTests.push([false, func]);
  return func;
}

// We lazy import Task.jsm so we don't incur a run-time penalty for all tests.
let _Task;

/**
 * Add a test function which is a Task function.
 *
 * Task functions are functions fed into Task.jsm's Task.spawn(). They are
 * generators that emit promises.
 *
 * If an exception is thrown, a do_check_* comparison fails, or if a rejected
 * promise is yielded, the test function aborts immediately and the test is
 * reported as a failure.
 *
 * Unlike add_test(), there is no need to call run_next_test(). The next test
 * will run automatically as soon the task function is exhausted. To trigger
 * premature (but successful) termination of the function, simply return or
 * throw a Task.Result instance.
 *
 * Example usage:
 *
 * add_task(function test() {
 *   let result = yield Promise.resolve(true);
 *
 *   do_check_true(result);
 *
 *   let secondary = yield someFunctionThatReturnsAPromise(result);
 *   do_check_eq(secondary, "expected value");
 * });
 *
 * add_task(function test_early_return() {
 *   let result = yield somethingThatReturnsAPromise();
 *
 *   if (!result) {
 *     // Test is ended immediately, with success.
 *     return;
 *   }
 *
 *   do_check_eq(result, "foo");
 * });
 */
function add_task(func) {
  if (!_Task) {
    let ns = {};
    _Task = Components.utils.import("resource://gre/modules/Task.jsm", ns).Task;
  }

  _gTests.push([true, func]);
}

/**
 * Runs the next test function from the list of async tests.
 */
let _gRunningTest = null;
let _gTestIndex = 0; // The index of the currently running test.
function run_next_test()
{
  function _run_next_test()
  {
    if (_gTestIndex < _gTests.length) {
      let _isTask;
      [_isTask, _gRunningTest] = _gTests[_gTestIndex++];
      print("TEST-INFO | " + _TEST_FILE + " | Starting " + _gRunningTest.name);
      do_test_pending(_gRunningTest.name);

      if (_isTask) {
        _Task.spawn(_gRunningTest)
             .then(run_next_test, do_report_unexpected_exception);
      } else {
        // Exceptions do not kill asynchronous tests, so they'll time out.
        try {
          _gRunningTest();
        } catch (e) {
          do_throw(e);
        }
      }
    }
  }

  // For sane stacks during failures, we execute this code soon, but not now.
  // We do this now, before we call do_test_finished(), to ensure the pending
  // counter (_tests_pending) never reaches 0 while we still have tests to run
  // (do_execute_soon bumps that counter).
  do_execute_soon(_run_next_test, "run_next_test " + _gTestIndex);

  if (_gRunningTest !== null) {
    // Close the previous test do_test_pending call.
    do_test_finished(_gRunningTest.name);
  }
}
