/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The test js is shared between sandboxed (which has no SpecialPowers object)
// and content mochitests (where the |Components| object is accessible only as
// SpecialPowers.Components). Expose Components if necessary here to make things
// work everywhere.
//
// Even if the real |Components| doesn't exist, we might shim in a simple JS
// placebo for compat. An easy way to differentiate this from the real thing
// is whether the property is read-only or not.
{
  let c = Object.getOwnPropertyDescriptor(this, 'Components');
  if ((!c.value || c.writable) && typeof SpecialPowers === 'object')
    Components = SpecialPowers.wrap(SpecialPowers.Components);
}

/*
 * This file contains common code that is loaded before each test file(s).
 * See http://developer.mozilla.org/en/docs/Writing_xpcshell-based_unit_tests
 * for more information.
 */

var _quit = false;
var _tests_pending = 0;
var _pendingTimers = [];
var _cleanupFunctions = [];

function _dump(str) {
  let start = /^TEST-/.test(str) ? "\n" : "";
  dump(start + str);
}

// Disable automatic network detection, so tests work correctly when
// not connected to a network.
{
  let ios = Components.classes["@mozilla.org/network/io-service;1"]
             .getService(Components.interfaces.nsIIOService2);
  ios.manageOfflineStatus = false;
  ios.offline = false;
}

// Determine if we're running on parent or child
var runningInParent = true;
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
    let crashReporter =
          Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
          .getService(Components.interfaces.nsICrashReporter);
    crashReporter.enabled = true;
    crashReporter.minidumpPath = do_get_cwd();
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
    if (iid.equals(Components.interfaces.nsITimerCallback) ||
        iid.equals(Components.interfaces.nsISupports))
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

function do_execute_soon(callback) {
  do_test_pending();
  var tm = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService(Components.interfaces.nsIThreadManager);

  tm.dispatchToMainThread({
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
        do_test_finished();
      }
    }
  });
}

function do_throw(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _dump("TEST-UNEXPECTED-FAIL | " + stack.filename + " | " + text +
        " - See following stack:\n");
  var frame = Components.stack;
  while (frame != null) {
    _dump(frame + "\n");
    frame = frame.caller;
  }

  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function do_throw_todo(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

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
      _dump("TEST-KNOWN-FAIL | " + stack.filename + " | [" + stack.name +
            " : " + stack.lineNumber + "] " + text +"\n");
    }
  } else {
    if (!todo) {
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
      _dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
            stack.lineNumber + "] " + text + "\n");
    }
  } else {
    if (todo) {
      _dump("TEST-KNOWN-FAIL | " + stack.filename + " | [" + stack.name +
            " : " + stack.lineNumber + "] " + text +"\n");
    } else {
      do_throw(text, stack);
    }
  }
}

/**
 * Checks for a true condition, with a success message.
 */
function ok(condition, msg) {
  do_report_result(condition, msg, Components.stack.caller, false);
}

/**
 * Checks for a condition equality, with a success message.
 */
