/**
 * @fileoverview Reject use of Cu.importGlobalProperties
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const path = require("path");

const privilegedGlobals = Object.keys(
  require("../environments/privileged.js").globals
);

function getMessageId(context) {
  return path.extname(context.getFilename()) == ".sjs"
    ? "unexpectedCallSjs"
    : "unexpectedCall";
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/reject-importGlobalProperties.html",
    },
    messages: {
      unexpectedCall: "Unexpected call to Cu.importGlobalProperties",
      unexpectedCallCuWebIdl:
        "Unnecessary call to Cu.importGlobalProperties for {{name}} (webidl names are automatically imported)",
      unexpectedCallSjs:
        "Do not call Cu.importGlobalProperties in sjs files, expand the global instead (see rule docs).",
      unexpectedCallXPCOMWebIdl:
        "Unnecessary call to XPCOMUtils.defineLazyGlobalGetters for {{name}} (webidl names are automatically imported)",
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
          // Only Cu, not Components.utils as `use-cc-etc` handles this for us.
          memexp.object.name === "Cu" &&
          memexp.property.type === "Identifier" &&
          memexp.property.name === "importGlobalProperties"
        ) {
          if (context.options.includes("allownonwebidl")) {
            for (let element of node.arguments[0].elements) {
              if (privilegedGlobals.includes(element.value)) {
                context.report({
                  node,
                  messageId: "unexpectedCallCuWebIdl",
                  data: { name: element.value },
                });
              }
            }
          } else {
            context.report({ node, messageId: getMessageId(context) });
          }
        }
        if (
          memexp.object.type === "Identifier" &&
          memexp.object.name === "XPCOMUtils" &&
          memexp.property.type === "Identifier" &&
          memexp.property.name === "defineLazyGlobalGetters" &&
          node.arguments.length >= 2
        ) {
          if (context.options.includes("allownonwebidl")) {
            for (let element of node.arguments[1].elements) {
              if (privilegedGlobals.includes(element.value)) {
                context.report({
                  node,
                  messageId: "unexpectedCallXPCOMWebIdl",
                  data: { name: element.value },
                });
              }
            }
          } else {
            context.report({ node, messageId: getMessageId(context) });
          }
        }
      },
    };
  },
};
