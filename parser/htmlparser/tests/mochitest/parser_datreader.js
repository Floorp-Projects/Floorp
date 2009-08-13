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

function getLastLine(str) {
  var str_array = str.split("\n");
  let last_line = str_array[str_array.length - 1];
  return last_line;
}

/**
 * Produces a string containing the expected output of a
 * JSON-formatted test, by running the "output" object
 * of the test through a serializer.
 *
 * @param buf string buffer containing the serialized output
 * @param obj the object to serialize
 * @param indent the current line indent
 */
var gDumpMode;
function dumpTree(buf, obj, indent) {
  var buffer = buf;
  if (typeof(obj) == "object" && (obj instanceof Array)) {
    for each (var item in obj) {
      [buffer, indent] = dumpTree(buffer, item, indent);
    }
    gDumpMode = -1;
  }
  else {
    // Node.* constants are used here for convenience.
    switch(obj) {
      case "ParseError":
        // no-op
        break;
      case "Character":
        gDumpMode = Node.TEXT_NODE;
        break;
      case "StartTag":
        gDumpMode = Node.ELEMENT_NODE;
        break;
      case "EndTag":
        indent = indent.substring(2);
        break;
      case "Comment":
        gDumpMode = Node.COMMENT_NODE;
        break;
      case "DOCTYPE":
        gDumpMode = Node.DOCUMENT_TYPE_NODE;
        break;
      default:
        switch(gDumpMode) {
          case Node.DOCUMENT_TYPE_NODE:
            buffer += "<!DOCTYPE " + obj + ">\n<html>\n  <head>\n  <body>";
            indent += "    "
            gDumpMode = -1;
            break;
          case Node.COMMENT_NODE:
            if (buffer.length > 1) {
              buffer += "\n";
            }
            buffer += indent + "<!-- " + obj + " -->";
            gDumpMode = -1;
            break;
          case Node.ATTRIBUTE_NODE:
            is(typeof(obj), "object", "obj not an object!");
            indent += "  ";
            for (var key in obj) {
              buffer += "\n" + indent + key + "=\"" + obj[key] + "\"";
            }
            gDumpMode = -1;
            break;
          case Node.TEXT_NODE:
            if (buffer.indexOf("<head>") == -1) {
              buffer += "\n<html>\n  <head>\n  <body>";
              indent += "    ";
            }
            // If this text is being appended to some earlier
            // text, concatenate the two by chopping off the
            // trailing quote before adding new string.
            let last_line = trimString(getLastLine(buffer));
            if (last_line[0] == "\"" && 
              last_line[last_line.length - 1] == "\"") {
              buffer = buffer.substring(0, buffer.length - 1);    
            }
            else {
              buffer += "\n" + indent + "\"";  
            }
            buffer += obj + "\"";
            break;
          case Node.ELEMENT_NODE:
            buffer += "\n" + indent + "<" + obj + ">";
            gDumpMode = Node.ATTRIBUTE_NODE;
            break;
          default:
            // no-op
            break;
        }
        break;
    }
  }
  return [buffer, indent];
}

/**
 * Parses an individual testcase in decoded JSON form, 
 * as for tokenizer tests.
 *
 * @param An object containing a single testcase
 */
function parseJsonTestcase(testcase) {
  gDumpMode = -1;
  // If the test begins with something that looks like the
  // beginning of a doctype, then don't add a standard doctype,
  // otherwise do.
  if (testcase["input"].toLowerCase().indexOf("<!doc") == 0) {
    var test_output = dumpTree(
      "", 
      testcase["output"],
      "");
  } else {
    var test_output = dumpTree(
      "<!DOCTYPE html>\n<html>\n  <head>\n  <body>", 
      testcase["output"],
      "    ");
  }
  // Add html, head and body elements now if they
  // haven't been added already.
  if (test_output[0].indexOf("<head>") == -1) {
    test_output[0] += "\n<html>\n  <head>\n  <body>";
  }
  return [testcase["input"], test_output[0], "",
    testcase["description"],
    JSON.stringify(testcase["output"])];
}

