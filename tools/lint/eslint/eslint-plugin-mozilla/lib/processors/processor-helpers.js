/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

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
    column: parseInt(matches[3]),
  };
}

let entityRegex = /&[\w][\w-\.]*;/g;

// A simple sax listener that generates a tree of element information
function XMLParser(text) {
  // Non-strict allows us to ignore many errors from entities and
  // preprocessing at the expense of failing to report some XML errors.
  // Unfortunately it also throws away the case of tagnames and attributes
  let parser = sax.parser(false, {
    lowercase: true,
    xmlns: true,
  });

  parser.onerror = function(err) {
    this.lastError = parseError(err);
  };

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
    comments: [],
  };
  this._currentNode = this.document;

  parser.write(text);
}

XMLParser.prototype = {
  parser: null,

  lastError: null,

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
      textEndLine: this.parser.line,
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
  },
};

module.exports = {
  XMLParser,
};
