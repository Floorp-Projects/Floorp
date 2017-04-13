/**
 * @fileoverview Defines the environment for chrome workers. This differs
 *               from normal workers by the fact that `ctypes` can be accessed
 *               as well.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var globals = require("globals");
var util = require("util");

var workerGlobals = util._extend({
  ctypes: false
}, globals.worker);

module.exports = {
  globals: workerGlobals
};
