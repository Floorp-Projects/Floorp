/**
 * @fileoverview Reject calls into OS.File. We're phasing this out in
 * favour of IOUtils.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

function isIdentifier(node, id) {
  return node && node.type === "Identifier" && node.name === id;
}

function isOSProp(expr, prop) {
  if (!isIdentifier(expr.property, prop)) {
    return false;
  }

  if (isIdentifier(expr.object, "OS")) {
    return true;
  }

  if (
    expr.object.type === "MemberExpression" &&
    isIdentifier(expr.object.object, "lazy") &&
    isIdentifier(expr.object.property, "OS")
  ) {
    return true;
  }

  return false;
}

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/reject-osfile.html",
    },
    type: "problem",
  },

  create(context) {
    return {
      MemberExpression(node) {
        if (isOSProp(node, "File")) {
          context.report(
            node,
            "OS.File is deprecated. You should use IOUtils instead."
          );
        } else if (isOSProp(node, "Path")) {
          context.report(
            node,
            "OS.Path is deprecated. You should use PathUtils instead."
          );
        }
      },
    };
  },
};
