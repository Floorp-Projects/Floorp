/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  globals,
} = require("../../toolkit/components/extensions/parent/.eslintrc.js");

module.exports = {
  overrides: [
    {
      files: ["components/extensions/ext-*.js"],
      excludedFiles: ["components/extensions/ext-c-*.js"],
      globals: {
        ...globals,
        // These globals are defined in ext-android.js and can only be used in
        // the extension files that run in the parent process.
        EventDispatcher: true,
        ExtensionError: true,
        makeGlobalEvent: true,
        TabContext: true,
        tabTracker: true,
        windowTracker: true,
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
            argsIgnorePattern: "^_",
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
