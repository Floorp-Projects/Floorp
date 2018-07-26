/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://normandy/lib/NormandyDriver.jsm");
ChromeUtils.import("resource://normandy/lib/SandboxManager.jsm");
ChromeUtils.import("resource://normandy/lib/LogManager.jsm");

var EXPORTED_SYMBOLS = ["ActionSandboxManager"];

/**
 * An extension to SandboxManager that prepares a sandbox for executing
 * Normandy actions.
 *
 * Actions register a set of named callbacks, which this class makes available
 * for execution. This allows a single action script to define multiple,
 * independent steps that execute in isolated sandboxes.
 *
 * Callbacks are assumed to be async and must return Promises.
 */
var ActionSandboxManager = class extends SandboxManager {
  constructor(actionScript) {
    super();

    // Prepare the sandbox environment
    const driver = new NormandyDriver(this);
    this.cloneIntoGlobal("sandboxedDriver", driver, {cloneFunctions: true});
    this.evalInSandbox(`
      // Shim old API for registering actions
      function registerAction(name, Action) {
        registerAsyncCallback("action", (driver, recipe) => {
          return new Action(driver, recipe).execute();
        });
      };

      this.asyncCallbacks = new Map();
      function registerAsyncCallback(name, callback) {
        asyncCallbacks.set(name, callback);
      }

      this.window = this;
      this.setTimeout = sandboxedDriver.setTimeout;
      this.clearTimeout = sandboxedDriver.clearTimeout;
    `);
    this.evalInSandbox(actionScript);
  }

  /**
   * Execute a callback in the sandbox with the given name. If the script does
   * not register a callback with the given name, we log a message and return.
   * @param {String} callbackName
   * @param {...*} [args]
   *   Remaining arguments are cloned into the sandbox and passed as arguments
   *   to the callback.
   * @resolves
   *   The return value of the callback, cloned into the current compartment, or
   *   undefined if a matching callback was not found.
   * @rejects
   *   If the sandbox rejects, an error object with the message from the sandbox
   *   error. Due to sandbox limitations, the stack trace is not preserved.
   */
  async runAsyncCallback(callbackName, ...args) {
    const callbackWasRegistered = this.evalInSandbox(`asyncCallbacks.has("${callbackName}")`);
    if (!callbackWasRegistered) {
      return undefined;
    }

    this.cloneIntoGlobal("callbackArgs", args);
    const result = await this.evalInSandbox(`
      asyncCallbacks.get("${callbackName}")(sandboxedDriver, ...callbackArgs);
    `);
    return Cu.cloneInto(result, {});
  }
};
