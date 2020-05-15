/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported withSyncContext */

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/ExtensionCommon.jsm", this);

class KintoExtContext extends ExtensionCommon.BaseContext {
  constructor(principal) {
    super();
    Object.defineProperty(this, "principal", {
      value: principal,
      configurable: true,
    });
    this.sandbox = Cu.Sandbox(principal, { wantXrays: false });
    this.extension = { id: "test@web.extension" };
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
async function withContext(f) {
  const ssm = Services.scriptSecurityManager;
  const PRINCIPAL1 = ssm.createContentPrincipalFromOrigin(
    "http://www.example.org"
  );
  const context = new KintoExtContext(PRINCIPAL1);
  try {
    await f(context);
  } finally {
    await context.unload();
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
async function withSyncContext(f) {
  const STORAGE_SYNC_PREF = "webextensions.storage.sync.enabled";
  let prefs = Services.prefs;

  try {
    prefs.setBoolPref(STORAGE_SYNC_PREF, true);
    await withContext(f);
  } finally {
    prefs.clearUserPref(STORAGE_SYNC_PREF);
  }
}
