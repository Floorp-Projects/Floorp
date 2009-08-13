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
 
/**
 * Runs html5lib-formatted test cases in the browser. Requires SimpleTest.
 *
 * Define an array named parserDatFiles before loading this script,
 * and it will load each of those dat files into an array, then run
 * the test parser on each and run the tests by assigning the input
 * data to an iframe's url.
 *
 * Your test document should have an element with id "display" and
 * an iframe with id "testframe".
 */

function writeErrorSummary(input, expected, got, isTodo, description,
  expectedTokenizerOutput) {
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
    PRE("Input: " + input)
  );
  if (typeof(expectedTokenizerOutput) != "undefined") {
    appendChildNodes(
      $("display"), P("Expected raw tokenizer output: " +
        expectedTokenizerOutput)
    );
  }
  let expectedTitle = "Expected:";
  let outputTitle = "Output:";
  if (gJSCompatibilityMode) {
    outputTitle = "Gecko parser output:";
    expectedTitle = "JavaScript parser output:";
  }
  appendChildNodes(
    $("display"),
    PRE(expectedTitle + "\n|" + expected +"|", "\n-\n",
        outputTitle + "\n|" + got + "|\n\n"),
    HR()
  );
}

function checkTests(input, expected, errors, description, 
  expectedTokenizerOutput, testDocument) {
  // For fragment tests, the expected output will be empty, so skip
  // the test.
  if (expected.length > 1) {    
    var domAsString = docToTestOutput(testDocument);
    // It's possible we need to reorder attributes to get these to match
    if (expected == domAsString) {
      is(domAsString, expected, "HTML5 expected success. " + new Date());
    } else {
      var reorderedDOM = reorderToMatchExpected(domAsString, expected);
      if (!gJSCompatibilityMode && html5Exceptions[input]) {
        todo(reorderedDOM == expected, "HTML5 expected failure. " + new Date());
        writeErrorSummary(input, expected, reorderedDOM, true, description,
          expectedTokenizerOutput);
      } else {
        if (reorderedDOM != expected) {
          is(reorderedDOM, expected, "HTML5 unexpected failure. " + input + " " + new Date());
          writeErrorSummary(input, expected, reorderedDOM, false,
            description, expectedTokenizerOutput);
        } else {
          is(reorderedDOM, expected, "HTML5 expected success. " + new Date());
        }
      }
    }
  }    
}

/**
 * Control will bounce back and forth between nextTest() and the
 * event handler returned by makeTestChecker() until the 'testcases'
 * iterator is spent.
 */
function makeTestChecker(input, expected, errors, description, 
  expectedTokenizerOutput) {
  return function (e) {
    if (!gJSCompatibilityMode) {
      checkTests(input, expected, errors, description, 
        expectedTokenizerOutput, e.target.contentDocument);
      nextTest(e.target);
    }
    else {
      window.parseHtmlDocument(input, $("jsframe").contentDocument,
        function() {
          expected = docToTestOutput($("jsframe").contentDocument);
          checkTests(input, expected, errors, description, 
            expectedTokenizerOutput, e.target.contentDocument);
          nextTest(e.target);        
        }, null);      
    }
  }
}

var testcases;
function nextTest(testframe) {
  var test = 0;
  try {
    if (gTokenizerMode) {
      // For tokenizer tests, testcases.next() return an extra paramter,
      // the index of the test.  The dataURL points to a server-side js
      // that returns the test data.
      var [index, input, output, errors, description, expectedTokenizerOutput]
        = testcases.next();
      dataURL = "tokenizer_file_server.sjs?" + index + 
        "&" + gTokenizerTestFile;
    } else {
      var [input, output, errors, description, expectedTokenizerOutput] = 
        testcases.next();
      dataURL = "data:text/html;base64," + btoa(input);
    }
    testframe.onload = makeTestChecker(input, output, errors, description,
      expectedTokenizerOutput);
    testframe.src = dataURL;
  } catch (err if err instanceof StopIteration) {
    // restore the original value of html5.enable if it was changed earlier
    if (typeof(gOriginalHtml5Pref) == "boolean") {
      netscape.security.PrivilegeManager
                  .enablePrivilege("UniversalXPConnect");
      var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                  .getService(Components.interfaces.nsIPrefBranch);
      prefs.setBoolPref("html5.enable", gOriginalHtml5Pref);
    }
    SimpleTest.finish();
  }
}

var framesLoaded = [];
function frameLoaded(e) {
  framesLoaded.push(e.target);
  if (framesLoaded.length == parserDatFiles.length) {
    var tests = [scrapeText(ifr.contentDocument)
                 for each (ifr in framesLoaded)];
    testcases = test_parser(tests);    
    nextTest($("testframe"));

    //SimpleTest.finish();
  }
}

/**
 * Create an iframe for each dat file
 */
function makeIFrames() {
  // Set JavaScript compatibility mode if the function 
  // window.parseHtmlDocument exists.
  gJSCompatibilityMode = typeof(window.parseHtmlDocument) != "undefined";

  if (gTokenizerMode) {
    // For tokenizer tests, no additional frames need to be created.
    appendChildNodes($("display"), BR(), "Results: ", HR());
    testcases = test_parser(tokenizerTests["tests"]);    
    nextTest($("testframe"));
  }
  else {
    for each (var filename in parserDatFiles) {
      var datFrame = document.createElement("iframe");
      datFrame.onload = frameLoaded;
      datFrame.src = filename;
      $("display").appendChild(datFrame);
    }
    appendChildNodes($("display"), BR(), "Results: ", HR());
  }
}

// If gTokenizerMode is undefined, set it to false: this
// isn't a tokenizer test.
if (typeof(gTokenizerMode) == "undefined") {
  gTokenizerMode = false;
}

// Global variable indicating whether or not the tests should
// be run in JavaScript compatibility mode.
var gJSCompatibilityMode;

addLoadEvent(makeIFrames);
SimpleTest.waitForExplicitFinish();
