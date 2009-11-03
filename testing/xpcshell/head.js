/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Boris Zbarsky <bzbarsky@mit.edu>
 *  Jeff Walden <jwalden+code@mit.edu>
 *  Serge Gautherie <sgautherie.bz@free.fr>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains common code that is loaded before each test file(s).
 * See http://developer.mozilla.org/en/docs/Writing_xpcshell-based_unit_tests
 * for more information.
 */

var _quit = false;
var _passed = true;
var _tests_pending = 0;
var _passedChecks = 0, _falsePassedChecks = 0;
var _cleanupFunctions = [];
var _pendingCallbacks = [];

// Disable automatic network detection, so tests work correctly when
// not connected to a network.
let (ios = Components.classes["@mozilla.org/network/io-service;1"]
           .getService(Components.interfaces.nsIIOService2)) {
  ios.manageOfflineStatus = false;
  ios.offline = false;
}

// Enable crash reporting, if possible
// We rely on the Python harness to set MOZ_CRASHREPORTER_NO_REPORT
// and handle checking for minidumps.
if ("@mozilla.org/toolkit/crash-reporter;1" in Components.classes) {
  // Remember to update </toolkit/crashreporter/test/unit/test_crashreporter.js>
  // too if you change this initial setting.
  let (crashReporter =
        Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
        .getService(Components.interfaces.nsICrashReporter)) {
    crashReporter.enabled = true;
    crashReporter.minidumpPath = do_get_cwd();
  }
}


function _TimerCallback(expr, timer) {
  this._expr = expr;
  // Keep timer alive until it fires
  _pendingCallbacks.push(timer);
}
_TimerCallback.prototype = {
  _expr: "",

  QueryInterface: function(iid) {
    if (iid.Equals(Components.interfaces.nsITimerCallback) ||
        iid.Equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notify: function(timer) {
    _pendingCallbacks.splice(_pendingCallbacks.indexOf(timer), 1);
    eval(this._expr);
  }
};

function _do_main() {
  if (_quit)
    return;

  dump("TEST-INFO | (xpcshell/head.js) | running event loop\n");

  var thr = Components.classes["@mozilla.org/thread-manager;1"]
                      .getService().currentThread;

  while (!_quit)
    thr.processNextEvent(true);

  while (thr.hasPendingEvents())
    thr.processNextEvent(true);
}

function _do_quit() {
  dump("TEST-INFO | (xpcshell/head.js) | exiting test\n");

  _quit = true;
}

function _execute_test() {
  // _HEAD_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_HEAD_FILES);
  // _TEST_FILE is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TEST_FILE);

  try {
    do_test_pending();
    run_test();
    do_test_finished();
    _do_main();
  } catch (e) {
    _passed = false;
    // Print exception, but not do_throw() result.
    // Hopefully, this won't mask other NS_ERROR_ABORTs.
    if (!_quit || e != Components.results.NS_ERROR_ABORT)
      dump("TEST-UNEXPECTED-FAIL | (xpcshell/head.js) | " + e + "\n");
  }

  // _TAIL_FILES is dynamically defined by <runxpcshelltests.py>.
  _load_files(_TAIL_FILES);

  // Execute all of our cleanup functions.
  var func;
  while ((func = _cleanupFunctions.pop()))
    func();

  if (!_passed)
    return;

  var truePassedChecks = _passedChecks - _falsePassedChecks;
  if (truePassedChecks > 0)
    dump("TEST-PASS | (xpcshell/head.js) | " + truePassedChecks + " (+ " +
            _falsePassedChecks + ") check(s) passed\n");
  else
    // ToDo: switch to TEST-UNEXPECTED-FAIL when all tests have been updated. (Bug 496443)
    dump("TEST-INFO | (xpcshell/head.js) | No (+ " + _falsePassedChecks + ") checks actually run\n");
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


function do_timeout(delay, expr) {
  var timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);
  timer.initWithCallback(new _TimerCallback(expr, timer), delay, timer.TYPE_ONE_SHOT);
}

function do_execute_soon(callback) {
  var tm = Components.classes["@mozilla.org/thread-manager;1"]
                     .getService(Components.interfaces.nsIThreadManager);

  tm.mainThread.dispatch({
    run: function() {
      callback();
    }
  }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
}

function do_throw(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  _passed = false;
  dump("TEST-UNEXPECTED-FAIL | " + stack.filename + " | " + text +
         " - See following stack:\n");
  var frame = Components.stack;
  while (frame != null) {
    dump(frame + "\n");
    frame = frame.caller;
  }

  _do_quit();
  throw Components.results.NS_ERROR_ABORT;
}

function do_check_neq(left, right, stack) {
  if (!stack)
    stack = Components.stack.caller;

  var text = left + " != " + right;
  if (left == right)
    do_throw(text, stack);
  else {
    ++_passedChecks;
    dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
         stack.lineNumber + "] " + text + "\n");
  }
}

function do_check_eq(left, right, stack) {
  if (!stack)
    stack = Components.stack.caller;

  var text = left + " == " + right;
  if (left != right)
    do_throw(text, stack);
  else {
    ++_passedChecks;
    dump("TEST-PASS | " + stack.filename + " | [" + stack.name + " : " +
         stack.lineNumber + "] " + text + "\n");
  }
}

function do_check_true(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  do_check_eq(condition, true, stack);
}

function do_check_false(condition, stack) {
  if (!stack)
    stack = Components.stack.caller;

  do_check_eq(condition, false, stack);
}

function do_test_pending() {
  ++_tests_pending;

  dump("TEST-INFO | (xpcshell/head.js) | test " + _tests_pending +
         " pending\n");
}

function do_test_finished() {
  dump("TEST-INFO | (xpcshell/head.js) | test " + _tests_pending +
         " finished\n");

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
      dump("TEST-UNEXPECTED-FAIL | " + stack.filename + " | [" +
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

/**
 * Loads _HTTPD_JS_PATH file, which is dynamically defined by
 * <runxpcshelltests.py>.
 */
function do_load_httpd_js() {
  load(_HTTPD_JS_PATH);
}

function do_load_module(path) {
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
      if (prop == "ProfD" || prop == "ProfLD" || prop == "ProfDS") {
        return file.clone();
      }
      throw Components.results.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsIDirectoryProvider) ||
          iid.equals(Components.interfaces.nsISupports)) {
        return this;
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Components.interfaces.nsIDirectoryService)
        .registerProvider(provider);
  return file.clone();
}
