/**
 * @fileoverview Converts functions and handlers from XBL bindings into JS
 * functions
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const NS_XBL = "http://www.mozilla.org/xbl";

let sax = require("sax");

// Converts sax's error message to something that eslint will understand
let errorRegex = /(.*)\nLine: (\d+)\nColumn: (\d+)\nChar: (.*)/;
function parseError(err) {
  let matches = err.message.match(errorRegex);
  if (!matches) {
    return null;
  }

  return {
    fatal: true,
    message: matches[1],
    line: parseInt(matches[2]) + 1,
    column: parseInt(matches[3])
  };
}

let entityRegex = /&[\w][\w-\.]*;/g;

// A simple sax listener that generates a tree of element information
function XMLParser(parser) {
  this.parser = parser;
  parser.onopentag = this.onOpenTag.bind(this);
  parser.onclosetag = this.onCloseTag.bind(this);
  parser.ontext = this.onText.bind(this);
  parser.onopencdata = this.onOpenCDATA.bind(this);
  parser.oncdata = this.onCDATA.bind(this);
  parser.oncomment = this.onComment.bind(this);

  this.document = {
    local: "#document",
    uri: null,
    children: [],
    comments: []
  };
  this._currentNode = this.document;
}

XMLParser.prototype = {
  parser: null,

  onOpenTag(tag) {
    let node = {
      parentNode: this._currentNode,
      local: tag.local,
      namespace: tag.uri,
      attributes: {},
      children: [],
      comments: [],
      textContent: "",
      textLine: this.parser.line,
      textColumn: this.parser.column,
      textEndLine: this.parser.line
    };

    for (let attr of Object.keys(tag.attributes)) {
      if (tag.attributes[attr].uri == "") {
        node.attributes[attr] = tag.attributes[attr].value;
      }
    }

    this._currentNode.children.push(node);
    this._currentNode = node;
  },

  onCloseTag(tagname) {
    this._currentNode.textEndLine = this.parser.line;
    this._currentNode = this._currentNode.parentNode;
  },

  addText(text) {
    this._currentNode.textContent += text;
  },

  onText(text) {
    // Replace entities with some valid JS token.
    this.addText(text.replace(entityRegex, "null"));
  },

  onOpenCDATA() {
    // Turn the CDATA opening tag into whitespace for indent alignment
    this.addText(" ".repeat("<![CDATA[".length));
  },

  onCDATA(text) {
    this.addText(text);
  },

  onComment(text) {
    this._currentNode.comments.push(text);
  }
};

// -----------------------------------------------------------------------------
// Processor Definition
// -----------------------------------------------------------------------------

const INDENT_LEVEL = 2;

function indent(count) {
  return " ".repeat(count * INDENT_LEVEL);
}

// Stores any XML parse error
let xmlParseError = null;

// Stores the lines of JS code generated from the XBL
let scriptLines = [];
// Stores a map from the synthetic line number to the real line number
// and column offset.
let lineMap = [];

function addSyntheticLine(line, linePos, addDisableLine) {
  lineMap[scriptLines.length] = { line: linePos, offset: null };
  scriptLines.push(line + (addDisableLine ? "" : " // eslint-disable-line"));
}

/**
 * Adds generated lines from an XBL node to the script to be passed back to
 * eslint.
 */
function addNodeLines(node, reindent) {
  let lines = node.textContent.split("\n");
  let startLine = node.textLine;
  let startColumn = node.textColumn;

  // The case where there are no whitespace lines before the first text is
  // treated differently for indentation
  let indentFirst = false;

  // Strip off any preceeding whitespace only lines. These are often used to
  // format the XML and CDATA blocks.
  while (lines.length && lines[0].trim() == "") {
    indentFirst = true;
    startLine++;
    lines.shift();
  }

  // Strip off any whitespace lines at the end. These are often used to line
  // up the closing tags
  while (lines.length && lines[lines.length - 1].trim() == "") {
    lines.pop();
  }

  if (!indentFirst) {
    let firstLine = lines.shift();
    firstLine = " ".repeat(reindent * INDENT_LEVEL) + firstLine;
    // ESLint counts columns starting at 1 rather than 0
    lineMap[scriptLines.length] = {
      line: startLine,
      offset: reindent * INDENT_LEVEL - (startColumn - 1)
    };
    scriptLines.push(firstLine);
    startLine++;
  }

  // Find the preceeding whitespace for all lines that aren't entirely
  // whitespace.
  let indents = lines.filter(s => s.trim().length > 0)
                     .map(s => s.length - s.trimLeft().length);
  // Find the smallest indent level in use
  let minIndent = Math.min.apply(null, indents);

  for (let line of lines) {
    if (line.trim().length == 0) {
      // Don't offset lines that are only whitespace, the only possible JS error
      // is trailing whitespace and we want it to point at the right place
      lineMap[scriptLines.length] = { line: startLine, offset: 0 };
    } else {
      line = " ".repeat(reindent * INDENT_LEVEL) + line.substring(minIndent);
      lineMap[scriptLines.length] = {
        line: startLine,
        offset: reindent * INDENT_LEVEL - (minIndent - 1)
      };
    }

    scriptLines.push(line);
    startLine++;
  }
}

