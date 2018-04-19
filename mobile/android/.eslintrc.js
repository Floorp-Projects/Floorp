"use strict";

module.exports = {
  rules: {
    // XXX Bug 1358949 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    "complexity": ["error", 31],

    // Rules enabled in mozilla/recommended, and disabled for now, we should
    // re-enable these over time.
    "consistent-return": "off",
    "no-empty": "off",
    "no-native-reassign": "off",
    "no-nested-ternary": "off",
    "no-new-object": "off",
    "no-octal": "off",
    "no-redeclare": "off",
    "no-useless-call": "off",
    "no-useless-concat": "off",
    "object-shorthand": "off",
  },

  "overrides": [{
    files: [
      // Bug 1425047.
      "chrome/**",
      // Bug 1425048.
      "components/extensions/**",
      // Bug 1425034.
      "modules/WebsiteMetadata.jsm",
      // Bug 1425051.
      "tests/browser/robocop/**",
    ],
    rules: {
      "no-unused-vars": "off",
      "no-undef": "off",
    }
  }, {
    files: [
      "chrome/geckoview/**",
      "components/geckoview/**",
      "modules/geckoview/**",
    ],
    rules: {
      "no-restricted-syntax": [
        "error",
        {
            "selector": `CallExpression > \
                         Identifier.callee[name = /^debug$|^warn$/]`,
            "message": "Use debug and warn with template literals, e.g. debug `foo`;",
        },
        {
            "selector": `BinaryExpression[operator = '+'] > \
                         TaggedTemplateExpression.left > \
                         Identifier.tag[name = /^debug$|^warn$/]`,
            "message": "Use only one template literal with debug/warn instead of concatenating multiple expressions,\n" +
                       "    e.g. (debug `foo ${42} bar`) instead of (debug `foo` + 42 + `bar`)",
        },
        {
            "selector": `TaggedTemplateExpression[tag.type = 'Identifier'][tag.name = /^debug$|^warn$/] > \
                         TemplateLiteral.quasi CallExpression > \
                         MemberExpression.callee[object.type = 'Identifier'][object.name = 'JSON'] > \
                         Identifier.property[name = 'stringify']`,
            "message": "Don't call JSON.stringify within debug/warn literals,\n" +
                       "    e.g. (debug `foo=${foo}`) instead of (debug `foo=${JSON.stringify(foo)}`)",
        },
      ],
    },
  }],
};
