/**
 * @fileoverview Defines the environment for SpecialPowers chrome script.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

var { globals } = require("./special-powers-sandbox");
var util = require("util");

module.exports = {
  globals: util._extend(
    {
      // testing/specialpowers/content/SpecialPowersParent.jsm

      // SPLoadChromeScript block
      createWindowlessBrowser: false,
      sendAsyncMessage: false,
      addMessageListener: false,
      removeMessageListener: false,
      actorParent: false,
    },
    globals
  ),
};
