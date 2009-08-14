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
 * @param buf    string buffer containing the serialized output
 * @param obj    the object to serialize
 * @param indent the current line indent
 * @param mode   the current node type being serialized, or -1
 *               between nodes
 */
function dumpTree(buf, obj, indent, mode) {
  var dumpMode = mode;
  if (typeof(dumpMode) == "undefined") {
    dumpMode = -1
  }
  var buffer = buf;
  if (typeof(obj) == "object" && (obj instanceof Array)) {
    for each (var item in obj) {
      [buffer, indent, dumpMode] = 
        dumpTree(buffer, item, indent, dumpMode);
    }
    dumpMode = -1;
  }
  else {
    // Node.* constants are used here for convenience.
    switch(obj) {
      case "ParseError":
        // no-op
        break;
      case "Character":
        dumpMode = Node.TEXT_NODE;
        break;
      case "StartTag":
        dumpMode = Node.ELEMENT_NODE;
        break;
      case "EndTag":
        indent = indent.substring(2);
        break;
      case "Comment":
        dumpMode = Node.COMMENT_NODE;
        break;
      case "DOCTYPE":
        dumpMode = Node.DOCUMENT_TYPE_NODE;
        break;
      default:
        switch(dumpMode) {
          case Node.DOCUMENT_TYPE_NODE:
            buffer += "<!DOCTYPE " + obj + ">\n<html>\n  <head>\n  <body>";
            indent += "    "
            dumpMode = -1;
            break;
          case Node.COMMENT_NODE:
            if (buffer.length > 1) {
              buffer += "\n";
            }
            buffer += indent + "<!-- " + obj + " -->";
            dumpMode = -1;
            break;
          case Node.ATTRIBUTE_NODE:
            is(typeof(obj), "object", "obj not an object!");
            indent += "  ";
            for (var key in obj) {
              buffer += "\n" + indent + key + "=\"" + obj[key] + "\"";
            }
            dumpMode = -1;
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
            dumpMode = Node.ATTRIBUTE_NODE;
            break;
          default:
            // no-op
            break;
        }
        break;
    }
  }
  return [buffer, indent, dumpMode];
}

/**
 * Parses an individual testcase in decoded JSON form, 
 * as for tokenizer tests.
 *
 * @param An object containing a single testcase
 */
function parseJsonTestcase(testcase) {
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
 * Transforms a DOM document to a string matching the format in 
 * the test cases.
 *
 * @param doc  the DOM document
 * @param mode the mode of the current test runner 
 */
function docToTestOutput(doc, mode) {
  var walker = doc.createTreeWalker(doc, NodeFilter.SHOW_ALL, null, true);
  return addLevels(walker, "", "", mode).slice(0,-1); // remove last newline
}

function addLevels(walker, buf, indent, mode) {
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
          // Skip document type nodes for MODE_JSCOMPARE, as the nu.validator
          // doesn't emit them.
          if (mode != MODE_JSCOMPARE) {
            buf += indent + "<!DOCTYPE " + walker.currentNode.name + ">\n";
          }
          break;
        case Node.COMMENT_NODE:
          // Skip comment nodes for MODE_JSCOMPARE, as the nu.validator
          // doesn't emit them.
          if (mode != MODE_JSCOMPARE) {
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
      buf = addLevels(walker, buf, indent + "  ", mode);
    } while(walker.nextSibling());
    walker.parentNode();
  }
  return buf;
}
