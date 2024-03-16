/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const globals = require("../globals");

const fs = require("fs");

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-more-globals.html",
    },
    messages: {
      newGlobal:
        "The global {{ name }} was not previously in this file, where new global variables are not permitted.",
      removedGlobal:
        "The global {{ name }} was expected to be defined in this file but isn't. Please remove it from the .globals file.",
      missingGlobalsFile:
        "This file has mozilla/no-more-globals enabled but has no .globals sibling file. Please create one.",
    },
    schema: [],
    type: "problem",
  },

  create(context) {
    return {
      Program(node) {
        let filename = context.filename;
        let code = context.sourceCode.getText();
        let currentGlobals = globals.getGlobalsForCode(code, {}, false);
        let knownGlobals;
        try {
          knownGlobals = new Set(
            JSON.parse(fs.readFileSync(filename + ".globals"))
          );
        } catch (ex) {
          context.report({
            node,
            messageId: "missingGlobalsFile",
          });
          return;
        }
        for (let { name } of currentGlobals) {
          if (!knownGlobals.has(name)) {
            context.report({
              node,
              messageId: "newGlobal",
              data: { name },
            });
          }
        }
        for (let known of knownGlobals) {
          if (!currentGlobals.some(n => n.name == known)) {
            context.report({
              node,
              messageId: "removedGlobal",
              data: { name: known },
            });
          }
        }
      },
    };
  },
};
