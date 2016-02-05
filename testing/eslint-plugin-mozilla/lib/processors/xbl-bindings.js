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

let entityRegex = /&[\w][\w-\.]*;/g;

// A simple sax listener that generates a tree of element information
function XMLParser(parser) {
  this.parser = parser;
  parser.onopentag = this.onOpenTag.bind(this);
  parser.onclosetag = this.onCloseTag.bind(this);
  parser.ontext = this.onText.bind(this);
  parser.oncdata = this.onCDATA.bind(this);

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
      textStart: { line: this.parser.line, column: this.parser.column },
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

  addText: function(text) {
    this._currentNode.textContent += text;
  },

  onText: function(text) {
    // Replace entities with some valid JS token.
    this.addText(text.replace(entityRegex, "null"));
  },

  onCDATA: function(text) {
    // Turn the CDATA opening tag into whitespace for indent alignment
    this.addText(" ".repeat("<![CDATA[".length));
    this.addText(text);
  }
}

// Strips the indentation from lines of text and adds a fixed two spaces indent
function buildCodeBlock(tag, prefix, suffix, indent) {
  prefix = prefix === undefined ? "" : prefix;
  suffix = suffix === undefined ? "\n}" : suffix;
  indent = indent === undefined ? 2 : indent;

  let text = tag.textContent;
  let line = tag.textStart.line;
  let column = tag.textStart.column;

  let lines = text.split("\n");

  // Strip off any preceeding whitespace only lines. These are often used to
  // format the XML and CDATA blocks.
  while (lines.length && lines[0].trim() == "") {
    column = 0;
    line++;
    lines.shift();
  }

  // Strip off any whitespace lines at the end. These are often used to line
  // up the closing tags
  while (lines.length && lines[lines.length - 1].trim() == "") {
    lines.pop();
  }

  // Indent the first line with the starting position of the text block
  if (lines.length && column) {
    lines[0] = " ".repeat(column) + lines[0];
  }

  // Find the preceeding whitespace for all lines that aren't entirely whitespace
  let indents = lines.filter(s => s.trim().length > 0)
                     .map(s => s.length - s.trimLeft().length);
  // Find the smallest indent level in use
  let minIndent = Math.min.apply(null, indents);

  // Strip off the found indent level and prepend the new indent level, but only
  // if the string isn't already empty.
  let indentstr = " ".repeat(indent);
  lines = lines.map(s => s.length ? indentstr + s.substring(minIndent) : s);

  let block = {
    script: prefix + lines.join("\n") + suffix + "\n",
    line: line - prefix.split("\n").length,
    indent: minIndent - indent
  };
  return block;
}

// -----------------------------------------------------------------------------
// Processor Definition
// -----------------------------------------------------------------------------

// Stores any XML parse error
let xmlParseError = null;

// Stores the code blocks
let blocks = [];

module.exports = {
  preprocess: function(text, filename) {
    xmlParseError = null;
    blocks = [];

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

              // Ignore empty fields
              if (item.textContent.trim().length == 0) {
                continue;
              }
              blocks.push(buildCodeBlock(item, "", "", 0));
              break;
            }
            case "constructor":
            case "destructor": {
              // Constructors and destructors become function declarations
              blocks.push(buildCodeBlock(item, `function ${item.local}() {\n`));
              break;
            }
            case "method": {
              // Methods become function declarations with the appropriate params
              let params = item.children.filter(n => n.local == "parameter" && n.namespace == NS_XBL)
                                        .map(n => n.attributes.name)
                                        .join(", ");
              let body = item.children.filter(n => n.local == "body" && n.namespace == NS_XBL)[0];
              blocks.push(buildCodeBlock(body, `function ${item.attributes.name}(${params}) {\n`));
              break;
            }
            case "property": {
              // Properties become one or two function declarations
              for (let propdef of item.children) {
                if (propdef.namespace != NS_XBL) {
                  continue;
                }

                let params = propdef.local == "setter" ? "val" : "";
                blocks.push(buildCodeBlock(propdef, `function ${item.attributes.name}_${propdef.local}(${params}) {\n`));
              }
              break;
            }
            case "handler": {
              // Handlers become a function declaration with an `event` parameter
              blocks.push(buildCodeBlock(item, `function onevent(event) {\n`));
              break;
            }
            default:
              continue;
          }
        }
      }
    }

    return blocks.map(b => b.script);
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
      let block = blocks[i];

      for (let message of messages[i]) {
        message.line += block.line + 1;
        message.column += block.indent;
        errors.push(message);
      }
    }

    return errors;
  }
};
