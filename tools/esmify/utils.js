/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Shared utility functions.

/* eslint-env node */

function warnForPath(inputFile, path, message) {
  const loc = path.node.loc;
  console.log(
    `WARNING: ${inputFile}:${loc.start.line}:${loc.start.column} : ${message}`
  );
}

// Get the previous statement of `path.node` in `Program`.
function getPrevStatement(path) {
  const parent = path.parent;
  if (parent.node.type !== "Program") {
    return null;
  }

  const index = parent.node.body.findIndex(n => n == path.node);
  if (index === -1) {
    return null;
  }

  if (index === 0) {
    return null;
  }

  return parent.node.body[index - 1];
}

// Get the next statement of `path.node` in `Program`.
function getNextStatement(path) {
  const parent = path.parent;
  if (parent.node.type !== "Program") {
    return null;
  }

  const index = parent.node.body.findIndex(n => n == path.node);
  if (index === -1) {
    return null;
  }

  if (index + 1 == parent.node.body.length) {
    return null;
  }

  return parent.node.body[index + 1];
}

function isIdentifier(node, name) {
  if (node.type !== "Identifier") {
    return false;
  }
  if (node.name !== name) {
    return false;
  }
  return true;
}

function isString(node) {
  return node.type === "Literal" && typeof node.value === "string";
}

const jsmExtPattern = /\.(jsm|js|jsm\.js)$/;

function esmifyExtension(path) {
  return path.replace(jsmExtPattern, ".sys.mjs");
}

function calleeToString(node) {
  if (node.type === "Identifier") {
    return node.name;
  }

  if (node.type === "MemberExpression" && !node.computed) {
    return calleeToString(node.object) + "." + node.property.name;
  }

  return "???";
}

exports.warnForPath = warnForPath;
exports.getPrevStatement = getPrevStatement;
exports.getNextStatement = getNextStatement;
exports.isIdentifier = isIdentifier;
exports.isString = isString;
exports.jsmExtPattern = jsmExtPattern;
exports.esmifyExtension = esmifyExtension;
exports.calleeToString = calleeToString;
