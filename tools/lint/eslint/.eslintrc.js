/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  plugins: ["eslint-plugin"],
  extends: ["plugin:eslint-plugin/recommended"],
  // eslint-plugin-mozilla runs under node, so we need a more restrictive
  // environment / parser setup here than the rest of mozilla-central.
  env: {
    browser: false,
    node: true,
  },
  parser: "espree",
  parserOptions: {
    // This should match with the minimum node version that the ESLint CI
    // process uses (check the linux64-node toolchain).
    ecmaVersion: 12,
  },

  rules: {
    camelcase: ["error", { properties: "never" }],
    "handle-callback-err": ["error", "er"],
    "no-shadow": "error",
    "no-undef-init": "error",
    "one-var": ["error", "never"],
    strict: ["error", "global"],
  },
};
