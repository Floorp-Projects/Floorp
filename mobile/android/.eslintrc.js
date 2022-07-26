/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  overrides: [
    {
      files: [
        // Bug 1425048 - mainly going away, see bug 1583370.
        "components/extensions/**",
      ],
      rules: {
        "no-unused-vars": "off",
        "no-undef": "off",
      },
    },
    {
      files: [
        "chrome/geckoview/**",
        "components/geckoview/**",
        "modules/geckoview/**",
        "actors/**",
      ],
      rules: {
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "local",
            varsIgnorePattern: "(debug|warn)",
          },
        ],
        "no-restricted-syntax": [
          "error",
          {
            selector: `CallExpression > \
                         Identifier.callee[name = /^debug$|^warn$/]`,
            message:
              "Use debug and warn with template literals, e.g. debug `foo`;",
          },
          {
            selector: `BinaryExpression[operator = '+'] > \
                         TaggedTemplateExpression.left > \
                         Identifier.tag[name = /^debug$|^warn$/]`,
            message:
              "Use only one template literal with debug/warn instead of concatenating multiple expressions,\n" +
              "    e.g. (debug `foo ${42} bar`) instead of (debug `foo` + 42 + `bar`)",
          },
          {
            selector: `TaggedTemplateExpression[tag.type = 'Identifier'][tag.name = /^debug$|^warn$/] > \
                         TemplateLiteral.quasi CallExpression > \
                         MemberExpression.callee[object.type = 'Identifier'][object.name = 'JSON'] > \
                         Identifier.property[name = 'stringify']`,
            message:
              "Don't call JSON.stringify within debug/warn literals,\n" +
              "    e.g. (debug `foo=${foo}`) instead of (debug `foo=${JSON.stringify(foo)}`)",
          },
        ],
      },
    },
  ],
};
