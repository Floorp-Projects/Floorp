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
    "recommended": require("../lib/configs/recommended"),
    "xpcshell-test": require("../lib/configs/xpcshell-test")
  },
  environments: {
    "browser-window": require("../lib/environments/browser-window.js"),
    "chrome-worker": require("../lib/environments/chrome-worker.js"),
    "frame-script": require("../lib/environments/frame-script.js"),
    "jsm": require("../lib/environments/jsm.js"),
    "places-overlay": require("../lib/environments/places-overlay.js"),
    "simpletest": require("../lib/environments/simpletest.js")
  },
  processors: {
    ".xml": require("../lib/processors/xbl-bindings")
  },
  rules: {
    "avoid-Date-timing": require("../lib/rules/avoid-Date-timing"),
    "avoid-removeChild": require("../lib/rules/avoid-removeChild"),
    "balanced-listeners": require("../lib/rules/balanced-listeners"),
    "import-browser-window-globals":
      require("../lib/rules/import-browser-window-globals"),
    "import-content-task-globals":
      require("../lib/rules/import-content-task-globals"),
    "import-globals": require("../lib/rules/import-globals"),
    "import-headjs-globals": require("../lib/rules/import-headjs-globals"),
    "mark-test-function-used": require("../lib/rules/mark-test-function-used"),
    "no-aArgs": require("../lib/rules/no-aArgs"),
    "no-arbitrary-setTimeout": require("../lib/rules/no-arbitrary-setTimeout"),
    "no-cpows-in-tests": require("../lib/rules/no-cpows-in-tests"),
    "no-single-arg-cu-import": require("../lib/rules/no-single-arg-cu-import"),
    "no-import-into-var-and-global":
      require("../lib/rules/no-import-into-var-and-global.js"),
    "no-task": require("../lib/rules/no-task"),
    "no-useless-parameters": require("../lib/rules/no-useless-parameters"),
    "no-useless-removeEventListener":
      require("../lib/rules/no-useless-removeEventListener"),
    "no-useless-run-test":
      require("../lib/rules/no-useless-run-test"),
    "reject-importGlobalProperties":
      require("../lib/rules/reject-importGlobalProperties"),
    "reject-some-requires": require("../lib/rules/reject-some-requires"),
    "use-default-preference-values":
      require("../lib/rules/use-default-preference-values"),
    "use-ownerGlobal": require("../lib/rules/use-ownerGlobal"),
    "use-services": require("../lib/rules/use-services"),
    "var-only-at-top-level": require("../lib/rules/var-only-at-top-level")
  },
  rulesConfig: {
    "avoid-Date-timing": "off",
    "avoid-removeChild": "off",
    "balanced-listeners": "off",
    "import-browser-window-globals": "off",
    "import-content-task-globals": "off",
    "import-globals": "off",
    "import-headjs-globals": "off",
    "mark-test-function-used": "off",
    "no-aArgs": "off",
    "no-arbitrary-setTimeout": "off",
    "no-cpows-in-tests": "off",
    "no-single-arg-cu-import": "off",
    "no-import-into-var-and-global": "off",
    "no-task": "off",
    "no-useless-parameters": "off",
    "no-useless-run-test": "off",
    "no-useless-removeEventListener": "off",
    "reject-importGlobalProperties": "off",
    "reject-some-requires": "off",
    "use-default-preference-values": "off",
    "use-ownerGlobal": "off",
    "use-services": "off",
    "var-only-at-top-level": "off"
  }
};
