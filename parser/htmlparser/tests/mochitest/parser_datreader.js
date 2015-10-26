/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  for (var line of lines) {
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
  for (var line of lines) {
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
  for (var testgroup of testlist) {
    var tests = testgroup.split("#data\n");
    tests = tests.filter(test => test).map(test => "#data\n" + test);
    for (var test of tests) {
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
  var walker = doc.createTreeWalker(doc, NodeFilter.SHOW_ALL, null);
  return addLevels(walker, "", "| ").slice(0,-1); // remove the last newline
}

/**
 * Creates a walker for a fragment that skips over the root node.
 *
 * @param an element
 */
function createFragmentWalker(elt) {
  return elt.ownerDocument.createTreeWalker(elt, NodeFilter.SHOW_ALL,
    function (node) {
      return elt == node ? NodeFilter.FILTER_SKIP : NodeFilter.FILTER_ACCEPT;
    });
}

/**
 * Transforms the descendants of an element to a string matching the format
 * in the test cases.
 *
 * @param an element
 */
function fragmentToTestOutput(elt) {
  var walker = createFragmentWalker(elt);
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
      // In the case of template elements, children do not get inserted as
      // children of the template element, instead they are inserted
      // as children of the template content (which is a document fragment).
      if (walker.currentNode instanceof HTMLTemplateElement) {
        buf += indent + "  content\n";
        // Walk through the template content.
        var templateWalker = createFragmentWalker(walker.currentNode.content);
        buf = addLevels(templateWalker, buf, indent + "    ");
      }
      buf = addLevels(walker, buf, indent + "  ");
    } while(walker.nextSibling());
    walker.parentNode();
  }
  return buf;
}

