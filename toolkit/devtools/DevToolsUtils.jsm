/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * General utilities used throughout devtools.
 *
 * To support chrome debugging, the debugger server needs to have all its
 * code in one global, so it must use loadSubScript directly. Everyone else,
 * though, prefers a JSM.
 */

this.EXPORTED_SYMBOLS = [ "DevToolsUtils" ];

Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Components.interfaces.mozIJSSubScriptLoader)
  .loadSubScript("resource://gre/modules/devtools/DevToolsUtils.js", this);

this.DevToolsUtils = {
  safeErrorString: safeErrorString,
  reportException: reportException,
  makeInfallible: makeInfallible,
  yieldingEach: yieldingEach,
  defineLazyPrototypeGetter: defineLazyPrototypeGetter
};
