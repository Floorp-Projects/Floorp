/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cu = Components.utils;
const PREF_DEPRECATION_WARNINGS = "devtools.errorconsole.deprecation_warnings";

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Deprecated.jsm", this);

// Using this named functions to test deprecation and the properly logged
// callstacks.
function basicDeprecatedFunction() {
  Deprecated.warning("this method is deprecated.", "http://example.com");
  return true;
}

function deprecationFunctionBogusCallstack() {
  Deprecated.warning("this method is deprecated.", "http://example.com", {
    caller: {}
  });
  return true;
}

function deprecationFunctionCustomCallstack() {
  // Get the nsIStackFrame that will contain the name of this function.
  function getStack() {
    return Components.stack;
  }
  Deprecated.warning("this method is deprecated.", "http://example.com",
    getStack());
  return true;
}

var tests = [
// Test deprecation warning without passing the callstack.
{
  deprecatedFunction: basicDeprecatedFunction,
  expectedObservation(aMessage) {
    testAMessage(aMessage);
    ok(aMessage.errorMessage.indexOf("basicDeprecatedFunction") > 0,
      "Callstack is correctly logged.");
  }
},
// Test a reported error when URL to documentation is not passed.
{
  deprecatedFunction() {
    Deprecated.warning("this method is deprecated.");
    return true;
  },
  expectedObservation(aMessage) {
    ok(aMessage.errorMessage.indexOf("must provide a URL") > 0,
      "Deprecation warning logged an empty URL argument.");
  }
},
// Test deprecation with a bogus callstack passed as an argument (it will be
// replaced with the current call stack).
{
  deprecatedFunction: deprecationFunctionBogusCallstack,
  expectedObservation(aMessage) {
    testAMessage(aMessage);
    ok(aMessage.errorMessage.indexOf("deprecationFunctionBogusCallstack") > 0,
      "Callstack is correctly logged.");
  }
},
// When pref is unset Deprecated.warning should not log anything.
{
  deprecatedFunction: basicDeprecatedFunction,
  expectedObservation: null,
  // Set pref to false.
  logWarnings: false
},
// Test deprecation with a valid custom callstack passed as an argument.
{
  deprecatedFunction: deprecationFunctionCustomCallstack,
  expectedObservation(aMessage) {
    testAMessage(aMessage);
    ok(aMessage.errorMessage.indexOf("deprecationFunctionCustomCallstack") > 0,
      "Callstack is correctly logged.");
  },
  // Set pref to true.
  logWarnings: true
}];

// Which test are we running now?
var idx = -1;

function test() {
  waitForExplicitFinish();

  // Check if Deprecated is loaded.
  ok(Deprecated, "Deprecated object exists");

  nextTest();
}

// Test Consle Message attributes.
function testAMessage(aMessage) {
  ok(aMessage.errorMessage.indexOf("DEPRECATION WARNING: " +
    "this method is deprecated.") === 0,
    "Deprecation is correctly logged.");
  ok(aMessage.errorMessage.indexOf("http://example.com") > 0,
    "URL is correctly logged.");
}

function nextTest() {
  idx++;

  if (idx == tests.length) {
    finish();
    return;
  }

  info("Running test #" + idx);
  let test = tests[idx];

  // Deprecation warnings will be logged only when the preference is set.
  if (typeof test.logWarnings !== "undefined") {
    Services.prefs.setBoolPref(PREF_DEPRECATION_WARNINGS, test.logWarnings);
  }

  // Create a console listener.
  let consoleListener = {
    observe(aMessage) {
      // Ignore unexpected messages.
      if (!(aMessage instanceof Ci.nsIScriptError)) {
        return;
      }
      if (aMessage.errorMessage.indexOf("DEPRECATION WARNING: ") < 0 &&
          aMessage.errorMessage.indexOf("must provide a URL") < 0) {
        return;
      }
      ok(aMessage instanceof Ci.nsIScriptError,
        "Deprecation log message is an instance of type nsIScriptError.");


      if (test.expectedObservation === null) {
        ok(false, "Deprecated warning not expected");
      } else {
        test.expectedObservation(aMessage);
      }

      Services.console.unregisterListener(consoleListener);
      executeSoon(nextTest);
    }
  };
  Services.console.registerListener(consoleListener);
  test.deprecatedFunction();
  if (test.expectedObservation === null) {
    executeSoon(function() {
      Services.console.unregisterListener(consoleListener);
      executeSoon(nextTest);
    });
  }
}
