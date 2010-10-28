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
 *   Henri Sivonen <hsivonen@iki.fi>
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

var functionsToRunAsync = [];

window.addEventListener("message", function(event) {
  if (event.source == window && event.data == "async-run") {
    event.stopPropagation();
    var fn = functionsToRunAsync.shift();
    fn();
  }
}, true);

function asyncRun(fn) {
  functionsToRunAsync.push(fn);
  window.postMessage("async-run", "*");
}

function writeErrorSummary(input, expected, got, isTodo) {
  if (isTodo) {
    appendChildNodes($("display"), H2("Unexpected Success:"));
  } else {
    appendChildNodes($("display"), H2("Unexpected Failure:"));
  }
  appendChildNodes(
    $("display"), BR(),
    SPAN("Matched: "), "" + (expected == got),
    PRE("Input: \n" + input, "\n-\n",
        "Expected:\n" + expected, "\n-\n",
        "Output:\n" + got + "\n-\n"),
    HR()
  );
}

/**
 * Control will bounce back and forth between nextTest() and the
 * event handler returned by makeTestChecker() or the callback returned by
 * makeFragmentTestChecker() until the 'testcases' iterator is spent.
 */
function makeTestChecker(input, expected, errors) {
  return function (e) {
    var domAsString = docToTestOutput(e.target.contentDocument);
    if (html5Exceptions[input]) {
      todo_is(domAsString, expected, "HTML5 expected success.");
      if (domAsString == expected) {
        writeErrorSummary(input, expected, domAsString, true);
      }
    } else {
      is(domAsString, expected, "HTML5 expected success.");
      if (domAsString != expected) {
        writeErrorSummary(input, expected, domAsString, false);
      }
    }
    nextTest(e.target);
  } 
}

function makeFragmentTestChecker(input, 
                                 expected, 
                                 errors, 
                                 fragment, 
                                 testframe) {
  return function () {
    var context = document.createElementNS("http://www.w3.org/1999/xhtml",
                                           fragment);
    context.innerHTML = input;
    var domAsString = fragmentToTestOutput(context);
    is(domAsString, expected, "HTML5 expected success. " + new Date());
    if (domAsString != expected) {
      writeErrorSummary(input, expected, domAsString, false);
    }
    nextTest(testframe);
  } 
}

var testcases;
function nextTest(testframe) {
  var test = 0;
  try {
    var [input, output, errors, fragment] = testcases.next();
    if (fragment) {
      asyncRun(makeFragmentTestChecker(input, 
                                       output, 
                                       errors, 
                                       fragment, 
                                       testframe));
    } else {
      dataURL = "data:text/html;charset=utf-8," + encodeURIComponent(input);
      testframe.onload = makeTestChecker(input, output, errors);
      testframe.src = dataURL;
    }
  } catch (err if err instanceof StopIteration) {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    prefService.setBoolPref("html5.enable", origPref);
    SimpleTest.finish();
  }
}

var testFileContents = [];
function loadNextTestFile() {
  var datFile = parserDatFiles.shift();
  if (datFile) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
      if (this.readyState == 4) {
        testFileContents.push(this.responseText);
        loadNextTestFile();
      }
    };
    xhr.open("GET", "html5lib_tree_construction/" + datFile);
    xhr.send();
  } else {
    testcases = test_parser(testFileContents);
    nextTest($("testframe"));
  }
}

netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                  .getService(Components.interfaces.nsIPrefBranch)
                  .QueryInterface(Components.interfaces.nsIPrefService);
var origPref = prefService.getBoolPref("html5.enable");
prefService.setBoolPref("html5.enable", true);
addLoadEvent(loadNextTestFile);
SimpleTest.waitForExplicitFinish();
