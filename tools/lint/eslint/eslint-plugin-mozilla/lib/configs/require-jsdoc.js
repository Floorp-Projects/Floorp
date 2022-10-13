/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  plugins: ["jsdoc"],

  rules: {
    "jsdoc/require-jsdoc": [
      "error",
      {
        require: {
          ClassDeclaration: true,
          FunctionDeclaration: false,
        },
      },
    ],
    "jsdoc/require-param": "error",
    "jsdoc/require-param-description": "error",
    "jsdoc/require-param-name": "error",
    "jsdoc/require-property": "error",
    "jsdoc/require-property-description": "error",
    "jsdoc/require-property-name": "error",
    "jsdoc/require-property-type": "error",
    "jsdoc/require-returns": "error",
    "jsdoc/require-returns-check": "error",
    "jsdoc/require-yields": "error",
    "jsdoc/require-yields-check": "error",
  },
};
