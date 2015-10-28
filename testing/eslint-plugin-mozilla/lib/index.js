/**
 * @fileoverview A collection of rules that help enforce JavaScript coding
 * standard and avoid common errors in the Mozilla project.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

//------------------------------------------------------------------------------
// Plugin Definition
//------------------------------------------------------------------------------

module.exports = {
  rules: {
    "balanced-listeners": require("../lib/rules/balanced-listeners"),
    "components-imports": require("../lib/rules/components-imports"),
    "import-headjs-globals": require("../lib/rules/import-headjs-globals"),
    "mark-test-function-used": require("../lib/rules/mark-test-function-used"),
    "no-aArgs": require("../lib/rules/no-aArgs"),
    "var-only-at-top-level": require("../lib/rules/var-only-at-top-level")
  },
  rulesConfig: {
    "balanced-listeners": 0,
    "components-imports": 0,
    "import-headjs-globals": 0,
    "mark-test-function-used": 0,
    "no-aArgs": 0,
    "var-only-at-top-level": 0
  }
};
