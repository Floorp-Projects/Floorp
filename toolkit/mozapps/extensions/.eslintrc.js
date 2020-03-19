/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  rules: {
    // Warn about cyclomatic complexity in functions.
    // XXX Bug 1326071 - This should be reduced down - probably to 20 or to
    // be removed & synced with the mozilla/recommended value.
    complexity: ["error", { max: 68 }],

    "no-unused-vars": [
      "error",
      {
        args: "none",
        vars: "all",
      },
    ],
  },
  overrides: [
    {
      files: "test/xpcshell/head*.js",
      rules: {
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "local",
          },
        ],
      },
    },
  ],
};