module.exports = {
  preprocess(text, filename) {
    xmlParseError = null;
    scriptLines = [];
    lineMap = [];

    // Non-strict allows us to ignore many errors from entities and
    // preprocessing at the expense of failing to report some XML errors.
    // Unfortunately it also throws away the case of tagnames and attributes
    let parser = sax.parser(false, {
      lowercase: true,
      xmlns: true
    });

    parser.onerror = function(err) {
      xmlParseError = parseError(err);
    };

    let xp = new XMLParser(parser);
    parser.write(text);

    // Sanity checks to make sure we're dealing with an XBL document
    let document = xp.document;
    if (document.children.length != 1) {
      return [];
    }

    let bindings = document.children[0];
    if (bindings.local != "bindings" || bindings.namespace != NS_XBL) {
      return [];
    }

    for (let comment of document.comments) {
      addSyntheticLine(`/*`, 0, true);
      for (let line of comment.split("\n")) {
        addSyntheticLine(`${line.trim()}`, 0, true);
      }
      addSyntheticLine(`*/`, 0, true);
    }

    addSyntheticLine(`this.bindings = {`, bindings.textLine);

    for (let binding of bindings.children) {
      if (binding.local != "binding" || binding.namespace != NS_XBL) {
        continue;
      }

      addSyntheticLine(indent(1) +
        `"${binding.attributes.id}": {`, binding.textLine);

      for (let part of binding.children) {
        if (part.namespace != NS_XBL) {
          continue;
        }

        if (part.local == "implementation") {
          addSyntheticLine(indent(2) + `implementation: {`, part.textLine);
        } else if (part.local == "handlers") {
          addSyntheticLine(indent(2) + `handlers: [`, part.textLine);
        } else {
          continue;
        }

        for (let item of part.children) {
          if (item.namespace != NS_XBL) {
            continue;
          }

          switch (item.local) {
            case "field": {
              // Fields are something like lazy getter functions

              // Ignore empty fields
              if (item.textContent.trim().length == 0) {
                continue;
              }

              addSyntheticLine(indent(3) +
                `get ${item.attributes.name}() {`, item.textLine);
              addSyntheticLine(indent(4) +
                `return (`, item.textLine);

              // Remove trailing semicolons, as we are adding our own
              item.textContent = item.textContent.replace(/;(?=\s*$)/, "");
              addNodeLines(item, 5);

              addSyntheticLine(indent(4) + `);`, item.textLine);
              addSyntheticLine(indent(3) + `},`, item.textEndLine);
              break;
            }
            case "constructor":
            case "destructor": {
              // Constructors and destructors become function declarations
              addSyntheticLine(indent(3) + `${item.local}() {`, item.textLine);
              addNodeLines(item, 4);
              addSyntheticLine(indent(3) + `},`, item.textEndLine);
              break;
            }
            case "method": {
              // Methods become function declarations with the appropriate
              // params.

              let params = item.children.filter(n => {
                return n.local == "parameter" && n.namespace == NS_XBL;
              })
                                        .map(n => n.attributes.name)
                                        .join(", ");
              let body = item.children.filter(n => {
                return n.local == "body" && n.namespace == NS_XBL;
              })[0];

              addSyntheticLine(indent(3) +
                `${item.attributes.name}(${params}) {`, item.textLine);
              addNodeLines(body, 4);
              addSyntheticLine(indent(3) + `},`, item.textEndLine);
              break;
            }
            case "property": {
              // Properties become one or two function declarations
              for (let propdef of item.children) {
                if (propdef.namespace != NS_XBL) {
                  continue;
                }

                if (propdef.local == "setter") {
                  addSyntheticLine(indent(3) +
                    `set ${item.attributes.name}(val) {`, propdef.textLine);
                } else if (propdef.local == "getter") {
                  addSyntheticLine(indent(3) +
                    `get ${item.attributes.name}() {`, propdef.textLine);
                } else {
                  continue;
                }
                addNodeLines(propdef, 4);
                addSyntheticLine(indent(3) + `},`, propdef.textEndLine);
              }
              break;
            }
            case "handler": {
              // Handlers become a function declaration with an `event`
              // parameter.
              addSyntheticLine(indent(3) + `function(event) {`, item.textLine);
              addNodeLines(item, 4);
              addSyntheticLine(indent(3) + `},`, item.textEndLine);
              break;
            }
            default:
              continue;
          }
        }

        addSyntheticLine(indent(2) +
          (part.local == "implementation" ? `},` : `],`), part.textEndLine);
      }
      addSyntheticLine(indent(1) + `},`, binding.textEndLine);
    }
    addSyntheticLine(`};`, bindings.textEndLine);

    let script = scriptLines.join("\n") + "\n";
    return [script];
  },

  postprocess(messages, filename) {
    // If there was an XML parse error then just return that
    if (xmlParseError) {
      return [xmlParseError];
    }

    // For every message from every script block update the line to point to the
    // correct place.
    let errors = [];
    for (let i = 0; i < messages.length; i++) {
      for (let message of messages[i]) {
        // ESLint indexes lines starting at 1 but our arrays start at 0
        let mapped = lineMap[message.line - 1];

        message.line = mapped.line + 1;
        if (mapped.offset) {
          message.column -= mapped.offset;
        } else {
          message.column = NaN;
        }

        errors.push(message);
      }
    }

    return errors;
  }
};
