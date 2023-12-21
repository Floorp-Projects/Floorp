/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  plugins: ["jsdoc"],

  rules: {
    "jsdoc/check-access": "error",
    // Handled by prettier
    // "jsdoc/check-alignment": "error",
    "jsdoc/check-param-names": "error",
    "jsdoc/check-property-names": "error",
    "jsdoc/check-tag-names": "error",
    "jsdoc/check-types": "error",
    "jsdoc/empty-tags": "error",
    "jsdoc/newline-after-description": "error",
    "jsdoc/no-multi-asterisks": "error",
    "jsdoc/require-param-type": "error",
    "jsdoc/require-returns-type": "error",
    "jsdoc/valid-types": "error",
  },
  settings: {
    jsdoc: {
      // This changes what's allowed in JSDocs, enabling more type-inference
      // friendly types.  This is the default in eslint-plugin-jsdoc versions
      // since May 2023, but we're still on 39.9 and need opt-in for now.
      // https://github.com/gajus/eslint-plugin-jsdoc/issues/834
      mode: "typescript",
    },
  },
};
