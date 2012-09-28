/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleUtils",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageErrorListener",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPIListener",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSTermHelpers",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSPropertyProvider",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleAPIStorage",
                                  "resource://gre/modules/ConsoleAPIStorage.jsm");


/**
 * The WebConsoleActor implements capabilities needed for the Web Console
 * feature.
 *
 * @constructor
 * @param object aConnection
 *        The connection to the client, DebuggerServerConnection.
 * @param object aTabActor
 *        The parent tab actor.
 */
function WebConsoleActor(aConnection, aTabActor)
{
  this.conn = aConnection;
  this._browser = aTabActor.browser;

  this._objectActorsPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._objectActorsPool);
}

WebConsoleActor.prototype =
{
  /**
   * The xul:browser we work with.
   * @private
   * @type nsIDOMElement
   */
  _browser: null,

  /**
   * Actor pool for all of the object actors for objects we send to the client.
   * @private
   * @type object
   * @see ActorPool
   * @see this.objectGrip()
   */
  _objectActorsPool: null,

  /**
   * Tells the current page location associated to the sandbox. When the page
   * location is changed, we recreate the sandbox.
   * @private
   * @type object
   */
  _sandboxLocation: null,

  /**
   * The JavaScript Sandbox where code is evaluated.
   * @type object
   */
  sandbox: null,

  /**
   * The debugger server connection instance.
   * @type object
   */
  conn: null,

  /**
   * The content window we work with.
   * @type nsIDOMWindow
   */
  get window() this._browser.contentWindow,

  /**
   * The PageErrorListener instance.
   * @type object
   */
  pageErrorListener: null,

  /**
   * The ConsoleAPIListener instance.
   */
  consoleAPIListener: null,

  actorPrefix: "console",

  grip: function WCA_grip()
  {
    return { actor: this.actorID };
  },

  /**
   * Tells if the window.console object is native or overwritten by script in
   * the page.
   *
   * @return boolean
   *         True if the window.console object is native, or false otherwise.
   */
  hasNativeConsoleAPI: function WCA_hasNativeConsoleAPI()
  {
    let isNative = false;
    try {
      let consoleObject = WebConsoleUtils.unwrap(this.window).console;
      isNative = "__mozillaConsole__" in consoleObject;
    }
    catch (ex) { }
    return isNative;
  },

  /**
   * Destroy the current WebConsoleActor instance.
   */
  disconnect: function WCA_disconnect()
  {
    if (this.pageErrorListener) {
      this.pageErrorListener.destroy();
      this.pageErrorListener = null;
    }
    if (this.consoleAPIListener) {
      this.consoleAPIListener.destroy();
      this.consoleAPIListener = null;
    }
    this.conn.removeActorPool(this._objectActorsPool);
    this._objectActorsPool = null;
    this._sandboxLocation = this.sandbox = null;
    this.conn = this._browser = null;
  },

  /**
   * Create a grip for the given value. If the value is an object,
   * a WebConsoleObjectActor will be created.
   *
   * @param mixed aValue
   * @return object
   */
  createValueGrip: function WCA_createValueGrip(aValue)
  {
    return WebConsoleUtils.createValueGrip(aValue,
                                           this.createObjectActor.bind(this));
  },

  /**
   * Create a grip for the given object.
   *
   * @param object aObject
   *        The object you want.
   * @param object
   *        The object grip.
   */
  createObjectActor: function WCA_createObjectActor(aObject)
  {
    // We need to unwrap the object, otherwise we cannot access the properties
    // and methods added by the content scripts.
    let obj = WebConsoleUtils.unwrap(aObject);
    let actor = new WebConsoleObjectActor(obj, this);
    this._objectActorsPool.addActor(actor);
    return actor.grip();
  },

  /**
   * Get an object actor by its ID.
   *
   * @param string aActorID
   * @return object
   */
  getObjectActorByID: function WCA_getObjectActorByID(aActorID)
  {
    return this._objectActorsPool.get(aActorID);
  },

  /**
   * Release an object grip for the given object actor.
   *
   * @param object aActor
   *        The WebConsoleObjectActor instance you want to release.
   */
  releaseObject: function WCA_releaseObject(aActor)
  {
    this._objectActorsPool.removeActor(aActor.actorID);
  },

  /**
   * Handler for the "startListeners" request.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response object which holds the startedListeners array.
   */
  onStartListeners: function WCA_onStartListeners(aRequest)
  {
    let startedListeners = [];

    while (aRequest.listeners.length > 0) {
      let listener = aRequest.listeners.shift();
      switch (listener) {
        case "PageError":
          if (!this.pageErrorListener) {
            this.pageErrorListener =
              new PageErrorListener(this.window, this);
            this.pageErrorListener.init();
          }
          startedListeners.push(listener);
          break;
        case "ConsoleAPI":
          if (!this.consoleAPIListener) {
            this.consoleAPIListener =
              new ConsoleAPIListener(this.window, this);
            this.consoleAPIListener.init();
          }
          startedListeners.push(listener);
          break;
      }
    }
    return {
      startedListeners: startedListeners,
      nativeConsoleAPI: this.hasNativeConsoleAPI(),
    };
  },

  /**
   * Handler for the "stopListeners" request.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response packet to send to the client: holds the
   *         stoppedListeners array.
   */
  onStopListeners: function WCA_onStopListeners(aRequest)
  {
    let stoppedListeners = [];

    // If no specific listeners are requested to be detached, we stop all
    // listeners.
    let toDetach = aRequest.listeners || ["PageError", "ConsoleAPI"];

    while (toDetach.length > 0) {
      let listener = toDetach.shift();
      switch (listener) {
        case "PageError":
          if (this.pageErrorListener) {
            this.pageErrorListener.destroy();
            this.pageErrorListener = null;
          }
          stoppedListeners.push(listener);
          break;
        case "ConsoleAPI":
          if (this.consoleAPIListener) {
            this.consoleAPIListener.destroy();
            this.consoleAPIListener = null;
          }
          stoppedListeners.push(listener);
          break;
      }
    }

    return { stoppedListeners: stoppedListeners };
  },

  /**
   * Handler for the "getCachedMessages" request. This method sends the cached
   * error messages and the window.console API calls to the client.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The response packet to send to the client: it holds the cached
   *         messages array.
   */
  onGetCachedMessages: function WCA_onGetCachedMessages(aRequest)
  {
    let types = aRequest.messageTypes;
    if (!types) {
      return {
        error: "missingParameter",
        message: "The messageTypes parameter is missing.",
      };
    }

    let messages = [];

    while (types.length > 0) {
      let type = types.shift();
      switch (type) {
        case "ConsoleAPI":
          if (this.consoleAPIListener) {
            let cache = this.consoleAPIListener.getCachedMessages();
            cache.forEach(function(aMessage) {
              let message = this.prepareConsoleMessageForRemote(aMessage);
              message._type = type;
              messages.push(message);
            }, this);
          }
          break;
        case "PageError":
          if (this.pageErrorListener) {
            let cache = this.pageErrorListener.getCachedMessages();
            cache.forEach(function(aMessage) {
              let message = this.preparePageErrorForRemote(aMessage);
              message._type = type;
              messages.push(message);
            }, this);
          }
          break;
      }
    }

    messages.sort(function(a, b) { return a.timeStamp - b.timeStamp; });

    return {
      from: this.actorID,
      messages: messages,
    };
  },

  /**
   * Handler for the "evaluateJS" request. This method evaluates the given
   * JavaScript string and sends back the result.
   *
   * @param object aRequest
   *        The JSON request object received from the Web Console client.
   * @return object
   *         The evaluation response packet.
   */
  onEvaluateJS: function WCA_onEvaluateJS(aRequest)
  {
    let input = aRequest.text;
    let result, error = null;
    let timestamp;

    this.helperResult = null;
    this.evalInput = input;
    try {
      timestamp = Date.now();
      result = this.evalInSandbox(input);
    }
    catch (ex) {
      error = ex;
    }

    let helperResult = this.helperResult;
    delete this.helperResult;
    delete this.evalInput;

    return {
      from: this.actorID,
      input: input,
      result: this.createValueGrip(result),
      timestamp: timestamp,
      error: error,
      errorMessage: error ? String(error) : null,
      helperResult: helperResult,
    };
  },

  /**
   * The Autocomplete request handler.
   *
   * @param object aRequest
   *        The request message - what input to autocomplete.
   * @return object
   *         The response message - matched properties.
   */
  onAutocomplete: function WCA_onAutocomplete(aRequest)
  {
    let result = JSPropertyProvider(this.window, aRequest.text) || {};
    return {
      from: this.actorID,
      matches: result.matches || [],
      matchProp: result.matchProp,
    };
  },

  /**
   * The "clearMessagesCache" request handler.
   */
  onClearMessagesCache: function WCA_onClearMessagesCache()
  {
    // TODO: Bug 717611 - Web Console clear button does not clear cached errors
    let windowId = WebConsoleUtils.getInnerWindowId(this.window);
    ConsoleAPIStorage.clearEvents(windowId);
    return {};
  },

  /**
   * Create the JavaScript sandbox where user input is evaluated.
   * @private
   */
  _createSandbox: function WCA__createSandbox()
  {
    this._sandboxLocation = this.window.location;
    this.sandbox = new Cu.Sandbox(this.window, {
      sandboxPrototype: this.window,
      wantXrays: false,
    });

    this.sandbox.console = this.window.console;

    JSTermHelpers(this);
  },

  /**
   * Evaluates a string in the sandbox.
   *
   * @param string aString
   *        String to evaluate in the sandbox.
   * @return mixed
   *         The result of the evaluation.
   */
  evalInSandbox: function WCA_evalInSandbox(aString)
  {
    // If the user changed to a different location, we need to update the
    // sandbox.
    if (this._sandboxLocation !== this.window.location) {
      this._createSandbox();
    }

    // The help function needs to be easy to guess, so we make the () optional
    if (aString.trim() == "help" || aString.trim() == "?") {
      aString = "help()";
    }

    let window = WebConsoleUtils.unwrap(this.sandbox.window);
    let $ = null, $$ = null;

    // We prefer to execute the page-provided implementations for the $() and
    // $$() functions.
    if (typeof window.$ == "function") {
      $ = this.sandbox.$;
      delete this.sandbox.$;
    }
    if (typeof window.$$ == "function") {
      $$ = this.sandbox.$$;
      delete this.sandbox.$$;
    }

    let result = Cu.evalInSandbox(aString, this.sandbox, "1.8",
                                  "Web Console", 1);

    if ($) {
      this.sandbox.$ = $;
    }
    if ($$) {
      this.sandbox.$$ = $$;
    }

    return result;
  },

  /**
   * Handler for page errors received from the PageErrorListener. This method
   * sends the nsIScriptError to the remote Web Console client.
   *
   * @param nsIScriptError aPageError
   *        The page error we need to send to the client.
   */
  onPageError: function WCA_onPageError(aPageError)
  {
    let packet = {
      from: this.actorID,
      type: "pageError",
      pageError: this.preparePageErrorForRemote(aPageError),
    };
    this.conn.send(packet);
  },

  /**
   * Prepare an nsIScriptError to be sent to the client.
   *
   * @param nsIScriptError aPageError
   *        The page error we need to send to the client.
   * @return object
   *         The object you can send to the remote client.
   */
  preparePageErrorForRemote: function WCA_preparePageErrorForRemote(aPageError)
  {
    return {
      message: aPageError.message,
      errorMessage: aPageError.errorMessage,
      sourceName: aPageError.sourceName,
      lineText: aPageError.sourceLine,
      lineNumber: aPageError.lineNumber,
      columnNumber: aPageError.columnNumber,
      category: aPageError.category,
      timeStamp: aPageError.timeStamp,
      warning: !!(aPageError.flags & aPageError.warningFlag),
      error: !!(aPageError.flags & aPageError.errorFlag),
      exception: !!(aPageError.flags & aPageError.exceptionFlag),
      strict: !!(aPageError.flags & aPageError.strictFlag),
    };
  },

  /**
   * Handler for window.console API calls received from the ConsoleAPIListener.
   * This method sends the object to the remote Web Console client.
   *
   * @param object aMessage
   *        The console API call we need to send to the remote client.
   */
  onConsoleAPICall: function WCA_onConsoleAPICall(aMessage)
  {
    let packet = {
      from: this.actorID,
      type: "consoleAPICall",
      message: this.prepareConsoleMessageForRemote(aMessage),
    };
    this.conn.send(packet);
  },

  /**
   * Prepare a message from the console API to be sent to the remote Web Console
   * instance.
   *
   * @param object aMessage
   *        The original message received from console-api-log-event.
   * @return object
   *         The object that can be sent to the remote client.
   */
  prepareConsoleMessageForRemote:
  function WCA_prepareConsoleMessageForRemote(aMessage)
  {
    let result = {
      level: aMessage.level,
      filename: aMessage.filename,
      lineNumber: aMessage.lineNumber,
      functionName: aMessage.functionName,
      timeStamp: aMessage.timeStamp,
    };

    switch (result.level) {
      case "trace":
      case "group":
      case "groupCollapsed":
      case "time":
      case "timeEnd":
        result.arguments = aMessage.arguments;
        break;
      default:
        result.arguments = Array.map(aMessage.arguments || [],
          function(aObj) {
            return this.createValueGrip(aObj);
          }, this);

        if (result.level == "dir") {
          result.objectProperties = [];
          let first = result.arguments[0];
          if (typeof first == "object" && first && first.inspectable) {
            let actor = this.getObjectActorByID(first.actor);
            result.objectProperties = actor.onInspectProperties().properties;
          }
        }
        break;
    }

    return result;
  },

  /**
   * Find the XUL window that owns the content window.
   *
   * @return Window
   *         The XUL window that owns the content window.
   */
  chromeWindow: function WCA_chromeWindow()
  {
    return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
           .getInterface(Ci.nsIWebNavigation).QueryInterface(Ci.nsIDocShell)
           .chromeEventHandler.ownerDocument.defaultView;
  },
};

