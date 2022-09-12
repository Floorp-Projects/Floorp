/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Realm", "RealmType", "WindowRealm"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  addDebuggerToGlobal: "resource://gre/modules/jsdebugger.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "dbg", () => {
  // eslint-disable-next-line mozilla/reject-globalThis-modification
  lazy.addDebuggerToGlobal(globalThis);
  return new Debugger();
});

/**
 * @typedef {string} RealmType
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

function getUUID() {
  return Services.uuid
    .generateUUID()
    .toString()
    .slice(1, -1);
}

/**
 * Base class that wraps any kind of WebDriver BiDi realm.
 */
class Realm {
  #handleObjectMap;
  #id;

  constructor() {
    this.#id = getUUID();

    // Map of unique handles (UUIDs) to objects belonging to this realm.
    this.#handleObjectMap = new Map();
  }

  destroy() {
    this.#handleObjectMap = null;
  }

  /**
   * Get the unique identifier of the realm instance.
   *
   * @return {string} The unique identifier.
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
   * @param {Object} object
   *     Any non-primitive object.
   * @return {string} The unique handle created for this strong reference.
   */
  getHandleForObject(object) {
    const handle = getUUID();
    this.#handleObjectMap.set(handle, object);
    return handle;
  }

  /**
   * Get the basic realm information.
   *
   * @return {BaseRealmInfo}
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
   * @return {Object} object
   *     Any non-primitive object.
   */
  getObjectForHandle(handle) {
    return this.#handleObjectMap.get(handle);
  }
}

/**
 * Wrapper for Window realms including sandbox objects.
 */
class WindowRealm extends Realm {
  #globalObject;
  #globalObjectReference;
  #sandboxName;
  #window;

  static type = RealmType.Window;

  /**
   *
   * @param {Window} window
   *     The window global to wrap.
   * @param {Object} options
   * @param {string=} options.sandboxName
   *     Name of the sandbox to create if specified. Defaults to `null`.
   */
  constructor(window, options = {}) {
    const { sandboxName = null } = options;

    super();

    this.#sandboxName = sandboxName;
    this.#window = window;
    this.#globalObject =
      sandboxName === null ? this.#window : this.#createSandbox();
    this.#globalObjectReference = lazy.dbg.makeGlobalObjectReference(
      this.#globalObject
    );

    lazy.dbg.enableAsyncStack(this.#globalObject);
  }

  destroy() {
    lazy.dbg.disableAsyncStack(this.#globalObject);

    this.#globalObjectReference = null;
    this.#globalObject = null;
    this.#window = null;

    super.destroy();
  }

  get globalObjectReference() {
    return this.#globalObjectReference;
  }

  get origin() {
    return this.#window.origin;
  }

  #cloneAsDebuggerObject(obj) {
    // To use an object created in the priviledged Debugger compartment from
    // the content compartment, we need to first clone it into the target
    // compartment and then retrieve the corresponding Debugger.Object wrapper.
    const proxyObject = Cu.cloneInto(
      obj,
      this.#globalObjectReference.unsafeDereference()
    );

    return this.#globalObjectReference.makeDebuggeeValue(proxyObject);
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

  /**
   * Evaluates a provided expression in the context of the current realm.
   *
   * @param {string} expression
   *     The expression to evaluate.
   *
   * @return {Object}
   *     - evaluationStatus {EvaluationStatus} One of "normal", "throw".
   *     - exceptionDetails {ExceptionDetails=} the details of the exception if
   *       the evaluation status was "throw".
   *     - result {RemoteValue=} the result of the evaluation serialized as a
   *       RemoteValue if the evaluation status was "normal".
   */
  executeInGlobal(expression) {
    return this.#globalObjectReference.executeInGlobal(expression, {
      url: this.#window.document.baseURI,
    });
  }

  /**
   * Call a function in the context of the current realm.
   *
   * @param {string} functionDeclaration
   *     The body of the function to call.
   * @param {Array<Object>} functionArguments
   *     The arguments to pass to the function call.
   * @param {Object} thisParameter
   *     The value of the `this` keyword for the function call.
   *
   * @return {Object}
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
    const expression = `(${functionDeclaration}).apply(__bidi_this, __bidi_args)`;

    return this.#globalObjectReference.executeInGlobalWithBindings(
      expression,
      {
        __bidi_args: this.#cloneAsDebuggerObject(functionArguments),
        __bidi_this: this.#cloneAsDebuggerObject(thisParameter),
      },
      {
        url: this.#window.document.baseURI,
      }
    );
  }

  /**
   * Get the realm information.
   *
   * @return {Object}
   *     - context {BrowsingContext} The browsing context, associated with the realm.
   *     - id {string} The realm unique identifier.
   *     - origin {string} The serialization of an origin.
   *     - sandbox {string|null} The name of the sandbox.
   *     - type {RealmType.Window} The window realm type.
   */
  getInfo() {
    const baseInfo = super.getInfo();
    return {
      ...baseInfo,
      context: this.#window.browsingContext,
      sandbox: this.#sandboxName,
      type: WindowRealm.type,
    };
  }
}
