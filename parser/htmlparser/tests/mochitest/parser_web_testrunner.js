/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Sayre <sayrer@gmail.com>
 *   Jonathan Griffin <jgriffin@mozilla.com>
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
 * ***** END LICENSE BLOCK *****/
 
const MODE_PARSER = 1;
const MODE_TOKENIZER = 2;
const MODE_JSCOMPARE = 3;

/**********************************************************************
 * The ParserTestRunner class.  This is the base class for running all
 * supported types (tree construction, tokenizer, and js comparison) of
 * html5lib-formatted HTML5 parser tests.  Subclasses exist for tokenizer 
 * and js comparison tests, which override some methods of the base class.
 *
 * To use this with tree construction tests:
 *   Define an array named parserDatFiles which contain the names of the
 *   .dat files to be tested.  Create an instance of the ParserTestRunner()
 *   class, and call the startTest() method on that instance.
 * To use this with js comparison tests:
 *   Define an array named parserDatFiles which contain the names of the
 *   .dat files to be tested.  Create an instance of the JSCompareTestRunner()
 *   class, and call the startTest() method on that instance.  You must
 *   load the nu.validator JS parser before starting the test.
 * To use this with tokenizer tests:
 *   Before loading this script, load a script containing a JSON object
 *   named tokenizerTests, which contains the tests to be run.  Create
 *   an instance of the TokenizerTestRunner() class, and pass the filename
 *   of the script containing the tokenizerTests object to the constructor.
 *   Call the startTest() method on that instance to start the test.
 *
 * In all cases, your test document should have an element with id "display"
 * and an iframe with id "testframe".
 */
function ParserTestRunner() {
  this.mode = MODE_PARSER;
}

/**
 * For failed tests, write an error summary to the main document, in 
 * order to make it easier to work with them.
 */
ParserTestRunner.prototype.writeErrorSummary = function (input, 
  expected, got, isTodo, description, expectedTokenizerOutput) {
  if (!isTodo) {
    appendChildNodes($("display"), H2("Unexpected Failure:"));
  }
  appendChildNodes(
    $("display"), BR(),
    SPAN("Matched: "), "" + (expected == got)
  );
  if (typeof(description) != "undefined") {
    appendChildNodes(
      $("display"), P("Description: " + description)
    );
  }
  appendChildNodes(
    $("display"), 
    PRE("Input: " + JSON.stringify(input))
  );
  if (typeof(expectedTokenizerOutput) != "undefined") {
    appendChildNodes(
      $("display"), P("Expected raw tokenizer output: " +
        expectedTokenizerOutput)
    );
  }
  let expectedTitle = this.mode == MODE_JSCOMPARE ? 
    "JavaScript parser output:" : "Expected:";
  let outputTitle = this.mode == MODE_JSCOMPARE ? 
    "Gecko parser output:" : "Output:";
  appendChildNodes(
    $("display"),
    PRE(expectedTitle + "\n|" + expected +"|", "\n-\n",
        outputTitle + "\n|" + got + "|\n\n"),
    HR()
  );
}

/**
 * Restore the original value of the "html5.enable" pref and
 * let SimpleTest the test is finished.
 */
ParserTestRunner.prototype.finishTest = function() {
  if (typeof(this.originalHtml5Pref) == "boolean") {
    netscape.security.PrivilegeManager
                .enablePrivilege("UniversalXPConnect");
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
    prefs.setBoolPref("html5.enable", this.originalHtml5Pref);
  }
  SimpleTest.finish();
}

/**
 * Set the "html5.enable" pref to true, and store the original
 * value of the pref to be restored at the end of the test.
 */
ParserTestRunner.prototype.enableHTML5Parser = function() {
  netscape.security.PrivilegeManager.
           enablePrivilege("UniversalXPConnect");
  var prefs = Components.classes["@mozilla.org/preferences-service;1"]
           .getService(Components.interfaces.nsIPrefBranch);
  this.originalHtml5Pref = prefs.getBoolPref("html5.enable");
  prefs.setBoolPref("html5.enable", true);
}

/**
 * Returns true if the input is in the exception list of tests
 * known to fail.
 *
 * @param the 'input' field of the test to check
 */
ParserTestRunner.prototype.inTodoList = function(input) {
  return html5TreeConstructionExceptions[input];
}

