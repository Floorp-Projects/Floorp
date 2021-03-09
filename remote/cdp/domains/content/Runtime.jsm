/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Runtime"];

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  addDebuggerToGlobal: "resource://gre/modules/jsdebugger.jsm",
  Services: "resource://gre/modules/Services.jsm",

  ContentProcessDomain:
    "chrome://remote/content/cdp/domains/ContentProcessDomain.jsm",
  executeSoon: "chrome://remote/content/shared/Sync.jsm",
  ExecutionContext:
    "chrome://remote/content/cdp/domains/content/runtime/ExecutionContext.jsm",
});

// Import the `Debugger` constructor in the current scope
addDebuggerToGlobal(Cu.getGlobalForObject(this));

const OBSERVER_CONSOLE_API = "console-api-log-event";

const CONSOLE_API_LEVEL_MAP = {
  warn: "warning",
};

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
    this.innerWindowIdToContexts = new SetMap();

    this._onContextCreated = this._onContextCreated.bind(this);
    this._onContextDestroyed = this._onContextDestroyed.bind(this);

    // TODO Bug 1602083
    this.session.contextObserver.on("context-created", this._onContextCreated);
    this.session.contextObserver.on(
      "context-destroyed",
      this._onContextDestroyed
    );
  }

  destructor() {
    this.disable();

    this.session.contextObserver.off("context-created", this._onContextCreated);
    this.session.contextObserver.off(
      "context-destroyed",
      this._onContextDestroyed
    );

    super.destructor();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;

      Services.console.registerListener(this);
      Services.obs.addObserver(this, OBSERVER_CONSOLE_API);

      // Spin the event loop in order to send the `executionContextCreated` event right
      // after we replied to `enable` request.
      executeSoon(() => {
        this._onContextCreated("context-created", {
          windowId: this.content.windowGlobalChild.innerWindowId,
          window: this.content,
          isDefault: true,
        });
      });
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;

      Services.console.unregisterListener(this);
      Services.obs.removeObserver(this, OBSERVER_CONSOLE_API);
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
      throw new Error(
        `Unable to get execution context by object ID: ${objectId}`
      );
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
   * @param {boolean=} options.awaitPromise
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
    const {
      expression,
      awaitPromise = false,
      contextId,
      returnByValue = false,
    } = options;

    if (typeof expression != "string") {
      throw new Error("expression: string value expected");
    }
    if (!["undefined", "boolean"].includes(typeof options.awaitPromise)) {
      throw new TypeError("awaitPromise: boolean value expected");
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

    return context.evaluate(expression, awaitPromise, returnByValue);
  }

  getProperties({ objectId, ownProperties }) {
    for (const ctx of this.contexts.values()) {
      const debuggerObj = ctx.getRemoteObject(objectId);
      if (debuggerObj) {
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

  _buildStackTrace(stack) {
    const callFrames = [];

    while (
      stack &&
      stack.source !== "debugger eval code" &&
      !stack.source.startsWith("chrome://")
    ) {
      callFrames.push({
        functionName: stack.functionDisplayName,
        scriptId: stack.sourceId,
        url: stack.source,
        lineNumber: stack.line,
        columnNumber: stack.column,
      });
      stack = stack.parent || stack.asyncParent;
    }

    return {
      callFrames,
    };
  }

  _getRemoteObject(objectId) {
    for (const ctx of this.contexts.values()) {
      const debuggerObj = ctx.getRemoteObject(objectId);
      if (debuggerObj) {
        return debuggerObj;
      }
    }
    return null;
  }

  _serializeRemoteObject(debuggerObj, executionContextId) {
    const ctx = this.contexts.get(executionContextId);
    return ctx._toRemoteObject(debuggerObj);
  }

  _getRemoteObjectByNodeId(nodeId, executionContextId) {
    let debuggerObj = null;

    if (typeof executionContextId != "undefined") {
      const ctx = this.contexts.get(executionContextId);
      debuggerObj = ctx.getRemoteObjectByNodeId(nodeId);
    } else {
      for (const ctx of this.contexts.values()) {
        const obj = ctx.getRemoteObjectByNodeId(nodeId);
        if (obj) {
          debuggerObj = obj;
          break;
        }
      }
    }

    return debuggerObj;
  }

  _setRemoteObject(debuggerObj, context) {
    return context.setRemoteObject(debuggerObj);
  }

  _getDefaultContextForWindow(innerWindowId) {
    if (!innerWindowId) {
      innerWindowId = this.content.windowGlobalChild.innerWindowId;
    }
    const curContexts = this.innerWindowIdToContexts.get(innerWindowId);
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

  _emitConsoleAPICalled(payload) {
    // Filter out messages that aren't coming from a valid inner window, or from
    // a different browser tab. Also messages of type "time", which are not
    // getting reported by Chrome.
    const curBrowserId = this.session.browsingContext.browserId;
    const win = Services.wm.getCurrentInnerWindowWithId(payload.innerWindowId);
    if (
      !win ||
      BrowsingContext.getFromWindow(win).browserId != curBrowserId ||
      payload.type === "time"
    ) {
      return;
    }

    const context = this._getDefaultContextForWindow();
    this.emit("Runtime.consoleAPICalled", {
      args: payload.arguments.map(arg => context._toRemoteObject(arg)),
      executionContextId: context?.id || 0,
      timestamp: payload.timestamp,
      type: payload.type,
      stackTrace: this._buildStackTrace(payload.stack),
    });
  }

  _emitExceptionThrown(payload) {
    // Filter out messages that aren't coming from a valid inner window, or from
    // a different browser tab. Also messages of type "time", which are not
    // getting reported by Chrome.
    const curBrowserId = this.session.browsingContext.browserId;
    const win = Services.wm.getCurrentInnerWindowWithId(payload.innerWindowId);
    if (!win || BrowsingContext.getFromWindow(win).browserId != curBrowserId) {
      return;
    }

    const context = this._getDefaultContextForWindow();
    this.emit("Runtime.exceptionThrown", {
      timestamp: payload.timestamp,
      exceptionDetails: {
        // Temporary placeholder to return a number.
        exceptionId: 0,
        text: payload.text,
        lineNumber: payload.lineNumber,
        columnNumber: payload.columnNumber,
        url: payload.url,
        stackTrace: this._buildStackTrace(payload.stack),
        executionContextId: context?.id || undefined,
      },
    });
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
      isDefault = true,
      contextType = "default",
    } = options;

    if (windowId === undefined) {
      throw new Error("windowId is required");
    }

    // allow only one default context per inner window
    if (isDefault && this.innerWindowIdToContexts.has(windowId)) {
      for (const ctx of this.innerWindowIdToContexts.get(windowId)) {
        if (ctx.isDefault) {
          return null;
        }
      }
    }

    const context = new ExecutionContext(
      this._debugger,
      window,
      this.innerWindowIdToContexts.count,
      isDefault
    );
    this.contexts.set(context.id, context);
    this.innerWindowIdToContexts.set(windowId, context);

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
   * @param {number} frameId
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
      contexts = this.innerWindowIdToContexts.get(windowId) || [];
    }

    for (const ctx of contexts) {
      const isFrame = !!BrowsingContext.get(ctx.frameId).parent;

      ctx.destructor();
      this.contexts.delete(ctx.id);
      this.innerWindowIdToContexts.get(ctx.windowId).delete(ctx);

      if (this.enabled) {
        this.emit("Runtime.executionContextDestroyed", {
          executionContextId: ctx.id,
        });
      }

      if (this.innerWindowIdToContexts.get(ctx.windowId).size == 0) {
        this.innerWindowIdToContexts.delete(ctx.windowId);
        // Only emit when all the exeuction contexts were cleared for the
        // current browser / target, which means it should only be emitted
        // for a top-level browsing context reference.
        if (this.enabled && !isFrame) {
          this.emit("Runtime.executionContextsCleared");
        }
      }
    }
  }

  // nsIObserver

  /**
   * Takes a console message belonging to the current window and emits a
   * "exceptionThrown" event if it's a Javascript error, otherwise a
   * "consoleAPICalled" event.
   *
   * @param {nsIConsoleMessage} message
   *     Console message.
   */
  observe(subject, topic, data) {
    let entry;

    if (topic == OBSERVER_CONSOLE_API) {
      const message = subject.wrappedJSObject;
      entry = fromConsoleAPI(message);
      this._emitConsoleAPICalled(entry);
    } else if (subject instanceof Ci.nsIScriptError && subject.hasException) {
      entry = fromScriptError(subject);
      this._emitExceptionThrown(entry);
    }
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIConsoleListener"]);
  }
}

function fromConsoleAPI(message) {
  // From sendConsoleAPIMessage (toolkit/modules/Console.jsm)
  return {
    arguments: message.arguments,
    innerWindowId: message.innerID,
    // TODO: Fetch the stack (Bug 1679981)
    stack: undefined,
    timestamp: message.timeStamp,
    type: CONSOLE_API_LEVEL_MAP[message.level] || message.level,
  };
}

function fromScriptError(error) {
  // From dom/bindings/nsIScriptError.idl
  return {
    innerWindowId: error.innerWindowID,
    columnNumber: error.columnNumber,
    lineNumber: error.lineNumber,
    stack: error.stack,
    text: error.errorMessage,
    timestamp: error.timeStamp,
    url: error.sourceName,
  };
}
