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
  }

  destructor() {
    this.disable();
  }

  // commands

  async enable() {
    if (!this.enabled) {
      this.enabled = true;
      this.chromeEventHandler.addEventListener("DOMWindowCreated", this,
        {mozSystemGroup: true});

      // Listen for pageshow and pagehide to track pages going in/out to/from the BF Cache
      this.chromeEventHandler.addEventListener("pageshow", this,
        {mozSystemGroup: true});
      this.chromeEventHandler.addEventListener("pagehide", this,
        {mozSystemGroup: true});

      Services.obs.addObserver(this, "inner-window-destroyed");

      // Spin the event loop in order to send the `executionContextCreated` event right
      // after we replied to `enable` request.
      Services.tm.dispatchToMainThread(() => {
        this._createContext(this.content);
      });
    }
  }

  disable() {
    if (this.enabled) {
      this.enabled = false;
      this.chromeEventHandler.removeEventListener("DOMWindowCreated", this,
        {mozSystemGroup: true});
      this.chromeEventHandler.removeEventListener("pageshow", this,
        {mozSystemGroup: true});
      this.chromeEventHandler.removeEventListener("pagehide", this,
        {mozSystemGroup: true});
      Services.obs.removeObserver(this, "inner-window-destroyed");
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

  get _debugger() {
    if (this.__debugger) {
      return this.__debugger;
    }
    this.__debugger = new Debugger();
    return this.__debugger;
  }

  handleEvent({type, target, persisted}) {
    if (target.defaultView != this.content) {
      // Ignore iframes for now.
      return;
    }
    switch (type) {
    case "DOMWindowCreated":
      this._createContext(target.defaultView);
      break;

    case "pageshow":
      // `persisted` is true when this is about a page being resurected from BF Cache
      if (!persisted) {
        return;
      }
      this._createContext(target.defaultView);
      break;

    case "pagehide":
      // `persisted` is true when this is about a page being frozen into BF Cache
      if (!persisted) {
        return;
      }
      const id = target.defaultView.windowUtils.currentInnerWindowID;
      this._destroyContext(id);
      break;
    }
  }

  observe(subject, topic, data) {
    const innerWindowID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    this._destroyContext(innerWindowID);
  }

  /**
   * Helper method in order to instantiate the ExecutionContext for a given
   * DOM Window as well as emitting the related `Runtime.executionContextCreated`
   * event.
   *
   * @param {Window} window
   *     The window object of the newly instantiated document.
   */
  _createContext(window) {
    const { windowUtils } = window;
    const id = windowUtils.currentInnerWindowID;
    if (this.contexts.has(id)) {
      return;
    }

    const context = new ExecutionContext(this._debugger, window);
    this.contexts.set(id, context);

    const frameId = windowUtils.outerWindowID;
    this.emit("Runtime.executionContextCreated", {
      context: {
        id,
        auxData: {
          isDefault: window == this.content,
          frameId,
        },
      },
    });
  }

  /**
   * Helper method to destroy the ExecutionContext of the given id. Also emit
   * the related `Runtime.executionContextDestroyed` event.
   *
   * @param {Number} id
   *     The execution context id to destroy.
   */
  _destroyContext(id) {
    const context = this.contexts.get(id);

    if (context) {
      context.destructor();
      this.contexts.delete(id);
      this.emit("Runtime.executionContextDestroyed", {
        executionContextId: id,
      });
    }
  }
}
