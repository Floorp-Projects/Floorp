/**
 * @fileoverview Reject some uses of require.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var helpers = require("../helpers");

const isRelativePath = function(path) {
  return path.startsWith("./") || path.startsWith("../");
};

module.exports = {
  create(context) {
    return {
      CallExpression(node) {
        const path = helpers.getDevToolsRequirePath(node);
        if (path && isRelativePath(path)) {
          context.report(node, "relative paths are not allowed with require()");
        }
      },
    };
  },
};
