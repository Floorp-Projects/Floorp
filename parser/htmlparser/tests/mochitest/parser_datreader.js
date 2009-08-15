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
 * A test suite that runs WHATWG HTML parser tests.
 * The tests are from html5lib. 
 *
 * http://html5lib.googlecode.com/
 */

/**
 * A few utility functions.
 */
function log(entry) {
    
}

function startsWith(s, s2) {
  return s.indexOf(s2)==0;
}

function trimString(s) {
  return(s.replace(/^\s+/,'').replace(/\s+$/,''));
}

/**
 * Parses an individual testcase into an array containing the input
 * string, a string representing the expected tree (DOM), and a list
 * of error messages.
 *
 * @param A string containing a single testcase
 */
function parseTestcase(testcase) {
  var lines = testcase.split("\n");
  if (lines[0] != "#data") {
    log(lines);
    throw "Unknown test format."
  }
  var input = [];
  var output = [];
  var errors = [];
  var currentList = input;
  for each (var line in lines) {
    if (line && !(startsWith(line, "#error") ||
		  startsWith(line, "#document") ||
		  startsWith(line, "#data"))) {
      if (currentList == output && startsWith(line, "|")) {
      	currentList.push(line.substring(2));
      } else {
	      currentList.push(line);
      }
    } else if (line == "#errors") {
      currentList = errors;
    } else if (line == "#document") {
      currentList = output;
    }
  }  
  //logger.log(input.length, output.length, errors.length);
  return [input.join("\n"), output.join("\n"), errors];
}

/**
 * Sometimes the test output will depend on attribute order.
 * This function fixes up that output to match, if possible.
 *
 * @param output The string generated from walking the DOM
 * @param expected The expected output from the test case
 */
function reorderToMatchExpected(output, expected) {
  var outputLines = output.split("\n");
  var expectedLines = expected.split("\n");

  // if the line count is different, they don't match anyway
  if (expectedLines.length != outputLines.length)
    return output;

  var fixedOutput = [];
  var outputAtts = {};
  var expectedAtts = [];
  printAtts = function() {
    for each (var expectedAtt in expectedAtts) {
      if (outputAtts.hasOwnProperty(expectedAtt)) {
        fixedOutput.push(outputAtts[expectedAtt]);
      } else {
        // found a missing attribute
        return false;
      }
    }
    outputAtts = {};
    expectedAtts = [];
    return true;
  }

  for (var i=0; i < outputLines.length; i++) {
    var outputLine = outputLines[i];
    var expectedLine = expectedLines[i];
    var inAttrList = false;
    if (isAttributeLine(outputLine)) {
      // attribute mismatch, return original
      if (!isAttributeLine(expectedLine)) {
        return output; // mismatch, return original
      }
      // stick these in a dictionary
      inAttrList = true;
      outputAtts[attName(outputLine)] = outputLine;
      expectedAtts.push(attName(expectedLine));
    } else {
      if (inAttrList && !printAtts()) {
        return output; // mismatch, return original
      }
      inAttrList = false;
      fixedOutput.push(outputLine);
    }
  }

  if (inAttrList && !printAtts()) {
    return output; // mismatch, return original
  }

  return fixedOutput.join("\n");
}

function attName(line) {
  var str = trimString(line);
  return str.substring(0, str.indexOf("=\""));
}

function isAttributeLine(line) {
  var str = trimString(line);
  return (!startsWith(str, "<") && !startsWith(str, "\"") &&
          (str.indexOf("=\"") > 0));
}

/**
 * A generator function that accepts a list of strings. Each list
 * member corresponds to the contents of a ".dat" file from the
 * html5lib test suite.
 *
 * @param The list of strings
 */
function test_parser(testlist) {
  for each (var testgroup in testlist) {
    var tests = testgroup.split("#data\n");
    tests = ["#data\n" + test for each(test in tests) if (test)];
    for each (var test in tests) {
      yield parseTestcase(test);
    }
  }
}

/**
 * Transforms a DOM document to a string matching the format in 
 * the test cases.
 *
 * @param the DOM document
 */
function docToTestOutput(doc) {
  var walker = doc.createTreeWalker(doc, NodeFilter.SHOW_ALL, null, true);
  return addLevels(walker, "", "").slice(0,-1); // remove the last newline
}

function addLevels(walker, buf, indent) {
  if(walker.firstChild()) {
    do {
      buf += indent;
      switch (walker.currentNode.nodeType) {
        case Node.ELEMENT_NODE:
          buf += "<" + walker.currentNode.tagName.toLowerCase() + ">";
          if (walker.currentNode.hasAttributes()) {
            var attrs = walker.currentNode.attributes;
            for (var i=0; i < attrs.length; ++i) {
              buf += "\n" + indent + "  " + attrs[i].name + 
                     "=\"" + attrs[i].value +"\"";
            }
          }
          break;
        case Node.DOCUMENT_TYPE_NODE:
          buf += "<!DOCTYPE " + walker.currentNode.name + ">";
          break;
        case Node.COMMENT_NODE:
          buf += "<!-- " + walker.currentNode.nodeValue + " -->";
          break;
        case Node.TEXT_NODE:
          buf += "\"" + walker.currentNode.nodeValue + "\"";
          break;
      }
      buf += "\n";
      buf = addLevels(walker, buf, indent + "  ");
    } while(walker.nextSibling());
    walker.parentNode();
  }
  return buf;
}

