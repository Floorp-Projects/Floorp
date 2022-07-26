/**
 * @fileoverview Defines the environment for SpecialPowers sandbox.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  globals: {
    // wantComponents defaults to true,
    Components: false,
    Ci: false,
    Cr: false,
    Cc: false,
    Cu: false,
    Services: false,

    // testing/specialpowers/content/SpecialPowersSandbox.jsm

    // SANDBOX_GLOBALS
    Blob: false,
    ChromeUtils: false,
    FileReader: false,
    TextDecoder: false,
    TextEncoder: false,
    URL: false,

    // EXTRA_IMPORTS
    EventUtils: false,

    // SpecialPowersSandbox constructor
    assert: false,
    Assert: false,
    BrowsingContext: false,
    InspectorUtils: false,
    ok: false,
    is: false,
    isnot: false,
    todo: false,
    todo_is: false,
    info: false,
  },
};
