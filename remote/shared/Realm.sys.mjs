/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  addDebuggerToGlobal: "resource://gre/modules/jsdebugger.sys.mjs",

  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "dbg", () => {
  // eslint-disable-next-line mozilla/reject-globalThis-modification
  lazy.addDebuggerToGlobal(globalThis);
  return new Debugger();
});

/**
 * @typedef {string} RealmType
 */

/**
 * Enum of realm types.
 *
 * @readonly
 * @enum {RealmType}
 */
export const RealmType = {
  AudioWorklet: "audio-worklet",
  DedicatedWorker: "dedicated-worker",
  PaintWorklet: "paint-worklet",
  ServiceWorker: "service-worker",
  SharedWorker: "shared-worker",
  Window: "window",
  Worker: "worker",
  Worklet: "worklet",
};

/**
 * Base class that wraps any kind of WebDriver BiDi realm.
 */
export class Realm {
  #handleObjectMap;
  #id;

  constructor() {
    this.#id = lazy.generateUUID();

    // Map of unique handles (UUIDs) to objects belonging to this realm.
    this.#handleObjectMap = new Map();
  }

  destroy() {
    this.#handleObjectMap = null;
  }

  /**
   * Get the browsing context of the realm instance.
   */
  get browsingContext() {
    return null;
  }

  /**
   * Get the unique identifier of the realm instance.
   *
   * @returns {string} The unique identifier.
   */
  get id() {
    return this.#id;
  }

  /**
   * A getter to get a realm origin.
   *
   * It's required to be implemented in the sub class.
   */
  get origin() {
    throw new Error("Not implemented");
  }

  /**
   * Ensure the provided object can be used within this realm.

   * @param {object} obj
   *     Any non-primitive object.

   * @returns {object}
   *     An object usable in the current realm.
   */
  cloneIntoRealm(obj) {
    return obj;
  }

  /**
   * Remove the reference corresponding to the provided unique handle.
   *
   * @param {string} handle
   *     The unique handle of an object reference tracked in this realm.
   */
  removeObjectHandle(handle) {
    this.#handleObjectMap.delete(handle);
  }

  /**
   * Get a new unique handle for the provided object, creating a strong
   * reference on the object.
   *
   * @param {object} object
   *     Any non-primitive object.
   * @returns {string} The unique handle created for this strong reference.
   */
  getHandleForObject(object) {
    const handle = lazy.generateUUID();
    this.#handleObjectMap.set(handle, object);
    return handle;
  }

  /**
   * Get the basic realm information.
   *
   * @returns {BaseRealmInfo}
   */
  getInfo() {
    return {
      realm: this.#id,
      origin: this.origin,
    };
  }

  /**
   * Retrieve the object corresponding to the provided unique handle.
   *
   * @param {string} handle
   *     The unique handle of an object reference tracked in this realm.
   * @returns {object} object
   *     Any non-primitive object.
   */
  getObjectForHandle(handle) {
    return this.#handleObjectMap.get(handle);
  }
}

/**
 * Wrapper for Window realms including sandbox objects.
 */
export class WindowRealm extends Realm {
  #realmAutomationFeaturesEnabled;
  #globalObject;
  #globalObjectReference;
  #isSandbox;
  #sandboxName;
  #userActivationEnabled;
  #window;

  static type = RealmType.Window;

