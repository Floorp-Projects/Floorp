/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported withSyncContext */

Components.utils.import("resource://gre/modules/Services.jsm", this);
Components.utils.import("resource://gre/modules/ExtensionCommon.jsm", this);

var {
  BaseContext,
} = ExtensionCommon;

class Context extends BaseContext {
  constructor(principal) {
    super();
    Object.defineProperty(this, "principal", {
      value: principal,
      configurable: true,
    });
    this.sandbox = Components.utils.Sandbox(principal, {wantXrays: false});
    this.extension = {id: "test@web.extension"};
  }

  get cloneScope() {
    return this.sandbox;
  }
}

/**
 * Call the given function with a newly-constructed context.
 * Unload the context on the way out.
 *
 * @param {function} f    the function to call
 */
function* withContext(f) {
  const ssm = Services.scriptSecurityManager;
  const PRINCIPAL1 = ssm.createCodebasePrincipalFromOrigin("http://www.example.org");
  const context = new Context(PRINCIPAL1);
  try {
    yield* f(context);
  } finally {
    yield context.unload();
  }
}

/**
 * Like withContext(), but also turn on the "storage.sync" pref for
 * the duration of the function.
 * Calls to this function can be replaced with calls to withContext
 * once the pref becomes on by default.
 *
 * @param {function} f    the function to call
 */
function* withSyncContext(f) {
  const STORAGE_SYNC_PREF = "webextensions.storage.sync.enabled";
  let prefs = Services.prefs;

  try {
    prefs.setBoolPref(STORAGE_SYNC_PREF, true);
    yield* withContext(f);
  } finally {
    prefs.clearUserPref(STORAGE_SYNC_PREF);
  }
}
