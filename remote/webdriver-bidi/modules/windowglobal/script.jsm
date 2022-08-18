/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["script"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const { Module } = ChromeUtils.import(
  "chrome://remote/content/shared/messagehandler/Module.jsm"
);

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  addDebuggerToGlobal: "resource://gre/modules/jsdebugger.jsm",

  deserialize: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  getFramesFromStack: "chrome://remote/content/shared/Stack.jsm",
  isChromeFrame: "chrome://remote/content/shared/Stack.jsm",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
  stringify: "chrome://remote/content/webdriver-bidi/RemoteValue.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "dbg", () => {
  // eslint-disable-next-line mozilla/reject-globalThis-modification
  lazy.addDebuggerToGlobal(globalThis);
  return new Debugger();
});

/**
 * @typedef {Object} EvaluationStatus
 **/

/**
 * Enum of possible evaluation states.
 *
 * @readonly
 * @enum {EvaluationStatus}
 **/
const EvaluationStatus = {
  Normal: "normal",
  Throw: "throw",
};

/**
 * @typedef {Object} RealmType
 **/

/**
 * Enum of realm types.
 *
 * @readonly
 * @enum {RealmType}
 **/
const RealmType = {
  AudioWorklet: "audio-worklet",
  DedicatedWorker: "dedicated-worker",
  PaintWorklet: "paint-worklet",
  ServiceWorker: "service-worker",
  SharedWorker: "shared-worker",
  Window: "window",
  Worker: "worker",
  Worklet: "worklet",
};

const WINDOW_GLOBAL_KEY = Symbol();

class ScriptModule extends Module {
  #globals;
  #realmIds;

  constructor(messageHandler) {
    super(messageHandler);

    // Maps sandbox names as keys to sandboxes as values and
    // WINDOW_GLOBAL_KEY key to default window-global
    this.#globals = new Map();
    // Maps default window-global and sandboxes as keys to realmIds as values
    this.#realmIds = new WeakMap();
  }

  destroy() {
    for (const global of this.#globals.values()) {
      lazy.dbg.disableAsyncStack(global.unsafeDereference());
    }
    this.#globals = null;
    this.#realmIds = null;
  }

  #buildExceptionDetails(exception, stack) {
    exception = this.#toRawObject(exception);
    const frames = lazy.getFramesFromStack(stack) || [];

    const callFrames = frames
      // Remove chrome/internal frames
      .filter(frame => !lazy.isChromeFrame(frame))
      // Translate frames from getFramesFromStack to frames expected by
      // WebDriver BiDi.
      .map(frame => {
        return {
          columnNumber: frame.columnNumber,
          functionName: frame.functionName,
          lineNumber: frame.lineNumber - 1,
          url: frame.filename,
        };
      });