/**
 * Verify the test, by comparing the DOM in the test document with a
 * reference version.  For MODE_PARSER tests, mark failed tests
 * as "todo" if they appear in the html5Exceptions list.
 *
 * Where the "reference version" comes from is dependent on the test
 * type; for MODE_PARSER and MODE_TOKENIZER, it is generated based
 * on the expected output which appears in the test file.  For
 * MODE_JSCOMPARE, it is the DOM of another iframe which was 
 * populated by the nu.validator JS parser.
 *
 * @param input        a string containing the test input
 * @param expected     a string containing the expected test output
 * @param errors       a string containing expected errors that the test
 *                     will induce in the parser; currently unused
 * @param description  a string description of the test
 * @param expectedTokenizerOutput for tokenizer tests, an array 
 *                     containing the expected output, otherwise 
 *                     undefined
 * @param testDocument the document property of the testframe that
 *                     contains the DOM under test
 */
ParserTestRunner.prototype.checkTests = function (input, expected, 
  errors, description, expectedTokenizerOutput, testDocument) {
  // For fragment tests, the expected output will be empty, so skip
  // the test.
  if (expected.length > 1) {
    var domAsString = docToTestOutput(testDocument, this.mode);
    if (expected == domAsString) {
      is(domAsString, expected, "HTML5 expected success. " + new Date());
    } else {
      if (this.inTodoList(input)) {
        todo(domAsString == expected, 
          "HTML5 expected failure. " + new Date());
        this.writeErrorSummary(input, expected, domAsString, 
          true, description, expectedTokenizerOutput);
      } else {
        if (domAsString != expected) {
          is(domAsString, expected, 
            "HTML5 unexpected failure. " + input + " " + new Date());
          this.writeErrorSummary(input, expected, domAsString, false,
            description, expectedTokenizerOutput);
        } else {
          is(domAsString, expected, "HTML5 expected success. " + new Date());
        }
      }
    }
  }
}

/**
 * Call the test checker to verify the current test, then
 * go to the next test.
 *
 * Control will bounce back and forth between nextTest() and
 * parseTests() until the 'testcases' iterator is spent.
 */
ParserTestRunner.prototype.makeTestChecker = function (input, 
  expected, errors, description, expectedTokenizerOutput,
  testframe) {
  this.checkTests(input, expected, errors, description, 
    expectedTokenizerOutput, testframe.contentDocument);
  this.nextTest(testframe);
}

/**
 * A generator function that accepts a list of tests. Each list
 * member corresponds to the contents of a ".dat" file from the
 * html5lib test suite.
 *
 * @param the list of strings
 */
ParserTestRunner.prototype.parseTests = function(testlist) {
  for each (var testgroup in testlist) {
    var tests = testgroup.split("#data\n");
    tests = ["#data\n" + test for each(test in tests) if (test)];
    for each (var test in tests) {
      yield parseTestcase(test);
    }
  }
}

/**
 * Parse the next test, then set the src attribute of the testframe
 * to a data: url containing the base64-encoded value of the test input.
 *
 * @param the iframe used for the test content
 */
ParserTestRunner.prototype.nextTest = function(testframe) {
  try {
    var [input, output, errors, description, expectedTokenizerOutput] = 
      this.testcases.next();
    dataURL = "data:text/html;base64," + btoa(input);
    var me = this;
    testframe.onload = function(e) {
      me.makeTestChecker.call(me, input, output, errors, 
        description, expectedTokenizerOutput, e.target);
    };
    testframe.src = dataURL;
  } catch (err if err instanceof StopIteration) {
    this.finishTest();
  }
}

/**
 * Create an iframe for each .dat file.  Once all the
 * iframes are loaded, concatenate their content into
 * one large string, pass that to the test parser, and
 * begin interating through the tests.
 */
ParserTestRunner.prototype.makeIFrames = function () {
  var framesLoaded = [];
  var me = this;
  for each (var filename in parserDatFiles) {
    var datFrame = document.createElement("iframe");
    datFrame.addEventListener("load", function(e) {
      framesLoaded.push(e.target);
      if (framesLoaded.length == parserDatFiles.length) {
        var tests = [scrapeText(ifr.contentDocument)
                     for each (ifr in framesLoaded)];
        me.testcases = me.parseTests(tests);    
        me.nextTest($("testframe"));
      }
    }, false);
    datFrame.src = filename;
    $("display").appendChild(datFrame);
  }
  appendChildNodes($("display"), BR(), "Results: ", HR());
}

/**
 * Start testing as soon as the onload event is fired.
 */
