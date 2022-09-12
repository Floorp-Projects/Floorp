/**
 * @fileoverview Ensures that Services uses have valid property names.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const helpers = require("../helpers");

module.exports = {
  meta: {
    docs: {
      url:
        "https://firefox-source-docs.mozilla.org/code-quality/lint/linters/eslint-plugin-mozilla/valid-services.html",
    },
    type: "problem",
  },

  create(context) {
    let servicesInterfaceMap = helpers.servicesData;
    let serviceAliases = new Set([
      ...Object.values(servicesInterfaceMap),
      // This is defined only for Android, so most builds won't pick it up.
      "androidBridge",
      // These are defined without interfaces and hence are not in the services map.
      "cpmm",
      "crashmanager",
      "mm",
      "ppmm",
      // The new xulStore also does not have an interface.
      "xulStore",
    ]);
    return {
      MemberExpression(node) {
        if (node.computed || node.object.type !== "Identifier") {
          return;
        }

        let alias;
        if (node.object.name == "Services") {
          alias = node.property.name;
        } else if (
          node.property.name == "Services" &&
          node.parent.type == "MemberExpression"
        ) {
          alias = node.parent.property.name;
        } else {
          return;
        }

        if (!serviceAliases.has(alias)) {
          context.report(node, `Unknown Services member property ${alias}`);
        }
      },
    };
  },
};