  /**
   *
   * @param {Window} window
   *     The window global to wrap.
   * @param {object} options
   * @param {string=} options.sandboxName
   *     Name of the sandbox to create if specified. Defaults to `null`.
   */
  constructor(window, options = {}) {
    const { sandboxName = null } = options;

    super();

    this.#isSandbox = sandboxName !== null;
    this.#sandboxName = sandboxName;
    this.#window = window;
    this.#globalObject = this.#isSandbox ? this.#createSandbox() : this.#window;
    this.#globalObjectReference = lazy.dbg.makeGlobalObjectReference(
      this.#globalObject
    );
    this.#realmAutomationFeaturesEnabled = false;
    this.#userActivationEnabled = false;
  }

  destroy() {
    if (this.#realmAutomationFeaturesEnabled) {
      lazy.dbg.disableAsyncStack(this.#globalObject);
      lazy.dbg.disableUnlimitedStacksCapturing(this.#globalObject);
      this.#realmAutomationFeaturesEnabled = false;
    }

    this.#globalObjectReference = null;
    this.#globalObject = null;
    this.#window = null;

    super.destroy();
  }

  get browsingContext() {
    return this.#window.browsingContext;
  }

  get globalObjectReference() {
    return this.#globalObjectReference;
  }

  get isSandbox() {
    return this.#isSandbox;
  }

  get origin() {
    return this.#window.origin;
  }

  get userActivationEnabled() {
    return this.#userActivationEnabled;
  }

  set userActivationEnabled(enable) {
    if (enable === this.#userActivationEnabled) {
      return;
    }

    const document = this.#window.document;
    if (enable) {
      document.notifyUserGestureActivation();
    } else {
      document.clearUserGestureActivation();
    }

    this.#userActivationEnabled = enable;
  }

  #createDebuggerObject(obj) {
    return this.#globalObjectReference.makeDebuggeeValue(obj);
  }

  #createSandbox() {
    const win = this.#window;
    const opts = {
      sameZoneAs: win,
      sandboxPrototype: win,
      wantComponents: false,
      wantXrays: true,
    };

    return new Cu.Sandbox(win, opts);
  }

  #enableRealmAutomationFeatures() {
    if (!this.#realmAutomationFeaturesEnabled) {
      lazy.dbg.enableAsyncStack(this.#globalObject);
      lazy.dbg.enableUnlimitedStacksCapturing(this.#globalObject);
      this.#realmAutomationFeaturesEnabled = true;
    }
  }

  /**
   * Clone the provided object into the scope of this Realm (either a window
   * global, or a sandbox).
   *
   * @param {object} obj
   *     Any non-primitive object.
   *
   * @returns {object}
   *     The cloned object.
   */
  cloneIntoRealm(obj) {
    return Cu.cloneInto(obj, this.#globalObject, { cloneFunctions: true });
  }

  /**
   * Evaluates a provided expression in the context of the current realm.
   *
   * @param {string} expression
   *     The expression to evaluate.
   *
   * @returns {object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *       the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *       RemoteValue if the evaluation status was "normal".
   */
  executeInGlobal(expression) {
    this.#enableRealmAutomationFeatures();
    return this.#globalObjectReference.executeInGlobal(expression, {
      url: this.#window.document.baseURI,
    });
  }

  /**
   * Call a function in the context of the current realm.
   *
   * @param {string} functionDeclaration
   *     The body of the function to call.
   * @param {Array<object>} functionArguments
   *     The arguments to pass to the function call.
   * @param {object} thisParameter
   *     The value of the `this` keyword for the function call.
   *
   * @returns {object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *       the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *       RemoteValue if the evaluation status was "normal".
   */
  executeInGlobalWithBindings(
    functionDeclaration,
    functionArguments,
    thisParameter
  ) {
    this.#enableRealmAutomationFeatures();
    const expression = `(${functionDeclaration}).apply(__bidi_this, __bidi_args)`;

    const args = this.cloneIntoRealm([]);
    for (const arg of functionArguments) {
      args.push(arg);
    }

    return this.#globalObjectReference.executeInGlobalWithBindings(
      expression,
      {
        __bidi_args: this.#createDebuggerObject(args),
        __bidi_this: this.#createDebuggerObject(thisParameter),
      },
      {
        url: this.#window.document.baseURI,
      }
    );
  }

  /**
   * Get the realm information.
   *
   * @returns {object}
   *     - context {BrowsingContext} The browsing context, associated with the realm.
   *     - id {string} The realm unique identifier.
   *     - origin {string} The serialization of an origin.
   *     - sandbox {string=} The name of the sandbox.
   *     - type {RealmType.Window} The window realm type.
   */
  getInfo() {
    const baseInfo = super.getInfo();
    const info = {
      ...baseInfo,
      context: this.#window.browsingContext,
      type: WindowRealm.type,
    };

    if (this.#isSandbox) {
      info.sandbox = this.#sandboxName;
    }

    return info;
  }

  /**
   * Log an error caused by a script evaluation.
   *
   * @param {string} message
   *     The error message.
   * @param {Stack} stack
   *     The JavaScript stack trace.
   */
  reportError(message, stack) {
    const { column, line, source: sourceLine } = stack;

    const scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
    const scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);

    scriptError.initWithWindowID(
      message,
      this.#window.document.baseURI,
      sourceLine,
      line,
      column,
      Ci.nsIScriptError.errorFlag,
      "content javascript",
      this.#window.windowGlobalChild.innerWindowId
    );
    Services.console.logMessage(scriptError);
  }
}
