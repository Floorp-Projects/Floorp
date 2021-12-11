/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {
  globals: {
    // JS files in this folder are commonly xpcshell scripts where |arguments|
    // is defined in the global scope.
    arguments: false,
  },
};
