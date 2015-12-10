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
let errorRegex = /(.*)\nLine: (\d+)\nColumn: (\d+)\nChar: (.*)/
function parseError(err) {
  let matches = err.message.match(errorRegex);
  if (!matches)
    return null;

  return {
    fatal: true,
    message: matches[1],
    line: parseInt(matches[2]) + 1,
    column: parseInt(matches[3])
  }
}

// A simple sax listener that generates a tree of element information
function XMLParser(parser) {
  this.parser = parser;
  parser.onopentag = this.onOpenTag.bind(this);
  parser.onclosetag = this.onCloseTag.bind(this);
  parser.ontext = this.onText.bind(this);
  parser.oncdata = this.onText.bind(this);

  this.document = {
    local: "#document",
    uri: null,
    children: [],
  }
  this._currentNode = this.document;
}

XMLParser.prototype = {
  parser: null,

  onOpenTag: function(tag) {
    let node = {
      parentNode: this._currentNode,
      local: tag.local,
      namespace: tag.uri,
      attributes: {},
      children: [],
      textContent: "",
      textStart: this.parser.line,
    }

    for (let attr of Object.keys(tag.attributes)) {
      if (tag.attributes[attr].uri == "") {
        node.attributes[attr] = tag.attributes[attr].value;
      }
    }

    this._currentNode.children.push(node);
    this._currentNode = node;
  },

  onCloseTag: function(tagname) {
    this._currentNode = this._currentNode.parentNode;
  },

  onText: function(text) {
    this._currentNode.textContent += text;
  }
}

// Strips the indentation from lines of text and adds a fixed two spaces indent
function reindent(text) {
  let lines = text.split("\n");

  // The last line is likely indentation for the XML closing tag.
  if (lines[lines.length - 1].trim() == "") {
    lines.pop();
  }

  if (!lines.length) {
    return "";
  }

  // Find the preceeding whitespace for all lines that aren't entirely whitespace
  let indents = lines.filter(s => s.trim().length > 0)
                     .map(s => s.length - s.trimLeft().length);
  // Find the smallest indent level in use
  let minIndent = Math.min(...indents);

  // Strip off the found indent level and prepend the new indent level, but only
  // if the string isn't already empty.
  lines = lines.map(s => s.length > 0 ? "  " + s.substring(minIndent) : s);
  return lines.join("\n") + "\n";
}

// -----------------------------------------------------------------------------
// Processor Definition
// -----------------------------------------------------------------------------

// Stores any XML parse error
let xmlParseError = null;

// Stores the starting line for each script block generated
let blockLines = [];

module.exports = {
  preprocess: function(text, filename) {
    xmlParseError = null;
    blockLines = [];

    // Non-strict allows us to ignore many errors from entities and
    // preprocessing at the expense of failing to report some XML errors.
    // Unfortunately it also throws away the case of tagnames and attributes
    let parser = sax.parser(false, {
      lowercase: true,
      xmlns: true,
    });

    parser.onerror = function(err) {
      xmlParseError = parseError(err);
    }

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

    let scripts = [];

    for (let binding of bindings.children) {
      if (binding.local != "binding" || binding.namespace != NS_XBL) {
        continue;
      }

      for (let part of binding.children) {
        if (part.namespace != NS_XBL) {
          continue;
        }

        if (part.local != "implementation" && part.local != "handlers") {
          continue;
        }

        for (let item of part.children) {
          if (item.namespace != NS_XBL) {
            continue;
          }

          switch (item.local) {
            case "field": {
              // Fields get converted into variable declarations
              let def = item.textContent.trimRight();
              // Ignore empty fields
              if (def.trim().length == 0) {
                continue;
              }
              blockLines.push(item.textStart);
              scripts.push(`let ${item.attributes.name} = ${def}\n`);
              break;
            }
            case "constructor":
            case "destructor": {
              // Constructors and destructors become function declarations
              blockLines.push(item.textStart);
              let content = reindent(item.textContent);
              scripts.push(`function ${item.local}() {${content}}\n`);
              break;
            }
            case "method": {
              // Methods become function declarations with the appropriate params
              let params = item.children.filter(n => n.local == "parameter" && n.namespace == NS_XBL)
                                        .map(n => n.attributes.name)
                                        .join(", ");
              let body = item.children.filter(n => n.local == "body" && n.namespace == NS_XBL)[0];
              blockLines.push(body.textStart);
              body = reindent(body.textContent);
              scripts.push(`function ${item.attributes.name}(${params}) {${body}}\n`)
              break;
            }
            case "property": {
              // Properties become one or two function declarations
              for (let propdef of item.children) {
                if (propdef.namespace != NS_XBL) {
                  continue;
                }

                blockLines.push(propdef.textStart);
                let content = reindent(propdef.textContent);
                let params = propdef.local == "setter" ? "val" : "";
                scripts.push(`function ${item.attributes.name}_${propdef.local}(${params}) {${content}}\n`);
              }
              break;
            }
            case "handler": {
              // Handlers become a function declaration with an `event` parameter
              blockLines.push(item.textStart);
              let content = reindent(item.textContent);
              scripts.push(`function on${item.attributes.event}(event) {${content}}\n`);
              break;
            }
            default:
              continue;
          }
        }
      }
    }

    return scripts;
  },

  postprocess: function(messages, filename) {
    // If there was an XML parse error then just return that
    if (xmlParseError) {
      return [xmlParseError];
    }

    // For every message from every script block update the line to point to the
    // correct place.
    let errors = [];
    for (let i = 0; i < messages.length; i++) {
      let line = blockLines[i];

      for (let message of messages[i]) {
        message.line += line;
        errors.push(message);
      }
    }

    return errors;
  }
};
