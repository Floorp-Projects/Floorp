"use strict";

/**
 * Based on npm coding standards at https://docs.npmjs.com/misc/coding-style.
 *
 * The places we differ from the npm coding style:
 *   - Commas should be at the end of a line.
 *   - Always use semicolons.
 *   - Functions should not have whitespace before params.
 */

module.exports = {
  "env": {
    "node": true
  },

  "rules": {
    "brace-style": ["error", "1tbs"],
    "camelcase": "error",
    "comma-dangle": ["error", "never"],
    "comma-spacing": "error",
    "comma-style": ["error", "last"],
    "curly": ["error", "multi-line"],
    "handle-callback-err": ["error", "er"],
    "indent": ["error", 2, {"SwitchCase": 1}],
    "max-len": ["error", 80, 2],
    "no-multiple-empty-lines": ["error", {"max": 1}],
    "no-undef": "error",
    "no-undef-init": "error",
    "no-unexpected-multiline": "error",
    "object-curly-spacing": "off",
    "one-var": ["error", "never"],
    "operator-linebreak": ["error", "after"],
    "semi": ["error", "always"],
    "space-before-blocks": "error",
    "space-before-function-paren": ["error", "never"],
    "keyword-spacing": "error",
    "strict": ["error", "global"],
  },

  // Globals accessible within node modules.
  "globals": {
    "DTRACE_HTTP_CLIENT_REQUEST": true,
    "DTRACE_HTTP_CLIENT_RESPONSE": true,
    "DTRACE_HTTP_SERVER_REQUEST": true,
    "DTRACE_HTTP_SERVER_RESPONSE": true,
    "DTRACE_NET_SERVER_CONNECTION": true,
    "DTRACE_NET_STREAM_END": true,
    "Intl": true,
  },
};
