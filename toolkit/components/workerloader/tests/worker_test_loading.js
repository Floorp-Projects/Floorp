/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

importScripts("utils_worker.js"); // Test suite code
info("Test suite configured");

importScripts("resource://gre/modules/workers/require.js");
info("Loader imported");

var PATH = "chrome://mochitests/content/chrome/toolkit/components/workerloader/tests/";
var tests = [];
var add_test = function(test) {
  tests.push(test);
};

add_test(function test_setup() {
  ok(typeof require != "undefined", "Function |require| is defined");
});

// Test simple loading (moduleA-depends.js requires moduleB-dependency.js)
add_test(function test_load() {
  let A = require(PATH + "moduleA-depends.js");
  ok(true, "Opened module A");

  is(A.A, true, "Module A exported value A");
  ok(!("B" in A), "Module A did not export value B");
  is(A.importedFoo, "foo", "Module A re-exported B.foo");

  // re-evaluating moduleB-dependency.js would cause an error, but re-requiring it shouldn't
  let B = require(PATH + "moduleB-dependency.js");
  ok(true, "Managed to re-require module B");
  is(B.B, true, "Module B exported value B");
  is(B.foo, "foo", "Module B exported value foo");
});

// Test simple circular loading (moduleC-circular.js and moduleD-circular.js require each other)
add_test(function test_circular() {
  let C = require(PATH + "moduleC-circular.js");
  ok(true, "Loaded circular modules C and D");
  is(C.copiedFromD.copiedFromC.enteredC, true, "Properties exported by C before requiring D can be seen by D immediately");

  let D = require(PATH + "moduleD-circular.js");
  is(D.exportedFromC.finishedC, true, "Properties exported by C after requiring D can be seen by D eventually");
});

// Testing error cases
add_test(function test_exceptions() {
  let should_throw = function(f) {
    try {
      f();
      return null;
    } catch (ex) {
      return ex;
    }
  };

  let exn = should_throw(() => require(PATH + "this module doesn't exist"));
  ok(!!exn, "Attempting to load a module that doesn't exist raises an error");

  exn = should_throw(() => require(PATH + "moduleE-throws-during-require.js"));
  ok(!!exn, "Attempting to load a module that throws at toplevel raises an error");
  is(exn.moduleName, PATH + "moduleE-throws-during-require.js",
    "moduleName is correct");
  isnot(exn.moduleStack.indexOf("moduleE-throws-during-require.js"), -1,
    "moduleStack contains the name of the module");
  is(exn.lineNumber, 10, "The error comes with the right line number");

  exn = should_throw(() => require(PATH + "moduleF-syntaxerror.xml"));
  ok(!!exn, "Attempting to load a non-well formatted module raises an error");

  exn = should_throw(() => require(PATH + "moduleG-throws-later.js").doThrow());
  ok(!!exn, "G.doThrow() has raised an error");
  info(exn);
  ok(exn.toString().startsWith("TypeError"), "The exception is a TypeError.");
  is(exn.moduleName, PATH + "moduleG-throws-later.js", "The name of the module is correct");
  isnot(exn.moduleStack.indexOf("moduleG-throws-later.js"), -1,
    "The name of the right file appears somewhere in the stack");
  is(exn.lineNumber, 11, "The error comes with the right line number");
});

function get_exn(f) {
  try {
    f();
    return undefined;
  } catch (ex) {
    return ex;
  }
}

// Test module.exports
add_test(function test_module_dot_exports() {
  let H = require(PATH + "moduleH-module-dot-exports.js");
  is(H.key, "value", "module.exports worked");
  let H2 = require(PATH + "moduleH-module-dot-exports.js");
  is(H2.key, "value", "module.exports returned the same key");
  ok(H2 === H, "module.exports returned the same module the second time");
  let exn = get_exn(() => H.key = "this should not be accepted");
  ok(exn instanceof TypeError, "Cannot alter value in module.exports after export");
  exn = get_exn(() => H.key2 = "this should not be accepted, either");
  ok(exn instanceof TypeError, "Cannot add value to module.exports after export");
});

self.onmessage = function(message) {
  for (let test of tests) {
    info("Entering " + test.name);
    try {
      test();
    } catch (ex) {
      ok(false, "Test " + test.name + " failed");
      info(ex);
      info(ex.stack);
    }
    info("Leaving " + test.name);
  }
  finish();
};



