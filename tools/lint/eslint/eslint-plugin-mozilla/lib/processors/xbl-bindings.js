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

let XMLParser = require("./processor-helpers").XMLParser;

// -----------------------------------------------------------------------------
// Processor Definition
// -----------------------------------------------------------------------------

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

  // Strip off any preceding whitespace only lines. These are often used to
  // format the XML and CDATA blocks.
  while (lines.length && lines[0].trim() == "") {
    indentFirst = true;
    startLine++;
    lines.shift();
  }

  // Remember the indent of the last blank line, which is the closing part of
  // the CDATA block.
  let lastIndent = 0;
  if (lines.length && lines[lines.length - 1].trim() == "") {
    lastIndent = lines[lines.length - 1].length;
  }

  // If the second to last line is also blank and has a higher indent than the
  // last one, then the CDATA block doesn't close with the closing tag.
  if (lines.length > 2 && lines[lines.length - 2].trim() == "" &&
      lines[lines.length - 2].length > lastIndent) {
    lastIndent = lines[lines.length - 2].length;
  }

  // Strip off any whitespace lines at the end. These are often used to line
  // up the closing tags
  while (lines.length && lines[lines.length - 1].trim() == "") {
    lines.pop();
  }

  if (!indentFirst) {
    let firstLine = lines.shift();
    // ESLint counts columns starting at 1 rather than 0
    lineMap[scriptLines.length] = { line: startLine, offset: startColumn - 1 };
    scriptLines.push(firstLine);
    startLine++;
  }

  // Find the preceding whitespace for all lines that aren't entirely
  // whitespace.
  let indents = lines.filter(s => s.trim().length > 0)
                     .map(s => s.length - s.trimLeft().length);
  // Find the smallest indent level in use
  let minIndent = Math.min.apply(null, indents);
  let indent = Math.max(2, minIndent - lastIndent);

  for (let line of lines) {
    if (line.trim().length == 0) {
      // Don't offset lines that are only whitespace, the only possible JS error
      // is trailing whitespace and we want it to point at the right place
      lineMap[scriptLines.length] = { line: startLine, offset: 0 };
    } else {
      line = " ".repeat(indent) + line.substring(minIndent);
      lineMap[scriptLines.length] = {
        line: startLine,
        offset: 1 + indent - minIndent,
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

    let xp = new XMLParser(text);
    if (xp.lastError) {
      xmlParseError = xp.lastError;
    }

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

      addSyntheticLine(`"${binding.attributes.id}": {`, binding.textLine);

      for (let part of binding.children) {
        if (part.namespace != NS_XBL) {
          continue;
        }

        if (part.local == "implementation") {
          addSyntheticLine(`implementation: {`, part.textLine);
        } else if (part.local == "handlers") {
          addSyntheticLine(`handlers: [`, part.textLine);
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

              addSyntheticLine(`get ${item.attributes.name}() {`, item.textLine);
              addSyntheticLine(`return (`, item.textLine);

              // Remove trailing semicolons, as we are adding our own
              item.textContent = item.textContent.replace(/;(?=\s*$)/, "");
              addNodeLines(item, 5);

              addSyntheticLine(`);`, item.textLine);
              addSyntheticLine(`},`, item.textEndLine);
              break;
            }
            case "constructor":
            case "destructor": {
              // Constructors and destructors become function declarations
              addSyntheticLine(`${item.local}() {`, item.textLine);
              addNodeLines(item, 4);
              addSyntheticLine(`},`, item.textEndLine);
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

              addSyntheticLine(`${item.attributes.name}(${params}) {`, item.textLine);
              addNodeLines(body, 4);
              addSyntheticLine(`},`, item.textEndLine);
              break;
            }
            case "property": {
              // Properties become one or two function declarations
              for (let propdef of item.children) {
                if (propdef.namespace != NS_XBL) {
                  continue;
                }

                if (propdef.local == "setter") {
                  addSyntheticLine(`set ${item.attributes.name}(val) {`, propdef.textLine);
                } else if (propdef.local == "getter") {
                  addSyntheticLine(`get ${item.attributes.name}() {`, propdef.textLine);
                } else {
                  continue;
                }
                addNodeLines(propdef, 4);
                addSyntheticLine(`},`, propdef.textEndLine);
              }
              break;
            }
            case "handler": {
              // Handlers become a function declaration with an `event`
              // parameter.
              addSyntheticLine(`function(event) {`, item.textLine);
              addNodeLines(item, 4);
              addSyntheticLine(`},`, item.textEndLine);
              break;
            }
            default:
              continue;
          }
        }

        addSyntheticLine((part.local == "implementation" ? `},` : `],`), part.textEndLine);
      }
      addSyntheticLine(`},`, binding.textEndLine);
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
  },
};