function is(left, right, msg) {
  do_report_result(left === right, "[ " + left + " === " + right + " ] " + msg,
    Components.stack.caller, false);
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
 * do_check_matches({foo:v => v.length == 2}, {foo:"hi"}) // pass
 * do_check_matches({foo:v => v.length == 2}, {bar:"hi"}) // fail
 * do_check_matches({foo:v => v.length == 2}, {foo:"hello"}) // fail
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
    var matchers = [];
    for (let p in pattern) {
      matchers.push([p, pattern_matcher(pattern[p])]);
    }
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

function do_test_pending() {
  ++_tests_pending;

  _dump("TEST-INFO | (xpcshell/head.js) | test " + _tests_pending +
         " pending\n");
}

function do_test_finished() {
  _dump("TEST-INFO | (xpcshell/head.js) | test " + _tests_pending +
         " finished\n");

  if (--_tests_pending == 0) {
    _do_execute_cleanup();
    _do_quit();
  }
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
  Components.manager.autoRegister(lf);
}

/**
 * Registers a function that will run when the test harness is done running all
 * tests.
 *
 * @param aFunction
 *        The function to be called when the test harness has finished running.
 */
function do_register_cleanup(func) {
  _dump("TEST-INFO | " + _TEST_FILE + " | " +
    (_gRunningTest ? _gRunningTest.name + " " : "") +
    "registering cleanup function.");

  _cleanupFunctions.push(func);
}

/**
 * Execute a function when the test harness is done running all tests.
 */
function _do_execute_cleanup() {
  let func;
  while ((func = _cleanupFunctions.pop())) {
    _dump("TEST-INFO | " + _TEST_FILE + " | executing cleanup function.");
    func();
  }
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
var _gTests = [];
function add_test(func) {
  _gTests.push([false, func]);
  return func;
}

// We lazy import Task.jsm so we don't incur a run-time penalty for all tests.
var _Task;

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
 * add_task(function* test() {
 *   let result = yield Promise.resolve(true);
 *
 *   do_check_true(result);
 *
 *   let secondary = yield someFunctionThatReturnsAPromise(result);
 *   do_check_eq(secondary, "expected value");
 * });
 *
 * add_task(function* test_early_return() {
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
var _gRunningTest = null;
var _gTestIndex = 0; // The index of the currently running test.
function run_next_test()
{
  function _run_next_test()
  {
    if (_gTestIndex < _gTests.length) {
      do_test_pending();
      let _isTask;
      [_isTask, _gRunningTest] = _gTests[_gTestIndex++];
      _dump("TEST-INFO | " + _TEST_FILE + " | Starting " + _gRunningTest.name);

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
  do_execute_soon(_run_next_test);

  if (_gRunningTest !== null) {
    // Close the previous test do_test_pending call.
    do_test_finished();
  }
}

/**
 * End of code adapted from xpcshell head.js
 */


/**
 * JavaBridge facilitates communication between Java and JS. See
 * JavascriptBridge.java for the corresponding JavascriptBridge and docs.
 */

function JavaBridge(obj) {

  this._EVENT_TYPE = "Robocop:JS";
  this._JAVA_EVENT_TYPE = "Robocop:Java";
  this._target = obj;
  // The number of replies needed to answer all outstanding sync calls.
  this._repliesNeeded = 0;
  this._EventDispatcher.registerListener(this, this._EVENT_TYPE);

  this._sendMessage("notify-loaded", []);
};

JavaBridge.prototype = {

  _Services: Components.utils.import(
    "resource://gre/modules/Services.jsm", {}).Services,

  _EventDispatcher: Components.utils.import(
    "resource://gre/modules/Messaging.jsm", {}).EventDispatcher.instance,

  _getArgs: function (args) {
    let out = {
      length: Math.max(0, args.length - 1),
    };
    for (let i = 1; i < args.length; i++) {
      out[i - 1] = args[i];
    }
    return out;
  },

  _sendMessage: function (innerType, args) {
    this._EventDispatcher.dispatch(this._JAVA_EVENT_TYPE, {
      innerType: innerType,
      method: args[0],
      args: this._getArgs(args),
    });
  },

  onEvent: function(event, message, callback) {
    if (typeof SpecialPowers === 'object') {
      message = SpecialPowers.wrap(message);
    }
    if (message.innerType === "sync-reply") {
      // Reply to our Javascript-to-Java sync call
      this._repliesNeeded--;
      return;
    }
    // Call the corresponding method on the target
    try {
      this._target[message.method].apply(this._target, message.args);
    } catch (e) {
      do_report_unexpected_exception(e, "Failed to call " + message.method);
    }
    if (message.innerType === "sync-call") {
      // Reply for sync message
      this._sendMessage("sync-reply", [message.method]);
    }
  },

  /**
   * Synchronously call a method in Java,
   * given the method name followed by a list of arguments.
   */
  syncCall: function (methodName /*, ... */) {
    this._sendMessage("sync-call", arguments);
    let thread = this._Services.tm.currentThread;
    let initialReplies = this._repliesNeeded;
    // Need one more reply to answer the current sync call.
    this._repliesNeeded++;
    // Wait for the reply to arrive. Normally we would not want to
    // spin the event loop, but here we're in a test and our API
    // specifies a synchronous call, so we spin the loop to wait for
    // the call to finish.
    this._Services.tm.spinEventLoopUntil(() => this._repliesNeeded <= initialReplies);
  },

  /**
   * Asynchronously call a method in Java,
   * given the method name followed by a list of arguments.
   */
  asyncCall: function (methodName /*, ... */) {
    this._sendMessage("async-call", arguments);
  },

  /**
   * Disconnect with Java.
   */
  disconnect: function () {
    this._EventDispatcher.unregisterListener(this, this._EVENT_TYPE);
  },
};
