/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Runtime"];

const {ContentProcessDomain} = ChromeUtils.import("chrome://remote/content/domains/ContentProcessDomain.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {ExecutionContext} = ChromeUtils.import("chrome://remote/content/domains/content/runtime/ExecutionContext.jsm");
const {addDebuggerToGlobal} = ChromeUtils.import("resource://gre/modules/jsdebugger.jsm", {});

// Import the `Debugger` constructor in the current scope
addDebuggerToGlobal(Cu.getGlobalForObject(this));

class Runtime extends ContentProcessDomain {
  constructor(session) {
    super(session);
    this.enabled = false;

    // Map of all the ExecutionContext instances:
    // [Execution context id (Number) => ExecutionContext instance]
    this.contexts = new Map();

    this.onContextCreated = this.onContextCreated.bind(this);
    this.onContextDestroyed = this.onContextDestroyed.bind(this);
  }

  destructor() {
    this.disable();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      this.contextObserver.on("context-created", this.onContextCreated);
      this.contextObserver.on("context-destroyed", this.onContextDestroyed);

      // Spin the event loop in order to send the `executionContextCreated` event right
      // after we replied to `enable` request.
      Services.tm.dispatchToMainThread(() => {
        this.onContextCreated("context-created", {
          id: this.content.windowUtils.currentInnerWindowID,
          window: this.content,
        });
      });
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
      this.contextObserver.off("context-created", this.onContextCreated);
      this.contextObserver.off("context-destroyed", this.onContextDestroyed);
    }
  }

  evaluate(request) {
    const context = this.contexts.get(request.contextId);
    if (!context) {
      throw new Error(`Unable to find execution context with id: ${request.contextId}`);
    }
    if (typeof(request.expression) != "string") {
      throw new Error(`Expecting 'expression' attribute to be a string. ` +
        `But was: ${typeof(request.expression)}`);
    }
    return context.evaluate(request.expression);
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

  callFunctionOn(request) {
    let context = null;
    // When an `objectId` is passed, we want to execute the function of a given object
    // So we first have to find its ExecutionContext
    if (request.objectId) {
      for (const ctx of this.contexts.values()) {
        if (ctx.hasRemoteObject(request.objectId)) {
          context = ctx;
          break;
        }
      }
      if (!context) {
        throw new Error(`Unable to get the context for object with id: ${request.objectId}`);
      }
    } else {
      context = this.contexts.get(request.executionContextId);
      if (!context) {
        throw new Error(`Unable to find execution context with id: ${request.executionContextId}`);
      }
    }
    if (typeof(request.functionDeclaration) != "string") {
      throw new Error("Expect 'functionDeclaration' attribute to be passed and be a string");
    }
    if (request.arguments && !Array.isArray(request.arguments)) {
      throw new Error("Expect 'arguments' to be an array");
    }
    if (request.returnByValue && typeof(request.returnByValue) != "boolean") {
      throw new Error("Expect 'returnByValue' to be a boolean");
    }
    if (request.awaitPromise && typeof(request.awaitPromise) != "boolean") {
      throw new Error("Expect 'awaitPromise' to be a boolean");
    }
    return context.callFunctionOn(request.functionDeclaration, request.arguments, request.returnByValue, request.awaitPromise, request.objectId);
  }

  get _debugger() {
    if (this.__debugger) {
      return this.__debugger;
    }
    this.__debugger = new Debugger();
    return this.__debugger;
  }

  getContextByFrameId(frameId) {
    for (const ctx of this.contexts.values()) {
      if (ctx.frameId === frameId) {
        return ctx;
      }
    }
    return null;
  }

  /**
   * Helper method in order to instantiate the ExecutionContext for a given
   * DOM Window as well as emitting the related `Runtime.executionContextCreated`
   * event.
   *
   * @param {Window} window
   *     The window object of the newly instantiated document.
   */
  onContextCreated(name, { id, window }) {
    if (this.contexts.has(id)) {
      return;
    }

    const context = new ExecutionContext(this._debugger, window);
    this.contexts.set(id, context);

    this.emit("Runtime.executionContextCreated", {
      context: {
        id,
        auxData: {
          isDefault: window == this.content,
          frameId: context.frameId,
        },
      },
    });
  }

  /**
   * Helper method to destroy the ExecutionContext of the given id. Also emit
   * the related `Runtime.executionContextDestroyed` event.
   * ContextObserver will call this method with either `id` or `frameId` argument
   * being set.
   *
   * @param {Number} id
   *     The execution context id to destroy.
   * @param {Number} frameId
   *     The frame id of execution context to destroy.
   * Eiter `id` or `frameId` is passed.
   */
  onContextDestroyed(name, { id, frameId }) {
    let context;
    if (id && frameId) {
      throw new Error("Expects only id *or* frameId argument to be passed");
    }

    if (id) {
      context = this.contexts.get(id);
    } else {
      context = this.getContextByFrameId(frameId);
    }

    if (context) {
      context.destructor();
      this.contexts.delete(context.id);
      this.emit("Runtime.executionContextDestroyed", {
        executionContextId: context.id,
      });
    }
  }
}
