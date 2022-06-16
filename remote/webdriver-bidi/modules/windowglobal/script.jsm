/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["script"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  addDebuggerToGlobal: "resource://gre/modules/jsdebugger.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "dbg", () => {
  lazy.addDebuggerToGlobal(globalThis);
  return new Debugger();
});

class ScriptModule extends Module {
  #global;

  constructor(messageHandler) {
    super(messageHandler);
    this.#global = lazy.dbg.makeGlobalObjectReference(
      this.messageHandler.window
    );
  }
  destroy() {
    this.#global = null;
  }

  /**
   * Evaluate a provided expression in the current window global.
   *
   * @param {Object} options
   * @param {string} expression
   *     The expression to evaluate.
   *
   * @return {Object}
   *     - result {RemoteValue} the result of the evaluation serialized as a
   *     RemoteValue.
   */
  evaluateExpression(options) {
    const { expression } = options;
    this.#global.executeInGlobal(expression);

    return {
      result: null,
    };
  }
}

const script = ScriptModule;
