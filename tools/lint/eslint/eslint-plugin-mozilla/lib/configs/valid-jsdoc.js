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
};
