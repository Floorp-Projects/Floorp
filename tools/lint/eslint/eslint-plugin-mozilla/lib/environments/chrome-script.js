/**
 * @fileoverview Defines the environment for SpecialPowers chrome script.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

module.exports = {
  globals: {
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

    // testing/specialpowers/content/SpecialPowersParent.jsm

    // SPLoadChromeScript block
    createWindowlessBrowser: false,
    sendAsyncMessage: false,
    addMessageListener: false,
    removeMessageListener: false,
    actorParent: false,
  },
};
