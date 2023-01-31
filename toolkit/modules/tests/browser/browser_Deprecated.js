/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_DEPRECATION_WARNINGS = "devtools.errorconsole.deprecation_warnings";

// Using this named functions to test deprecation and the properly logged
// callstacks.
function basicDeprecatedFunction() {
  Deprecated.warning("this method is deprecated.", "https://example.com");
  return true;
}

function deprecationFunctionBogusCallstack() {
  Deprecated.warning("this method is deprecated.", "https://example.com", {
    caller: {},
  });
  return true;
}

function deprecationFunctionCustomCallstack() {
  // Get the nsIStackFrame that will contain the name of this function.
  function getStack() {
    return Components.stack;
  }
  Deprecated.warning(
    "this method is deprecated.",
    "https://example.com",
    getStack()
  );
  return true;
}

var tests = [
  // Test deprecation warning without passing the callstack.
  {
    deprecatedFunction: basicDeprecatedFunction,
    expectedObservation(aMessage) {
      testAMessage(aMessage);
      ok(
        aMessage.indexOf("basicDeprecatedFunction") > 0,
        "Callstack is correctly logged."
      );
    },
  },
  // Test a reported error when URL to documentation is not passed.
  {
    deprecatedFunction() {
      Deprecated.warning("this method is deprecated.");
      return true;
    },
    expectedObservation(aMessage) {
      ok(
        aMessage.indexOf("must provide a URL") > 0,
        "Deprecation warning logged an empty URL argument."
      );
    },
  },
  // Test deprecation with a bogus callstack passed as an argument (it will be
  // replaced with the current call stack).
  {
    deprecatedFunction: deprecationFunctionBogusCallstack,
    expectedObservation(aMessage) {
      testAMessage(aMessage);
      ok(
        aMessage.indexOf("deprecationFunctionBogusCallstack") > 0,
        "Callstack is correctly logged."
      );
    },
  },
  // Test deprecation with a valid custom callstack passed as an argument.
  {
    deprecatedFunction: deprecationFunctionCustomCallstack,
    expectedObservation(aMessage) {
      testAMessage(aMessage);
      ok(
        aMessage.indexOf("deprecationFunctionCustomCallstack") > 0,
        "Callstack is correctly logged."
      );
    },
    // Set pref to true.
    logWarnings: true,
  },
];

// Test Console Message attributes.
function testAMessage(aMessage) {
  ok(
    aMessage.indexOf("DEPRECATION WARNING: this method is deprecated.") === 0,
    "Deprecation is correctly logged."
  );
  ok(aMessage.indexOf("https://example.com") > 0, "URL is correctly logged.");
}

add_task(async function test_setup() {
  Services.prefs.setBoolPref(PREF_DEPRECATION_WARNINGS, true);

  // Check if Deprecated is loaded.
  ok(Deprecated, "Deprecated object exists");
});

add_task(async function test_pref_enabled() {
  for (let [idx, test] of tests.entries()) {
    info("Running test #" + idx);

    let promiseObserved = TestUtils.consoleMessageObserved(subject => {
      let msg = subject.wrappedJSObject.arguments?.[0];
      return (
        msg.includes("DEPRECATION WARNING: ") ||
        msg.includes("must provide a URL")
      );
    });

    test.deprecatedFunction();

    let msg = await promiseObserved;

    test.expectedObservation(msg.wrappedJSObject.arguments?.[0]);
  }
});

add_task(async function test_pref_disabled() {
  // Deprecation warnings will be logged only when the preference is set.
  Services.prefs.setBoolPref(PREF_DEPRECATION_WARNINGS, false);

  let endFn = TestUtils.listenForConsoleMessages();
  basicDeprecatedFunction();

  let messages = await endFn();
  Assert.equal(messages.length, 0, "Should not have received any messages");
});
