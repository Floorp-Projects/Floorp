/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["loadExtScriptInScope"];

/*
 * This file provides the common global scope for all ext-*.js modules. Any
 * variable declared here is automatically shared with the ext-*.js files, so
 * try to keep the number of globals to a minimum.
 *
 * See loadExtScriptInScope below and ExtensionUtils.SchemaAPIManager for more
 * information.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionUtils",
                                  "resource://gre/modules/ExtensionUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "require",
                                  "resource://devtools/shared/Loader.jsm");

let _internalGlobals = new WeakMap();

/**
 * Load an ext-*.js script, with the global namespace set up as follows:
 * - (invisible root)  This JSM's global scope.
 * +- global           The global shared by all scripts in an apiManager.
 *  +- scope           The implied global, visible to the script as `this`.
 *
 * By default variable declarations stay within scope `scope`. If there is a
 * need to share variables with other modules, use `global`.
 *
 * To ensure consistent behavior in SchemaAPIManagers regardless of whether an
 * addon runs in the chrome process or separate addon process, the scope
 * (invisible root) should be avoided. When an ext-*.js script uses `Cu.import`
 * without a second argument, variables appear in (invisible root).
 *
 * @param {string} scriptUrl The local script to load.
 * @param {SchemaAPIManager} apiManager The API manager that is shared with the
 *     script.
 */
this.loadExtScriptInScope = (scriptUrl, apiManager) => {
  // Different SchemaAPIManagers should have different globals.
  let global = _internalGlobals.get(apiManager);
  if (!global) {
    global = Object.create(null);
    global.extensions = apiManager;
    global.global = global;
    global._internalScriptScopes = [];
    _internalGlobals.set(apiManager, global);
  }

  let scope = Object.create(global, {
    console: {
      get() { return ExtensionUtils.console; },
    },
  });

  Services.scriptloader.loadSubScript(scriptUrl, scope, "UTF-8");

  // Save the scope to avoid it being garbage collected.
  global._internalScriptScopes.push(scope);
};

/**
 * Retrieve the shared global for a given type. This enables unit tests to test
 * variables that were explicitly shared via `global`. If the global is not
 * found, an error is thrown.
 *
 * @param {*} type A description of the global.
 *     For now, only "chrome" is supported to get the global from the chrome
 *     process. In the future support for other process types may be added.
 * @returns {object} The global for the given type.
 */
this.getGlobalForTesting = (type) => {
  if (type === "chrome") {
    let {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
    let global = _internalGlobals.get(Management);
    if (global) {
      return global;
    }
    throw new Error("Global not found. Did you really load an ext- script?");
  }
  // For now just throw until we find a need for more types.
  throw new Error(`getGlobalForTesting: Parameter not supported yet: ${type}`);
};