ParserTestRunner.prototype.startTest = function () {
  SimpleTest.waitForExplicitFinish();
  this.enableHTML5Parser();
  var me = this;
  var onLoadHandler = function (e) { me.makeIFrames.apply(me); }
  addLoadEvent(onLoadHandler);
};

/**********************************************************************
 * The TokenizerTestRunner() class, which inherits from ParserTestRunner().
 */
function TokenizerTestRunner(filename) {
  this.mode = MODE_TOKENIZER;
  this.filename = filename;
}
TokenizerTestRunner.prototype = new ParserTestRunner();
TokenizerTestRunner.prototype.constructor = TokenizerTestRunner; 

/**
 * Returns true if the input is in the exception list of tests
 * known to fail.
 *
 * @param the 'input' field of the test to check
 */
TokenizerTestRunner.prototype.inTodoList = function(input) {
  return html5TokenizerExceptions[input];
}

/**
 * Parse the next test, then set the src attribute of the testframe
 * to a url which will be served by a server-side javascript;
 * the .sjs will return a document containing the test input.
 *
 * Note that tokenizer tests can't use the same method as the other
 * parser tests for loading the test input into the iframe,
 * as some tokenizer test input contains invalid unicode characters,
 * which are impossible to express in a base64-encoded data: url.
 *
 * @param the iframe used for the test content
 */
TokenizerTestRunner.prototype.nextTest = function(testframe) {
  try {
    var [index, input, expected, errors, description, expectedTokenizerOutput]
      = this.testcases.next();
    let dataURL = "tokenizer_file_server.sjs?" + index + 
      "&" + this.filename;
    var me = this;
    testframe.onload = function(e) {
      me.makeTestChecker.call(me, input, expected, errors, 
        description, expectedTokenizerOutput, e.target);
    };
    testframe.src = dataURL;
  } catch (err if err instanceof StopIteration) {
    this.finishTest();
  }
}

/**
 * A generator function that iterates through a list of
 * tokenizer tests which have been decoded from JSON
 * .test files.
 *
 * @param A decoded JSON object containing a series of tests.
 */
TokenizerTestRunner.prototype.parseTests = function(jsonTestList) {
  var index = 1;
  for each (var test in jsonTestList) {
    var tmpArray = [index];
    yield tmpArray.concat(parseJsonTestcase(test));
    index++;
  }
}

/**
 * Pass the JSON test object to the test parser, and begin
 * interating through the tests.
 */
TokenizerTestRunner.prototype.startTestParser = function() {
  appendChildNodes($("display"), BR(), "Results: ", HR());
  this.testcases = this.parseTests(tokenizerTests["tests"]);    
  this.nextTest($("testframe"));
}

/**
 * Start testing as soon as the onload event is fired.
 */
TokenizerTestRunner.prototype.startTest = function () {
  SimpleTest.waitForExplicitFinish();
  this.enableHTML5Parser();
  var me = this;
  var onLoadHandler = function (e) { me.startTestParser.apply(me); }
  addLoadEvent(onLoadHandler);
}

/**********************************************************************
 * The JSCompareTestRunner() class, which inherits from ParserTestRunner().
 *
 * This class compares output of the Gecko HTML parser with that of the
 * JavaScript nu.validator parser.
 */
function JSCompareTestRunner() {
  this.mode = MODE_JSCOMPARE;
}
JSCompareTestRunner.prototype = new ParserTestRunner();
JSCompareTestRunner.prototype.constructor = JSCompareTestRunner;

/**
 * Call the test checker to verify the current test, then
 * go to the next test.
 *
 * Control will bounce back and forth between nextTest() and
 * parseTests() until the 'testcases' iterator is spent.
 */
JSCompareTestRunner.prototype.makeTestChecker = function (input, 
  expected, errors, description, expectedTokenizerOutput,
  testframe) {
  var me = this;
  window.parseHtmlDocument(input, $("jsframe").contentDocument,
    function() {
      expected = docToTestOutput($("jsframe").contentDocument,
        me.mode);
      me.checkTests(input, expected, errors, description, 
        expectedTokenizerOutput, testframe.contentDocument);
      me.nextTest(testframe);
    }, null);
}

/**
 * Returns true if the input is in the exception list of tests
 * known to fail.
 *
 * @param the 'input' field of the test to check
 */
JSCompareTestRunner.prototype.inTodoList = function(input) {
  return html5JSCompareExceptions[input];
}