WebConsoleActor.prototype.requestTypes =
{
  startListeners: WebConsoleActor.prototype.onStartListeners,
  stopListeners: WebConsoleActor.prototype.onStopListeners,
  getCachedMessages: WebConsoleActor.prototype.onGetCachedMessages,
  evaluateJS: WebConsoleActor.prototype.onEvaluateJS,
  autocomplete: WebConsoleActor.prototype.onAutocomplete,
  clearMessagesCache: WebConsoleActor.prototype.onClearMessagesCache,
};

/**
 * Creates an actor for the specified object.
 *
 * @constructor
 * @param object aObj
 *        The object you want.
 * @param object aWebConsoleActor
 *        The parent WebConsoleActor instance for this object.
 */
function WebConsoleObjectActor(aObj, aWebConsoleActor)
{
  this.obj = aObj;
  this.parent = aWebConsoleActor;
}

WebConsoleObjectActor.prototype =
{
  actorPrefix: "consoleObj",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function WCOA_grip()
  {
    let grip = WebConsoleUtils.getObjectGrip(this.obj);
    grip.actor = this.actorID;
    return grip;
  },

  /**
   * Releases this actor from the pool.
   */
  release: function WCOA_release()
  {
    this.parent.releaseObject(this);
    this.parent = this.obj = null;
  },

  /**
   * Handle a protocol request to inspect the properties of the object.
   *
   * @return object
   *         Message to send to the client. This holds the 'properties' property
   *         - an array with a descriptor for each property in the object.
   */
  onInspectProperties: function WCOA_onInspectProperties()
  {
    // TODO: Bug 787981 - use LongStringActor for strings that are too long.
    let createObjectActor = this.parent.createObjectActor.bind(this.parent);
    let props = WebConsoleUtils.inspectObject(this.obj, createObjectActor);
    return {
      from: this.actorID,
      properties: props,
    };
  },

  /**
   * Handle a protocol request to release a grip.
   */
  onRelease: function WCOA_onRelease()
  {
    this.release();
    return {};
  },
};

WebConsoleObjectActor.prototype.requestTypes =
{
  "inspectProperties": WebConsoleObjectActor.prototype.onInspectProperties,
  "release": WebConsoleObjectActor.prototype.onRelease,
};