    return {
      columnNumber: stack.column,
      exception: lazy.serialize(exception, 1),
      lineNumber: stack.line - 1,
      stackTrace: { callFrames },
      text: lazy.stringify(exception),
    };
  }

  async #buildReturnValue(rv, awaitPromise, globalObjectReference) {
    let evaluationStatus, exception, result, stack;
    const realm = this.#getRealm(globalObjectReference);
    if ("return" in rv) {
      evaluationStatus = EvaluationStatus.Normal;
      if (
        awaitPromise &&
        // Only non-primitive return values are wrapped in Debugger.Object.
        rv.return instanceof Debugger.Object &&
        rv.return.isPromise
      ) {
        try {
          // Force wrapping the promise resolution result in a Debugger.Object
          // wrapper for consistency with the synchronous codepath.
          const asyncResult = await rv.return.unsafeDereference();
          result = globalObjectReference.makeDebuggeeValue(asyncResult);
        } catch (asyncException) {
          evaluationStatus = EvaluationStatus.Throw;
          exception = globalObjectReference.makeDebuggeeValue(asyncException);
          stack = rv.return.promiseResolutionSite;
        }
      } else {
        // rv.return is a Debugger.Object or a primitive.
        result = rv.return;
      }
    } else if ("throw" in rv) {
      // rv.throw will be set if the evaluation synchronously failed, either if
      // the script contains a syntax error or throws an exception.
      evaluationStatus = EvaluationStatus.Throw;
      exception = rv.throw;
      stack = rv.stack;
    }

    switch (evaluationStatus) {
      case EvaluationStatus.Normal:
        return {
          evaluationStatus,
          result: lazy.serialize(this.#toRawObject(result), 1),
          realm,
        };
      case EvaluationStatus.Throw:
        return {
          evaluationStatus,
          exceptionDetails: this.#buildExceptionDetails(exception, stack),
          realm,
        };
      default:
        throw new lazy.error.UnsupportedOperationError(
          `Unsupported completion value for expression evaluation`
        );
    }
  }

  #buildRealmId(sandboxName) {
    let realmId = String(this.messageHandler.innerWindowId);
    if (sandboxName) {
      realmId += "-sandbox-" + sandboxName;
    }
    return realmId;
  }

  #cloneAsDebuggerObject(obj, globalObjectReference) {
    // To use an object created in the priviledged Debugger compartment from
    // the content compartment, we need to first clone it into the target
    // compartment and then retrieve the corresponding Debugger.Object wrapper.
    const proxyObject = Cu.cloneInto(
      obj,
      globalObjectReference.unsafeDereference()
    );
    return globalObjectReference.makeDebuggeeValue(proxyObject);
  }

  #createSandbox() {
    const win = this.messageHandler.window;
    const opts = {
      sameZoneAs: win,
      sandboxPrototype: win,
      wantComponents: false,
      wantXrays: true,
    };

    return new Cu.Sandbox(win, opts);
  }

  #getGlobalObjectReference(sandboxName) {
    const key = sandboxName !== null ? sandboxName : WINDOW_GLOBAL_KEY;
    if (this.#globals.has(key)) {
      return this.#globals.get(key);
    }

    const globalObject =
      key == WINDOW_GLOBAL_KEY
        ? this.messageHandler.window
        : this.#createSandbox();

    const globalObjectReference = lazy.dbg.makeGlobalObjectReference(
      globalObject
    );
    lazy.dbg.enableAsyncStack(globalObject);

    this.#globals.set(key, globalObjectReference);
    this.#realmIds.set(globalObjectReference, this.#buildRealmId(sandboxName));

    return globalObjectReference;
  }

  #getRealm(globalObjectReference) {
    // TODO: Return an actual realm once we have proper realm support.
    // See Bug 1766240.
    return {
      realm: this.#realmIds.get(globalObjectReference),
      origin: null,
      type: RealmType.Window,
    };
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
   * Call a function in the current window global.
   *
   * @param {Object} options
   * @param {boolean} awaitPromise
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise.
   * @param {Array<RemoteValue>=} commandArguments
   *     The arguments to pass to the function call.
   * @param {string} functionDeclaration
   *     The body of the function to call.
   * @param {string=} sandbox
   *     The name of the sandbox.
   * @param {RemoteValue=} thisParameter
   *     The value of the this keyword for the function call.
   *
   * @return {Object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *     the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *     RemoteValue if the evaluation status was "normal".
   */
  async callFunctionDeclaration(options) {
    const {
      awaitPromise,
      commandArguments = null,
      functionDeclaration,
      sandbox: sandboxName = null,
      thisParameter = null,
    } = options;

    const deserializedArguments =
      commandArguments != null
        ? commandArguments.map(a => lazy.deserialize(a))
        : [];

    const deserializedThis =
      thisParameter != null ? lazy.deserialize(thisParameter) : null;

    const expression = `(${functionDeclaration}).apply(__bidi_this, __bidi_args)`;

    const globalObjectReference = this.#getGlobalObjectReference(sandboxName);
    const rv = globalObjectReference.executeInGlobalWithBindings(
      expression,
      {
        __bidi_args: this.#cloneAsDebuggerObject(
          deserializedArguments,
          globalObjectReference
        ),
        __bidi_this: this.#cloneAsDebuggerObject(
          deserializedThis,
          globalObjectReference
        ),
      },
      {
        url: this.messageHandler.window.document.baseURI,
      }
    );

    return this.#buildReturnValue(rv, awaitPromise, globalObjectReference);
  }

  /**
   * Evaluate a provided expression in the current window global.
   *
   * @param {Object} options
   * @param {boolean} awaitPromise
   *     Determines if the command should wait for the return value of the
   *     expression to resolve, if this return value is a Promise.
   * @param {string} expression
   *     The expression to evaluate.
   * @param {string=} sandbox
   *     The name of the sandbox.
   *
   * @return {Object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *     the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *     RemoteValue if the evaluation status was "normal".
   */
  async evaluateExpression(options) {
    const { awaitPromise, expression, sandbox: sandboxName = null } = options;

    const globalObjectReference = this.#getGlobalObjectReference(sandboxName);
    const rv = globalObjectReference.executeInGlobal(expression, {
      url: this.messageHandler.window.document.baseURI,
    });

    return this.#buildReturnValue(rv, awaitPromise, globalObjectReference);
  }
}

const script = ScriptModule;
