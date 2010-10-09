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

  /* check that the first non-empty, non-comment line is #data */
  for each (var line in lines) {
    if (!line || startsWith(line, "##")) {
      continue;
    }
    if (line == "#data")
      break;
    log(lines);
    throw "Unknown test format."
  }

  var input = [];
  var output = [];
  var errors = [];
  var fragment = [];
  var currentList = input;
  for each (var line in lines) {
    if (startsWith(line, "##todo")) {
      todo(false, line.substring(6));
      continue;
    }
    if (!(startsWith(line, "#error") ||
          startsWith(line, "#document") ||
          startsWith(line, "#document-fragment") ||
          startsWith(line, "#data"))) {
      currentList.push(line);
    } else if (line == "#errors") {
      currentList = errors;
    } else if (line == "#document") {
      currentList = output;
    } else if (line == "#document-fragment") {
      currentList = fragment;
    }
  }
  while (!output[output.length - 1]) {
    output.pop(); // zap trailing blank lines
  }
  //logger.log(input.length, output.length, errors.length);
  return [input.join("\n"), output.join("\n"), errors, fragment[0]];
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
  return addLevels(walker, "", "| ").slice(0,-1); // remove the last newline
}

/**
 * Transforms the descendants of an element to a string matching the format
 * in the test cases.
 *
 * @param an element
 */
function fragmentToTestOutput(elt) {
  var walker = elt.ownerDocument.createTreeWalker(elt, NodeFilter.SHOW_ALL, 
    function (node) { return elt == node ? 
                        NodeFilter.FILTER_SKIP : 
                        NodeFilter.FILTER_ACCEPT; }, true);
  return addLevels(walker, "", "| ").slice(0,-1); // remove the last newline
}

function addLevels(walker, buf, indent) {
  if(walker.firstChild()) {
    do {
      buf += indent;
      switch (walker.currentNode.nodeType) {
        case Node.ELEMENT_NODE:
          buf += "<"
          var ns = walker.currentNode.namespaceURI;
          if ("http://www.w3.org/1998/Math/MathML" == ns) {
            buf += "math ";
          } else if ("http://www.w3.org/2000/svg" == ns) {
            buf += "svg ";
          } else if ("http://www.w3.org/1999/xhtml" != ns) {
            buf += "otherns ";
          }
          buf += walker.currentNode.localName + ">";
          if (walker.currentNode.hasAttributes()) {
            var valuesByName = {};
            var attrs = walker.currentNode.attributes;
            for (var i = 0; i < attrs.length; ++i) {
              var localName = attrs[i].localName;
              if (localName.indexOf("_moz-") == 0) {
                // Skip bogus attributes added by the MathML implementation
                continue;
              }
              var name;
              var attrNs = attrs[i].namespaceURI;
              if (null == attrNs) {
                name = localName;
              } else if ("http://www.w3.org/XML/1998/namespace" == attrNs) {
                name = "xml " + localName;
              } else if ("http://www.w3.org/1999/xlink" == attrNs) {
                name = "xlink " + localName;
              } else if ("http://www.w3.org/2000/xmlns/" == attrNs) {
                name = "xmlns " + localName;
              } else {
                name = "otherns " + localName;
              }
              valuesByName[name] = attrs[i].value;
            }
            var keys = Object.keys(valuesByName).sort();
            for (var i = 0; i < keys.length; ++i) {
              buf += "\n" + indent + "  " + keys[i] + 
                     "=\"" + valuesByName[keys[i]] +"\"";
            }
          }
          break;
        case Node.DOCUMENT_TYPE_NODE:
          buf += "<!DOCTYPE " + walker.currentNode.name;
          if (walker.currentNode.publicId || walker.currentNode.systemId) {
            buf += " \"";
            buf += walker.currentNode.publicId;
            buf += "\" \"";
            buf += walker.currentNode.systemId;
            buf += "\"";
          }
          buf += ">";
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

