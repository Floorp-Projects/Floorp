/**
 * @fileoverview Reject use of non-zero values in setTimeout
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var helpers = require("../helpers");
var testTypes = new Set(["browser", "xpcshell"]);

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-arbitrary-setTimeout.html",
    },
    messages: {
      listenForEvents:
        "listen for events instead of setTimeout() with arbitrary delay",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    // We don't want to run this on mochitest plain as it already
    // prevents flaky setTimeout at runtime. This check is built-in
    // to the rule itself as sometimes other tests can live alongside
    // plain mochitests and so it can't be configured via eslintrc.
    if (!testTypes.has(helpers.getTestType(context))) {
      return {};
    }

    return {
      CallExpression(node) {
        let callee = node.callee;
        if (callee.type === "MemberExpression") {
          if (
            callee.property.name !== "setTimeout" ||
            callee.object.name !== "window" ||
            node.arguments.length < 2
          ) {
            return;
          }
        } else if (callee.type === "Identifier") {
          if (callee.name !== "setTimeout" || node.arguments.length < 2) {
            return;
          }
        } else {
          return;
        }

        let timeout = node.arguments[1];
        if (timeout.type !== "Literal" || timeout.value > 0) {
          context.report({
            node,
            messageId: "listenForEvents",
          });
        }
      },
    };
  },
};
