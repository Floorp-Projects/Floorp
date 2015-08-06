/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";
const { require } = Components.utils.import("resource://gre/modules/devtools/Loader.jsm", {});
const { JSPropertyProvider } = require("devtools/toolkit/webconsole/utils");

Components.utils.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

function run_test() {
  const testArray = 'var testArray = [\
    {propA: "A"},\
    {\
      propB: "B", \
      propC: [\
        {propD: "D"}\
      ]\
    },\
    [\
      {propE: "E"}\
    ]\
  ];'

  const testObject = 'var testObject = {"propA": [{"propB": "B"}]}';
  const testHyphenated = 'var testHyphenated = {"prop-A": "res-A"}';

  let sandbox = Components.utils.Sandbox("http://example.com");
  let dbg = new Debugger;
  let dbgObject = dbg.addDebuggee(sandbox);
  Components.utils.evalInSandbox(testArray, sandbox);
  Components.utils.evalInSandbox(testObject, sandbox);
  Components.utils.evalInSandbox(testHyphenated, sandbox);

  let results = JSPropertyProvider(dbgObject, null, "testArray[0].");
  do_print("Test that suggestions are given for 'foo[n]' where n is an integer.");
  test_has_result(results, "propA");

  do_print("Test that suggestions are given for multidimensional arrays.");
  results = JSPropertyProvider(dbgObject, null, "testArray[2][0].");
  test_has_result(results, "propE");

  do_print("Test that suggestions are not given for index that's out of bounds.");
  results = JSPropertyProvider(dbgObject, null, "testArray[10].");
  do_check_null(results);

  do_print("Test that no suggestions are given if an index is not numerical somewhere in the chain.");
  results = JSPropertyProvider(dbgObject, null, "testArray[0]['propC'][0].");
  do_check_null(results);

  results = JSPropertyProvider(dbgObject, null, "testObject['propA'][0].");
  do_check_null(results);

  results = JSPropertyProvider(dbgObject, null, "testArray[0]['propC'].");
  do_check_null(results);

  results = JSPropertyProvider(dbgObject, null, "testArray[][1].");
  do_check_null(results);

  do_print("Test that suggestions are not given if there is an hyphen in the chain.");
  results = JSPropertyProvider(dbgObject, null, "testHyphenated['prop-A'].");
  do_check_null(results);
}

/**
 * A helper that ensures (required) results were found.
 * @param Object aResults
 *        The results returned by JSPropertyProvider.
 * @param String aRequiredSuggestion
 *        A suggestion that must be found from the results.
 */
function test_has_result(aResults, aRequiredSuggestion) {
  do_check_neq(aResults, null);
  do_check_true(aResults.matches.length > 0);
  do_check_true(aResults.matches.indexOf(aRequiredSuggestion) !== -1);
}
