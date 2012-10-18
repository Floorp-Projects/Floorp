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

XPCOMUtils.defineLazyModuleGetter(this, "ConsoleProgressListener",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSTermHelpers",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "JSPropertyProvider",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetworkMonitor",
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
 * @param object [aParentActor]
 *        Optional, the parent actor.
 */
function WebConsoleActor(aConnection, aParentActor)
{
  this.conn = aConnection;

  if (aParentActor instanceof BrowserTabActor &&
      aParentActor.browser instanceof Ci.nsIDOMWindow) {
    this._window = aParentActor.browser;
  }
  else if (aParentActor instanceof BrowserTabActor &&
           aParentActor.browser instanceof Ci.nsIDOMElement) {
    this._window = aParentActor.browser.contentWindow;
  }
  else {
    this._window = Services.wm.getMostRecentWindow("navigator:browser");
    this._isGlobalActor = true;
  }

  this._objectActorsPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._objectActorsPool);

  this._networkEventActorsPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._networkEventActorsPool);

  this._prefs = {};
}

WebConsoleActor.prototype =
{
  /**
   * Tells if this Web Console actor is a global actor or not.
   * @private
   * @type boolean
   */
  _isGlobalActor: false,

  /**
   * Actor pool for all of the object actors for objects we send to the client.
   * @private
   * @type object
   * @see ActorPool
   * @see WebConsoleObjectActor
   * @see this.objectGrip()
   */
  _objectActorsPool: null,

  /**
   * Actor pool for all of the network event actors.
   * @private
   * @type object
   * @see NetworkEventActor
   */
  _networkEventActorsPool: null,

  /**
   * Web Console-related preferences.
   * @private
   * @type object
   */
  _prefs: null,

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
  get window() this._window,

  _window: null,

  /**
   * The PageErrorListener instance.
   * @type object
   */
  pageErrorListener: null,

  /**
   * The ConsoleAPIListener instance.
   */
  consoleAPIListener: null,

  /**
   * The NetworkMonitor instance.
   */
  networkMonitor: null,

  /**
   * The ConsoleProgressListener instance.
   */
  consoleProgressListener: null,

  /**
   * Getter for the NetworkMonitor.saveRequestAndResponseBodies preference.
   * @type boolean
   */
  get saveRequestAndResponseBodies()
    this._prefs["NetworkMonitor.saveRequestAndResponseBodies"],

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
    if (this.networkMonitor) {
      this.networkMonitor.destroy();
      this.networkMonitor = null;
    }
    if (this.consoleProgressListener) {
      this.consoleProgressListener.destroy();
      this.consoleProgressListener = null;
    }
    this.conn.removeActorPool(this._objectActorsPool);
    this.conn.removeActorPool(this._networkEventActorsPool);
    this._objectActorsPool = null;
    this._networkEventActorsPool = null;
    this._sandboxLocation = this.sandbox = null;
    this.conn = this._window = null;
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
   * Release a network event actor.
   *
   * @param object aActor
   *        The NetworkEventActor instance you want to release.
   */
  releaseNetworkEvent: function WCA_releaseNetworkEvent(aActor)
  {
    this._networkEventActorsPool.removeActor(aActor.actorID);
  },

  //////////////////
  // Request handlers for known packet types.
  //////////////////

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
    let window = !this._isGlobalActor ? this.window : null;

    while (aRequest.listeners.length > 0) {
      let listener = aRequest.listeners.shift();
      switch (listener) {
        case "PageError":
          if (!this.pageErrorListener) {
            this.pageErrorListener =
              new PageErrorListener(window, this);
            this.pageErrorListener.init();
          }
          startedListeners.push(listener);
          break;
        case "ConsoleAPI":
          if (!this.consoleAPIListener) {
            this.consoleAPIListener =
              new ConsoleAPIListener(window, this);
            this.consoleAPIListener.init();
          }
          startedListeners.push(listener);
          break;
        case "NetworkActivity":
          if (!this.networkMonitor) {
            this.networkMonitor =
              new NetworkMonitor(window, this);
            this.networkMonitor.init();
          }
          startedListeners.push(listener);
          break;
        case "FileActivity":
          if (!this.consoleProgressListener) {
            this.consoleProgressListener =
              new ConsoleProgressListener(this.window, this);
          }
          this.consoleProgressListener.startMonitor(this.consoleProgressListener.
                                                    MONITOR_FILE_ACTIVITY);
          startedListeners.push(listener);
          break;
        case "LocationChange":
          if (!this.consoleProgressListener) {
            this.consoleProgressListener =
              new ConsoleProgressListener(this.window, this);
          }
          this.consoleProgressListener.startMonitor(this.consoleProgressListener.
                                                    MONITOR_LOCATION_CHANGE);
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
    let toDetach = aRequest.listeners ||
                   ["PageError", "ConsoleAPI", "NetworkActivity",
                    "FileActivity", "LocationChange"];

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
        case "NetworkActivity":
          if (this.networkMonitor) {
            this.networkMonitor.destroy();
            this.networkMonitor = null;
          }
          stoppedListeners.push(listener);
          break;
        case "FileActivity":
          if (this.consoleProgressListener) {
            this.consoleProgressListener.stopMonitor(this.consoleProgressListener.
                                                     MONITOR_FILE_ACTIVITY);
          }
          stoppedListeners.push(listener);
          break;
        case "LocationChange":
          if (this.consoleProgressListener) {
            this.consoleProgressListener.stopMonitor(this.consoleProgressListener.
                                                     MONITOR_LOCATION_CHANGE);
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
    let windowId = !this._isGlobalActor ?
                   WebConsoleUtils.getInnerWindowId(this.window) : null;
    ConsoleAPIStorage.clearEvents(windowId);
    return {};
  },

  /**
   * The "setPreferences" request handler.
   *
   * @param object aRequest
   *        The request message - which preferences need to be updated.
   */
  onSetPreferences: function WCA_onSetPreferences(aRequest)
  {
    for (let key in aRequest.preferences) {
      this._prefs[key] = aRequest.preferences[key];
    }
    return { updated: Object.keys(aRequest.preferences) };
  },

  //////////////////
  // End of request handlers.
  //////////////////

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

  //////////////////
  // Event handlers for various listeners.
  //////////////////

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
   * @see ConsoleAPIListener
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
   * Handler for network events. This method is invoked when a new network event
   * is about to be recorded.
   *
   * @see NetworkEventActor
   * @see NetworkMonitor from WebConsoleUtils.jsm
   *
   * @param object aEvent
   *        The initial network request event information.
   * @return object
   *         A new NetworkEventActor is returned. This is used for tracking the
   *         network request and response.
   */
  onNetworkEvent: function WCA_onNetworkEvent(aEvent)
  {
    let actor = new NetworkEventActor(aEvent, this);
    this._networkEventActorsPool.addActor(actor);

    let packet = {
      from: this.actorID,
      type: "networkEvent",
      eventActor: actor.grip(),
    };

    this.conn.send(packet);

    return actor;
  },

  /**
   * Handler for file activity. This method sends the file request information
   * to the remote Web Console client.
   *
   * @see ConsoleProgressListener
   * @param string aFileURI
   *        The requested file URI.
   */
  onFileActivity: function WCA_onFileActivity(aFileURI)
  {
    let packet = {
      from: this.actorID,
      type: "fileActivity",
      uri: aFileURI,
    };
    this.conn.send(packet);
  },

  /**
   * Handler for location changes. This method sends the new browser location
   * to the remote Web Console client.
   *
   * @see ConsoleProgressListener
   * @param string aState
   *        Tells the location change state:
   *        - "start" means a load has begun.
   *        - "stop" means load completed.
   * @param string aURI
   *        The new browser URI.
   * @param string aTitle
   *        The new page title URI.
   */
  onLocationChange: function WCA_onLocationChange(aState, aURI, aTitle)
  {
    // TODO: Bug 792062 - Make the tabNavigated notification reusable by the Web Console
    let packet = {
      from: this.actorID,
      type: "locationChange",
      uri: aURI,
      title: aTitle,
      state: aState,
      nativeConsoleAPI: this.hasNativeConsoleAPI(),
    };
    this.conn.send(packet);
  },

  //////////////////
  // End of event handlers for various listeners.
  //////////////////

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
  setPreferences: WebConsoleActor.prototype.onSetPreferences,
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


/**
 * Creates an actor for a network event.
 *
 * @constructor
 * @param object aNetworkEvent
 *        The network event you want to use the actor for.
 * @param object aWebConsoleActor
 *        The parent WebConsoleActor instance for this object.
 */
function NetworkEventActor(aNetworkEvent, aWebConsoleActor)
{
  this.parent = aWebConsoleActor;
  this.conn = this.parent.conn;

  this._startedDateTime = aNetworkEvent.startedDateTime;

  this._request = {
    method: aNetworkEvent.method,
    url: aNetworkEvent.url,
    httpVersion: aNetworkEvent.httpVersion,
    headers: [],
    cookies: [],
    headersSize: aNetworkEvent.headersSize,
    postData: {},
  };

  this._response = {
    headers: [],
    cookies: [],
    content: {},
  };

  this._timings = {};

  this._discardRequestBody = aNetworkEvent.discardRequestBody;
  this._discardResponseBody = aNetworkEvent.discardResponseBody;
}

NetworkEventActor.prototype =
{
  _request: null,
  _response: null,
  _timings: null,

  actorPrefix: "netEvent",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function NEA_grip()
  {
    return {
      actor: this.actorID,
      startedDateTime: this._startedDateTime,
      url: this._request.url,
      method: this._request.method,
    };
  },

  /**
   * Releases this actor from the pool.
   */
  release: function NEA_release()
  {
    this.parent.releaseNetworkEvent(this);
  },

  /**
   * Handle a protocol request to release a grip.
   */
  onRelease: function NEA_onRelease()
  {
    this.release();
    return {};
  },

  /**
   * The "getRequestHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network request headers.
   */
  onGetRequestHeaders: function NEA_onGetRequestHeaders()
  {
    return {
      from: this.actorID,
      headers: this._request.headers,
      headersSize: this._request.headersSize,
    };
  },

  /**
   * The "getRequestCookies" packet type handler.
   *
   * @return object
   *         The response packet - network request cookies.
   */
  onGetRequestCookies: function NEA_onGetRequestCookies()
  {
    return {
      from: this.actorID,
      cookies: this._request.cookies,
    };
  },

  /**
   * The "getRequestPostData" packet type handler.
   *
   * @return object
   *         The response packet - network POST data.
   */
  onGetRequestPostData: function NEA_onGetRequestPostData()
  {
    return {
      from: this.actorID,
      postData: this._request.postData,
      postDataDiscarded: this._discardRequestBody,
    };
  },

  /**
   * The "getResponseHeaders" packet type handler.
   *
   * @return object
   *         The response packet - network response headers.
   */
  onGetResponseHeaders: function NEA_onGetResponseHeaders()
  {
    return {
      from: this.actorID,
      headers: this._response.headers,
      headersSize: this._response.headersSize,
    };
  },

  /**
   * The "getResponseCookies" packet type handler.
   *
   * @return object
   *         The response packet - network response cookies.
   */
  onGetResponseCookies: function NEA_onGetResponseCookies()
  {
    return {
      from: this.actorID,
      cookies: this._response.cookies,
    };
  },

  /**
   * The "getResponseContent" packet type handler.
   *
   * @return object
   *         The response packet - network response content.
   */
  onGetResponseContent: function NEA_onGetResponseContent()
  {
    return {
      from: this.actorID,
      content: this._response.content,
      contentDiscarded: this._discardResponseBody,
    };
  },

  /**
   * The "getEventTimings" packet type handler.
   *
   * @return object
   *         The response packet - network event timings.
   */
  onGetEventTimings: function NEA_onGetEventTimings()
  {
    return {
      from: this.actorID,
      timings: this._timings,
      totalTime: this._totalTime,
    };
  },

  /******************************************************************
   * Listeners for new network event data coming from NetworkMonitor.
   ******************************************************************/

  /**
   * Add network request headers.
   *
   * @param array aHeaders
   *        The request headers array.
   */
  addRequestHeaders: function NEA_addRequestHeaders(aHeaders)
  {
    this._request.headers = aHeaders;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestHeaders",
      headers: aHeaders.length,
      headersSize: this._request.headersSize,
    };

    this.conn.send(packet);
  },

  /**
   * Add network request cookies.
   *
   * @param array aCookies
   *        The request cookies array.
   */
  addRequestCookies: function NEA_addRequestCookies(aCookies)
  {
    this._request.cookies = aCookies;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestCookies",
      cookies: aCookies.length,
    };

    this.conn.send(packet);
  },

  /**
   * Add network request POST data.
   *
   * @param object aPostData
   *        The request POST data.
   */
  addRequestPostData: function NEA_addRequestPostData(aPostData)
  {
    this._request.postData = aPostData;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "requestPostData",
      dataSize: aPostData.text.length,
      discardRequestBody: this._discardRequestBody,
    };

    this.conn.send(packet);
  },

  /**
   * Add the initial network response information.
   *
   * @param object aInfo
   *        The response information.
   */
  addResponseStart: function NEA_addResponseStart(aInfo)
  {
    this._response.httpVersion = aInfo.httpVersion;
    this._response.status = aInfo.status;
    this._response.statusText = aInfo.statusText;
    this._response.headersSize = aInfo.headersSize;
    this._discardResponseBody = aInfo.discardResponseBody;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseStart",
      response: aInfo,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response headers.
   *
   * @param array aHeaders
   *        The response headers array.
   */
  addResponseHeaders: function NEA_addResponseHeaders(aHeaders)
  {
    this._response.headers = aHeaders;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseHeaders",
      headers: aHeaders.length,
      headersSize: this._response.headersSize,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response cookies.
   *
   * @param array aCookies
   *        The response cookies array.
   */
  addResponseCookies: function NEA_addResponseCookies(aCookies)
  {
    this._response.cookies = aCookies;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseCookies",
      cookies: aCookies.length,
    };

    this.conn.send(packet);
  },

  /**
   * Add network response content.
   *
   * @param object aContent
   *        The response content.
   * @param boolean aDiscardedResponseBody
   *        Tells if the response content was recorded or not.
   */
  addResponseContent:
  function NEA_addResponseContent(aContent, aDiscardedResponseBody)
  {
    this._response.content = aContent;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "responseContent",
      mimeType: aContent.mimeType,
      contentSize: aContent.text.length,
      discardResponseBody: aDiscardedResponseBody,
    };

    this.conn.send(packet);
  },

  /**
   * Add network event timing information.
   *
   * @param number aTotal
   *        The total time of the network event.
   * @param object aTimings
   *        Timing details about the network event.
   */
  addEventTimings: function NEA_addEventTimings(aTotal, aTimings)
  {
    this._totalTime = aTotal;
    this._timings = aTimings;

    let packet = {
      from: this.actorID,
      type: "networkEventUpdate",
      updateType: "eventTimings",
      totalTime: aTotal,
    };

    this.conn.send(packet);
  },
};

NetworkEventActor.prototype.requestTypes =
{
  "release": NetworkEventActor.prototype.onRelease,
  "getRequestHeaders": NetworkEventActor.prototype.onGetRequestHeaders,
  "getRequestCookies": NetworkEventActor.prototype.onGetRequestCookies,
  "getRequestPostData": NetworkEventActor.prototype.onGetRequestPostData,
  "getResponseHeaders": NetworkEventActor.prototype.onGetResponseHeaders,
  "getResponseCookies": NetworkEventActor.prototype.onGetResponseCookies,
  "getResponseContent": NetworkEventActor.prototype.onGetResponseContent,
  "getEventTimings": NetworkEventActor.prototype.onGetEventTimings,
};

