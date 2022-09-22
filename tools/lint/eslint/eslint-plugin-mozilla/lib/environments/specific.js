/**
 * @fileoverview Defines the environment for the Firefox browser. Allows global
 *               variables which are non-standard and specific to Firefox.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  globals: {
    Cc: false,
    ChromeUtils: false,
    Ci: false,
    Components: false,
    Cr: false,
    Cu: false,
    Debugger: false,
    InstallTrigger: false,
    // https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/InternalError
    InternalError: true,
    Services: false,
    // https://developer.mozilla.org/docs/Web/API/Window/dump
    dump: true,
    openDialog: false,
    // https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/uneval
    uneval: false,
  },
};
