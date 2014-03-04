/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * General utilities used throughout devtools.
 *
 * When using chrome debugging, the debugger server is unable to debug itself.
 * To avoid this, it must be loaded with a custom devtools loader with the
 * invisibleToDebugger flag set to true. Everyone else, though, prefers a JSM.
 */

this.EXPORTED_SYMBOLS = [ "DevToolsUtils" ];

const { devtools } = Components.utils.import("resource://gre/modules/devtools/Loader.jsm", {});
this.DevToolsUtils = devtools.require("devtools/toolkit/DevToolsUtils.js");
