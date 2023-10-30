/**
 * @fileoverview Require .ownerGlobal instead of .ownerDocument.defaultView.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  meta: {
    docs: {
      url: "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/rules/use-ownerGlobal.html",
    },
    messages: {
      useOwnerGlobal: "use .ownerGlobal instead of .ownerDocument.defaultView",
    },
    schema: [],
    type: "suggestion",
  },

  create(context) {
    return {
      MemberExpression(node) {
        if (
          node.property.type != "Identifier" ||
          node.property.name != "defaultView" ||
          node.object.type != "MemberExpression" ||
          node.object.property.type != "Identifier" ||
          node.object.property.name != "ownerDocument"
        ) {
          return;
        }

        context.report({
          node,
          messageId: "useOwnerGlobal",
        });
      },
    };
  },
};
