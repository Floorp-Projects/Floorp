/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Runtime"];

const { ContentProcessDomain } = ChromeUtils.import(
  "chrome://remote/content/domains/ContentProcessDomain.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { ExecutionContext } = ChromeUtils.import(
  "chrome://remote/content/domains/content/runtime/ExecutionContext.jsm"
);
const { addDebuggerToGlobal } = ChromeUtils.import(
  "resource://gre/modules/jsdebugger.jsm",
  {}
);

// Import the `Debugger` constructor in the current scope
addDebuggerToGlobal(Cu.getGlobalForObject(this));

class SetMap extends Map {
  constructor() {
    super();
    this._count = 1;
  }
  // Every key in the map is associated with a Set.
  // The first time `key` is used `obj.set(key, value)` maps `key` to
  // to `Set(value)`. Subsequent calls add more values to the Set for `key`.
  // Note that `obj.get(key)` will return undefined if there's no such key,
  // as in a regular Map.
  set(key, value) {
    const innerSet = this.get(key);
    if (innerSet) {
      innerSet.add(value);
    } else {
      super.set(key, new Set([value]));
    }
    this._count++;
    return this;
  }
  // used as ExecutionContext id
  get count() {
    return this._count;
  }
}

class Runtime extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;

    // Map of all the ExecutionContext instances:
    // [id (Number) => ExecutionContext instance]
    this.contexts = new Map();
    // [innerWindowId (Number) => Set of ExecutionContext instances]
    this.contextsByWindow = new SetMap();

    this._onContextCreated = this._onContextCreated.bind(this);
    this._onContextDestroyed = this._onContextDestroyed.bind(this);
    // TODO Bug 1602083
    this.contextObserver.on("context-created", this._onContextCreated);
    this.contextObserver.on("context-destroyed", this._onContextDestroyed);
  }

  destructor() {
    this.disable();
    this.contextObserver.off("context-created", this._onContextCreated);
    this.contextObserver.off("context-destroyed", this._onContextDestroyed);
    super.destructor();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;

      // Spin the event loop in order to send the `executionContextCreated` event right
      // after we replied to `enable` request.
      Services.tm.dispatchToMainThread(() => {
        this._onContextCreated("context-created", {
          windowId: this.content.windowUtils.currentInnerWindowID,
          window: this.content,
          isDefault: true,
        });
      });
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
    }
  }

  releaseObject({ objectId }) {
    let context = null;
    for (const ctx of this.contexts.values()) {
      if (ctx.hasRemoteObject(objectId)) {
        context = ctx;
        break;
      }
    }
    if (!context) {
      throw new Error(`Unable to get execution context by ID: ${objectId}`);
    }
    context.releaseObject(objectId);
  }

  /**
   * Calls function with given declaration on the given object.
   *
   * Object group of the result is inherited from the target object.
   *
   * @param {Object} options
   * @param {string} options.functionDeclaration
   *     Declaration of the function to call.
   * @param {Array.<Object>=} options.arguments
   *     Call arguments. All call arguments must belong to the same
   *     JavaScript world as the target object.
   * @param {boolean=} options.awaitPromise
   *     Whether execution should `await` for resulting value
   *     and return once awaited promise is resolved.
   * @param {number=} options.executionContextId
   *     Specifies execution context which global object will be used
   *     to call function on. Either executionContextId or objectId
   *     should be specified.
   * @param {string=} options.objectId
   *     Identifier of the object to call function on.
   *     Either objectId or executionContextId should be specified.
   * @param {boolean=} options.returnByValue
   *     Whether the result is expected to be a JSON object
   *     which should be sent by value.
   *
   * @return {Object.<RemoteObject, ExceptionDetails>}
   */
  callFunctionOn(options = {}) {
    if (typeof options.functionDeclaration != "string") {
      throw new TypeError("functionDeclaration: string value expected");
    }
    if (
      typeof options.arguments != "undefined" &&
      !Array.isArray(options.arguments)
    ) {
      throw new TypeError("arguments: array value expected");
    }
    if (!["undefined", "boolean"].includes(typeof options.awaitPromise)) {
      throw new TypeError("awaitPromise: boolean value expected");
    }
    if (!["undefined", "number"].includes(typeof options.executionContextId)) {
      throw new TypeError("executionContextId: number value expected");
    }
    if (!["undefined", "string"].includes(typeof options.objectId)) {
      throw new TypeError("objectId: string value expected");
    }
    if (!["undefined", "boolean"].includes(typeof options.returnByValue)) {
      throw new TypeError("returnByValue: boolean value expected");
    }

    if (
      typeof options.executionContextId == "undefined" &&
      typeof options.objectId == "undefined"
    ) {
      throw new Error(
        "Either objectId or executionContextId must be specified"
      );
    }

    let context = null;
    // When an `objectId` is passed, we want to execute the function of a given object
    // So we first have to find its ExecutionContext
    if (options.objectId) {
      for (const ctx of this.contexts.values()) {
        if (ctx.hasRemoteObject(options.objectId)) {
          context = ctx;
          break;
        }
      }
      if (!context) {
        throw new Error(
          `Unable to get the context for object with id: ${options.objectId}`
        );
      }
    } else {
      context = this.contexts.get(options.executionContextId);
      if (!context) {
        throw new Error("Cannot find context with specified id");
      }
    }

    return context.callFunctionOn(
      options.functionDeclaration,
      options.arguments,
      options.returnByValue,
      options.awaitPromise,
      options.objectId
    );
  }

  /**
   * Evaluate expression on global object.
   *
   * @param {Object} options
   * @param {string} options.expression
   *     Expression to evaluate.
   * @param {boolean=} options.awaitPromise [unsupported]
   *     Whether execution should `await` for resulting value
   *     and return once awaited promise is resolved.
   * @param {number=} options.contextId
   *     Specifies in which execution context to perform evaluation.
   *     If the parameter is omitted the evaluation will be performed
   *     in the context of the inspected page.
   * @param {boolean=} options.returnByValue
   *     Whether the result is expected to be a JSON object
   *     that should be sent by value. Defaults to false.
   * @param {boolean=} options.userGesture [unsupported]
   *     Whether execution should be treated as initiated by user in the UI.
   *
   * @return {Object<RemoteObject, exceptionDetails>}
   *     The evaluation result, and optionally exception details.
   */
  evaluate(options = {}) {
    const { expression, contextId, returnByValue = false } = options;

    if (typeof expression != "string") {
      throw new Error("expression: string value expected");
    }
    if (typeof returnByValue != "boolean") {
      throw new Error("returnByValue: boolean value expected");
    }

    let context;
    if (typeof contextId != "undefined") {
      context = this.contexts.get(contextId);
      if (!context) {
        throw new Error("Cannot find context with specified id");
      }
    } else {
      context = this._getDefaultContextForWindow();
    }

    return context.evaluate(expression, returnByValue);
  }

  getProperties({ objectId, ownProperties }) {
    for (const ctx of this.contexts.values()) {
      const obj = ctx.getRemoteObject(objectId);
      if (typeof obj != "undefined") {
        return ctx.getProperties({ objectId, ownProperties });
      }
    }
    return null;
  }

  /**
   * Internal methods: the following methods are not part of CDP;
   * note the _ prefix.
   */

  get _debugger() {
    if (this.__debugger) {
      return this.__debugger;
    }
    this.__debugger = new Debugger();
    return this.__debugger;
  }

  _getRemoteObject(objectId) {
    for (const ctx of this.contexts.values()) {
      const obj = ctx.getRemoteObject(objectId);
      if (typeof obj != "undefined") {
        return obj;
      }
    }
    return null;
  }

  _getDefaultContextForWindow(innerWindowId) {
    if (!innerWindowId) {
      const { windowUtils } = this.content;
      innerWindowId = windowUtils.currentInnerWindowID;
    }
    const curContexts = this.contextsByWindow.get(innerWindowId);
    if (curContexts) {
      for (const ctx of curContexts) {
        if (ctx.isDefault) {
          return ctx;
        }
      }
    }
    return null;
  }

  _getContextsForFrame(frameId) {
    const frameContexts = [];
    for (const ctx of this.contexts.values()) {
      if (ctx.frameId == frameId) {
        frameContexts.push(ctx);
      }
    }
    return frameContexts;
  }

  /**
   * Helper method in order to instantiate the ExecutionContext for a given
   * DOM Window as well as emitting the related
   * `Runtime.executionContextCreated` event
   *
   * @param {string} name
   *     Event name
   * @param {Object=} options
   * @param {number} options.windowId
   *     The inner window id of the newly instantiated document.
   * @param {Window} options.window
   *     The window object of the newly instantiated document.
   * @param {string=} options.contextName
   *     Human-readable name to describe the execution context.
   * @param {boolean=} options.isDefault
   *     Whether the execution context is the default one.
   * @param {string=} options.contextType
   *     "default" or "isolated"
   *
   * @return {number} ID of created context
   *
   */
  _onContextCreated(name, options = {}) {
    const {
      windowId,
      window,
      contextName = "",
      isDefault = options.window == this.content,
      contextType = options.contextType ||
        (options.window == this.content ? "default" : ""),
    } = options;

    if (windowId === undefined) {
      throw new Error("windowId is required");
    }

    // allow only one default context per inner window
    if (isDefault && this.contextsByWindow.has(windowId)) {
      for (const ctx of this.contextsByWindow.get(windowId)) {
        if (ctx.isDefault) {
          return null;
        }
      }
    }

    const context = new ExecutionContext(
      this._debugger,
      window,
      this.contextsByWindow.count,
      isDefault
    );
    this.contexts.set(context.id, context);
    this.contextsByWindow.set(windowId, context);

    if (this.enabled) {
      this.emit("Runtime.executionContextCreated", {
        context: {
          id: context.id,
          origin: window.location.href,
          name: contextName,
          auxData: {
            isDefault,
            frameId: context.frameId,
            type: contextType,
          },
        },
      });
    }

    return context.id;
  }

  /**
   * Helper method to destroy the ExecutionContext of the given id. Also emit
   * the related `Runtime.executionContextDestroyed` and
   * `Runtime.executionContextsCleared` events.
   * ContextObserver will call this method with either `id` or `frameId` argument
   * being set.
   *
   * @param {string} name
   *     Event name
   * @param {Object=} options
   * @param {number} id
   *     The execution context id to destroy.
   * @param {number} windowId
   *     The inner-window id of the execution context to destroy.
   * @param {string} frameId
   *     The frame id of execution context to destroy.
   * Either `id` or `frameId` or `windowId` is passed.
   */
  _onContextDestroyed(name, { id, frameId, windowId }) {
    let contexts;
    if ([id, frameId, windowId].filter(id => !!id).length > 1) {
      throw new Error("Expects only *one* of id, frameId, windowId");
    }

    if (id) {
      contexts = [this.contexts.get(id)];
    } else if (frameId) {
      contexts = this._getContextsForFrame(frameId);
    } else {
      contexts = this.contextsByWindow.get(windowId) || [];
    }

    for (const ctx of contexts) {
      ctx.destructor();
      this.contexts.delete(ctx.id);
      this.contextsByWindow.get(ctx.windowId).delete(ctx);
      if (this.enabled) {
        this.emit("Runtime.executionContextDestroyed", {
          executionContextId: ctx.id,
        });
      }
      if (this.contextsByWindow.get(ctx.windowId).size == 0) {
        this.contextsByWindow.delete(ctx.windowId);
        this.emit("Runtime.executionContextsCleared");
      }
    }
  }
}
