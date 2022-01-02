/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// inherits from ../../tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js

module.exports = {
  rules: {
    camelcase: ["error", { properties: "never" }],
    "no-var": "error",
  },
};
