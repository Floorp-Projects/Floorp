/**
 * @fileoverview Reject use of Cu.importGlobalProperties
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const privilegedGlobals = Object.keys(
  require("../environments/privileged.js").globals
);

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/reject-importGlobalProperties.html",
    },
    messages: {
      unexpectedCall: "Unexpected call to Cu.importGlobalProperties",
      unexpectedCallWebIdl:
        "Unnecessary call to Cu.importGlobalProperties (webidl names are automatically imported)",
    },
    schema: [
      {
        enum: ["everything", "allownonwebidl"],
      },
    ],
    type: "problem",
  },

  create(context) {
    return {
      CallExpression(node) {
        if (node.callee.type !== "MemberExpression") {
          return;
        }
        let memexp = node.callee;
        if (
          memexp.object.type === "Identifier" &&
          // Only Cu, not Components.utils; see bug 1230369.
          memexp.object.name === "Cu" &&
          memexp.property.type === "Identifier" &&
          memexp.property.name === "importGlobalProperties"
        ) {
          if (context.options.includes("allownonwebidl")) {
            for (let element of node.arguments[0].elements) {
              if (privilegedGlobals.includes(element.value)) {
                context.report({ node, messageId: "unexpectedCallWebIdl" });
              }
            }
          } else {
            context.report({ node, messageId: "unexpectedCall" });
          }
        }
      },
    };
  },
};
