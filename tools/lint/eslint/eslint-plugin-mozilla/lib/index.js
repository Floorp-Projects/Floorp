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
  processors: {
    ".xml": require("../lib/processors/xbl-bindings"),
    ".js": require("../lib/processors/self-hosted"),
  },
  rules: {
    "avoid-removeChild": require("../lib/rules/avoid-removeChild"),
    "balanced-listeners": require("../lib/rules/balanced-listeners"),
    "import-browserjs-globals": require("../lib/rules/import-browserjs-globals"),
    "import-globals": require("../lib/rules/import-globals"),
    "import-headjs-globals": require("../lib/rules/import-headjs-globals"),
    "import-test-globals": require("../lib/rules/import-test-globals"),
    "mark-test-function-used": require("../lib/rules/mark-test-function-used"),
    "no-aArgs": require("../lib/rules/no-aArgs"),
    "no-cpows-in-tests": require("../lib/rules/no-cpows-in-tests"),
    "no-single-arg-cu-import": require("../lib/rules/no-single-arg-cu-import"),
    "no-import-into-var-and-global": require("../lib/rules/no-import-into-var-and-global.js"),
    "no-useless-parameters": require("../lib/rules/no-useless-parameters"),
    "no-useless-removeEventListener": require("../lib/rules/no-useless-removeEventListener"),
    "reject-importGlobalProperties": require("../lib/rules/reject-importGlobalProperties"),
    "reject-some-requires": require("../lib/rules/reject-some-requires"),
    "use-ownerGlobal": require("../lib/rules/use-ownerGlobal"),
    "var-only-at-top-level": require("../lib/rules/var-only-at-top-level")
  },
  rulesConfig: {
    "avoid-removeChild": 0,
    "balanced-listeners": 0,
    "import-browserjs-globals": 0,
    "import-globals": 0,
    "import-headjs-globals": 0,
    "import-test-globals": 0,
    "mark-test-function-used": 0,
    "no-aArgs": 0,
    "no-cpows-in-tests": 0,
    "no-single-arg-cu-import": 0,
    "no-import-into-var-and-global": 0,
    "no-useless-parameters": 0,
    "no-useless-removeEventListener": 0,
    "reject-importGlobalProperties": 0,
    "reject-some-requires": 0,
    "use-ownerGlobal": 0,
    "var-only-at-top-level": 0
  }
};
