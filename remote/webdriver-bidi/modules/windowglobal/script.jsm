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

  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "dbg", () => {
  // eslint-disable-next-line mozilla/reject-globalThis-modification
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

  #toRawObject(maybeDebuggerObject) {
    if (maybeDebuggerObject instanceof Debugger.Object) {
      // Retrieve the referent for the provided Debugger.object.
      // See https://firefox-source-docs.mozilla.org/devtools-user/debugger-api/debugger.object/index.html
      const rawObject = maybeDebuggerObject.unsafeDereference();

      // TODO: Getters for Maps and Sets iterators return "Opaque" objects and
      // are not iterable. RemoteValue.jsm' serializer should handle calling
      // waiveXrays on Maps/Sets/... and then unwaiveXrays on entries but since
      // we serialize with maxDepth=1, calling waiveXrays once on the root
      // object allows to return correctly serialized values.
      return Cu.waiveXrays(rawObject);
    }

    // If maybeDebuggerObject was not a Debugger.Object, it is a primitive value
    // which can be used as is.
    return maybeDebuggerObject;
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
    const rv = this.#global.executeInGlobal(expression);

    if ("return" in rv) {
      return {
        result: lazy.serialize(this.#toRawObject(rv.return), 1),
      };
    }

    throw new lazy.error.UnsupportedOperationError(
      `Unsupported completion value for expression evaluation`
    );
  }
}

const script = ScriptModule;