/**
 * Parses an individual testcase into an array containing the input
 * string, a string representing the expected tree (DOM), and a list
 * of error messages.
 *
 * @param A string containing a single testcase
 */
function parseTestcase(testcase) {
  var documentFragmentTest = false;
  var lines = testcase.split("\n");
  if (lines[0] != "#data") {
    log(lines);
    throw "Unknown test format."
  }
  var input = [];
  var output = [];
  var errors = [];
  var description = undefined;
  var expectedTokenizerOutput = undefined;
  var currentList = input;
  for each (var line in lines) {
    // allow blank lines in input
    if ((line || currentList == input) && !(startsWith(line, "#errors") ||
		  startsWith(line, "#document") ||
		  startsWith(line, "#description") ||
		  startsWith(line, "#expected") || 
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
    } else if (line == "#document-fragment") {
      documentFragmentTest = true;
    }
  }  
  
  // For #document-fragment tests, erase the output, so that the 
  // test is skipped in makeTestChecker()...there is no good way
  // to run fragment tests without direct access to the parser.
  if (documentFragmentTest) {
    output = [];
  }
  //logger.log(input.length, output.length, errors.length);
  return [input.join("\n"), output.join("\n"), errors, description,
    expectedTokenizerOutput];
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
 * A generator function that accepts a list of tests. Each list
 * member corresponds to the contents of a ".dat" file from the
 * html5lib test suite, or an array of decoded JSON tests,
 * in the case of tokenizer "*.test" tests.
 *
 * @param The list of strings
 */
function test_parser(testlist) {
  var index = 1;
  if (gTokenizerMode) {
    for each (var test in testlist) {
      var tmpArray = [index];
      yield tmpArray.concat(parseJsonTestcase(test));
      index++;
    }
  }
  else {
    for each (var testgroup in testlist) {
      var tests = testgroup.split("#data\n");
      tests = ["#data\n" + test for each(test in tests) if (test)];
      for each (var test in tests) {
        yield parseTestcase(test);
      }
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
      switch (walker.currentNode.nodeType) {
        case Node.ELEMENT_NODE:
          buf += indent + "<";
          // Prefix MathML element names with "math " to match
          // the format of the expected output.
          if (walker.currentNode.namespaceURI.toLowerCase().
          indexOf("math") != -1) {
            buf += "math " + walker.currentNode.tagName.toLowerCase() + ">\n";
          }
          else if (walker.currentNode.namespaceURI.toLowerCase().
          indexOf("svg") != -1) {
            buf += "svg " + walker.currentNode.tagName + ">\n";
          }
          else {
            buf += walker.currentNode.tagName.toLowerCase() + ">\n";
          }
          if (walker.currentNode.hasAttributes()) {
            var attrs = walker.currentNode.attributes;
            for (var i=0; i < attrs.length; ++i) {
              // Ignore the -moz-math-font-style attr, which
              // Firefox automatically adds to every math element.
              var attrname = attrs[i].name;
              if (attrname != "-moz-math-font-style") {
                buf += indent + "  " + attrname + 
                       "=\"" + attrs[i].value +"\"\n";
              }
            }
          }
          break;
        case Node.DOCUMENT_TYPE_NODE:
          if (!gJSCompatibilityMode) {
            buf += indent + "<!DOCTYPE " + walker.currentNode.name + ">\n";
          }
          break;
        case Node.COMMENT_NODE:
          if (!gJSCompatibilityMode) {
            buf += indent + "<!-- " + walker.currentNode.nodeValue + " -->\n";
          }
          break;
        case Node.TEXT_NODE:
          // If this text is being appended to some earlier
          // text at the same indent level, concatenate the two by
          // removing the trailing quote before adding new string.
          let last_line = getLastLine(
            buf.substring(0, buf.length - 1));
          if (last_line[indent.length] == "\"" && 
            last_line[last_line.length - 1] == "\"") {
            buf = buf.substring(0, buf.length - 2);    
          }
          else {
            buf += indent + "\"";
          }
          buf += walker.currentNode.nodeValue + "\"\n";
          break;
      }
      buf = addLevels(walker, buf, indent + "  ");
    } while(walker.nextSibling());
    walker.parentNode();
  }
  return buf;
}

