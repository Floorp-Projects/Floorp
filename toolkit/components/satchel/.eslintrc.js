/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  rules: {
    "mozilla/balanced-listeners": "error",
    "mozilla/var-only-at-top-level": "error",

    "block-scoped-var": "error",
    complexity: [
      "error",
      {
        max: 23,
      },
    ],
    "max-nested-callbacks": ["error", 4],
    "no-console": "error",
    "no-fallthrough": "error",
    "no-multi-str": "error",
    "no-proto": "error",
    "no-unused-expressions": "error",
    "no-unused-vars": [
      "error",
      {
        args: "none",
        vars: "all",
      },
    ],
    "no-use-before-define": [
      "error",
      {
        functions: false,
      },
    ],
    radix: "error",
    "valid-jsdoc": [
      "error",
      {
        prefer: {
          return: "returns",
        },
        preferType: {
          Boolean: "boolean",
          Number: "number",
          String: "string",
          bool: "boolean",
        },
        requireParamDescription: false,
        requireReturn: false,
        requireReturnDescription: false,
      },
    ],
    yoda: "error",
  },
};
