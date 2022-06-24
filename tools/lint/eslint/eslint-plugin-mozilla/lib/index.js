/**
 * @fileoverview A collection of rules that help enforce JavaScript coding
 * standard and avoid common errors in the Mozilla project.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// ------------------------------------------------------------------------------
// Plugin Definition
// ------------------------------------------------------------------------------
module.exports = {
  configs: {
    "browser-test": require("../lib/configs/browser-test"),
    "chrome-test": require("../lib/configs/chrome-test"),
    "mochitest-test": require("../lib/configs/mochitest-test"),
    recommended: require("../lib/configs/recommended"),
    "xpcshell-test": require("../lib/configs/xpcshell-test"),
  },
  environments: {
    "browser-window": require("../lib/environments/browser-window.js"),
    "chrome-worker": require("../lib/environments/chrome-worker.js"),
    "frame-script": require("../lib/environments/frame-script.js"),
    jsm: require("../lib/environments/jsm.js"),
    simpletest: require("../lib/environments/simpletest.js"),
    sjs: require("../lib/environments/sjs.js"),
    privileged: require("../lib/environments/privileged.js"),
    xpcshell: require("../lib/environments/xpcshell.js"),
  },
  rules: {
    "avoid-Date-timing": require("../lib/rules/avoid-Date-timing"),
    "avoid-removeChild": require("../lib/rules/avoid-removeChild"),
    "balanced-listeners": require("../lib/rules/balanced-listeners"),
    "balanced-observers": require("../lib/rules/balanced-observers"),
    "consistent-if-bracing": require("../lib/rules/consistent-if-bracing"),
    "import-browser-window-globals": require("../lib/rules/import-browser-window-globals"),
    "import-content-task-globals": require("../lib/rules/import-content-task-globals"),
    "import-globals": require("../lib/rules/import-globals"),
    "import-headjs-globals": require("../lib/rules/import-headjs-globals"),
    "lazy-getter-object-name": require("../lib/rules/lazy-getter-object-name"),
    "mark-exported-symbols-as-used": require("../lib/rules/mark-exported-symbols-as-used"),
    "mark-test-function-used": require("../lib/rules/mark-test-function-used"),
    "no-aArgs": require("../lib/rules/no-aArgs"),
    "no-addtask-setup": require("../lib/rules/no-addtask-setup"),
    "no-arbitrary-setTimeout": require("../lib/rules/no-arbitrary-setTimeout"),
    "no-compare-against-boolean-literals": require("../lib/rules/no-compare-against-boolean-literals"),
    "no-define-cc-etc": require("../lib/rules/no-define-cc-etc"),
    "no-throw-cr-literal": require("../lib/rules/no-throw-cr-literal"),
    "no-useless-parameters": require("../lib/rules/no-useless-parameters"),
    "no-useless-removeEventListener": require("../lib/rules/no-useless-removeEventListener"),
    "no-useless-run-test": require("../lib/rules/no-useless-run-test"),
    "prefer-boolean-length-check": require("../lib/rules/prefer-boolean-length-check"),
    "prefer-formatValues": require("../lib/rules/prefer-formatValues"),
    "reject-addtask-only": require("../lib/rules/reject-addtask-only"),
    "reject-chromeutils-import-params": require("../lib/rules/reject-chromeutils-import-params"),
    "reject-eager-module-in-lazy-getter": require("../lib/rules/reject-eager-module-in-lazy-getter"),
    "reject-global-this": require("../lib/rules/reject-global-this"),
    "reject-globalThis-modification": require("../lib/rules/reject-globalThis-modification"),
    "reject-import-system-module-from-non-system": require("../lib/rules/reject-import-system-module-from-non-system"),
    "reject-importGlobalProperties": require("../lib/rules/reject-importGlobalProperties"),
    "reject-osfile": require("../lib/rules/reject-osfile"),
    "reject-scriptableunicodeconverter": require("../lib/rules/reject-scriptableunicodeconverter"),
    "reject-relative-requires": require("../lib/rules/reject-relative-requires"),
    "reject-some-requires": require("../lib/rules/reject-some-requires"),
    "reject-top-level-await": require("../lib/rules/reject-top-level-await"),
    "rejects-requires-await": require("../lib/rules/rejects-requires-await"),
    "use-cc-etc": require("../lib/rules/use-cc-etc"),
    "use-chromeutils-generateqi": require("../lib/rules/use-chromeutils-generateqi"),
    "use-chromeutils-import": require("../lib/rules/use-chromeutils-import"),
    "use-default-preference-values": require("../lib/rules/use-default-preference-values"),
    "use-ownerGlobal": require("../lib/rules/use-ownerGlobal"),
    "use-includes-instead-of-indexOf": require("../lib/rules/use-includes-instead-of-indexOf"),
    "use-isInstance": require("./rules/use-isInstance"),
    "use-returnValue": require("../lib/rules/use-returnValue"),
    "use-services": require("../lib/rules/use-services"),
    "valid-lazy": require("../lib/rules/valid-lazy"),
    "valid-services": require("../lib/rules/valid-services"),
    "var-only-at-top-level": require("../lib/rules/var-only-at-top-level"),
  },
};
