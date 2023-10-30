/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const { dirname, join } = require("path");

const eslintBasePath = dirname(require.resolve("eslint"));

const noredeclarePath = join(eslintBasePath, "rules/no-redeclare.js");
const baseRule = require(noredeclarePath);
const astUtils = require(join(eslintBasePath, "rules/utils/ast-utils.js"));

// Hack alert: our eslint env is pretty confused about `require` and
// `loader` for devtools modules - so ignore it for now.
// See bug 1812547
const gIgnoredImports = new Set(["loader", "require"]);

/**
 * Create a trap for a call to `report` that the original rule is
 * trying to make on `context`.
 *
 * Returns a function that forwards to `report` but provides a fixer
 * for redeclared imports that just removes those imports.
 *
 * @return {function}
 */
function trapReport(context) {
  return function (obj) {
    let declarator = obj.node.parent;
    while (
      declarator &&
      declarator.parent &&
      declarator.type != "VariableDeclarator"
    ) {
      declarator = declarator.parent;
    }
    if (
      declarator &&
      declarator.type == "VariableDeclarator" &&
      declarator.id.type == "ObjectPattern" &&
      declarator.init.type == "CallExpression"
    ) {
      let initialization = declarator.init;
      if (
        astUtils.isSpecificMemberAccess(
          initialization.callee,
          "ChromeUtils",
          /^import(ESModule|)$/
        )
      ) {
        // Hack alert: our eslint env is pretty confused about `require` and
        // `loader` for devtools modules - so ignore it for now.
        // See bug 1812547
        if (gIgnoredImports.has(obj.node.name)) {
          return;
        }
        // OK, we've got something we can fix. But we should be careful in case
        // there are multiple imports being destructured.
        // Do the easy (and common) case first - just one property:
        if (declarator.id.properties.length == 1) {
          context.report({
            node: declarator.parent,
            messageId: "duplicateImport",
            data: {
              name: declarator.id.properties[0].key.name,
            },
            fix(fixer) {
              return fixer.remove(declarator.parent);
            },
          });
          return;
        }

        // OK, figure out which import is duplicated here:
        let node = obj.node.parent;
        // Then remove a comma after it, or a comma before
        // if there's no comma after it.
        let sourceCode = context.getSourceCode();
        let rangeToRemove = node.range;
        let tokenAfter = sourceCode.getTokenAfter(node);
        let tokenBefore = sourceCode.getTokenBefore(node);
        if (astUtils.isCommaToken(tokenAfter)) {
          rangeToRemove[1] = tokenAfter.range[1];
        } else if (astUtils.isCommaToken(tokenBefore)) {
          rangeToRemove[0] = tokenBefore.range[0];
        }
        context.report({
          node,
          messageId: "duplicateImport",
          data: {
            name: node.key.name,
          },
          fix(fixer) {
            return fixer.removeRange(rangeToRemove);
          },
        });
        return;
      }
    }
    if (context.options[0]?.errorForNonImports) {
      // Report the result from no-redeclare - we can't autofix it.
      // This can happen for other redeclaration issues, e.g. naming
      // variables in a way that conflicts with builtins like "URL" or
      // "escape".
      context.report(obj);
    }
  };
}

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/no-redeclare-with-import-autofix.html",
    },
    messages: {
      ...baseRule.meta.messages,
      duplicateImport:
        "The import of '{{ name }}' is redundant with one set up earlier (e.g. head.js or the browser window environment). It should be removed.",
    },
    schema: [
      {
        type: "object",
        properties: {
          errorForNonImports: {
            type: "boolean",
            default: true,
          },
        },
        additionalProperties: false,
      },
    ],
    type: "suggestion",
    fixable: "code",
  },

  create(context) {
    // Test modules get the browser env applied wrongly in some cases,
    // don't try and remove imports there. This works out of the box
    // for sys.mjs modules because eslint won't check builtinGlobals
    // for the no-redeclare rule.
    if (context.getFilename().endsWith(".jsm")) {
      return {};
    }
    let newOptions = [{ builtinGlobals: true }];
    const contextForBaseRule = Object.create(context, {
      report: {
        value: trapReport(context),
        writable: false,
      },
      options: {
        value: newOptions,
      },
    });
    return baseRule.create(contextForBaseRule);
  },
};
