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

  this._actorPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._actorPool);

  this._prefs = {};

  this.dbg = new Debugger();
  this._createGlobal();

  this._protoChains = new Map();
}

WebConsoleActor.prototype =
{
  /**
   * Debugger instance.
   *
   * @see jsdebugger.jsm
   */
  dbg: null,

  /**
   * Tells if this Web Console actor is a global actor or not.
   * @private
   * @type boolean
   */
  _isGlobalActor: false,

  /**
   * Actor pool for all of the actors we send to the client.
   * @private
   * @type object
   * @see ActorPool
   */
  _actorPool: null,

  /**
   * Web Console-related preferences.
   * @private
   * @type object
   */
  _prefs: null,

  /**
   * Tells the current inner window of the window of |this._dbgWindow|. When the
   * page is navigated, we recreate the debugger object.
   * @private
   * @type object
   */
  _globalWindowId: 0,

  /**
   * The Debugger.Object that wraps the content window.
   * @private
   * @type object
   */
  _dbgWindow: null,

  /**
   * Object that holds the API we give to the JSTermHelpers constructor. This is
   * where the JSTerm helper functions are added.
   *
   * @see this._getJSTermHelpers()
   * @private
   * @type object
   */
  _jstermHelpers: null,

  /**
   * A cache of prototype chains for objects that have received a
   * prototypeAndProperties request.
   *
   * @private
   * @type Map
   * @see dbg-script-actors.js, ThreadActor._protoChains
   */
  _protoChains: null,

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

  hasNativeConsoleAPI: BrowserTabActor.prototype.hasNativeConsoleAPI,

  _createValueGrip: ThreadActor.prototype.createValueGrip,
  _stringIsLong: ThreadActor.prototype._stringIsLong,
  _findProtoChain: ThreadActor.prototype._findProtoChain,
  _removeFromProtoChain: ThreadActor.prototype._removeFromProtoChain,

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
    this.conn.removeActorPool(this._actorPool);
    this._actorPool = null;
    this._protoChains.clear();
    this.dbg.enabled = false;
    this.dbg = null;
    this._dbgWindow = null;
    this._globalWindowId = 0;
    this.conn = this._window = null;
  },

  /**
   * Create a grip for the given value.
   *
   * @param mixed aValue
   * @return object
   */
  createValueGrip: function WCA_createValueGrip(aValue)
  {
    return this._createValueGrip(aValue, this._actorPool);
  },

  /**
   * Make a debuggee value for the given value.
   *
   * @param mixed aValue
   *        The value you want to get a debuggee value for.
   * @return object
   *         Debuggee value for |aValue|.
   */
  makeDebuggeeValue: function WCA_makeDebuggeeValue(aValue)
  {
    return this._dbgWindow.makeDebuggeeValue(aValue);
  },

  /**
   * Create a grip for the given object.
   *
   * @param object aObject
   *        The object you want.
   * @param object aPool
   *        An ActorPool where the new actor instance is added.
   * @param object
   *        The object grip.
   */
  objectGrip: function WCA_objectGrip(aObject, aPool)
  {
    let actor = new ObjectActor(aObject, this);
    aPool.addActor(actor);
    return actor.grip();
  },

  /**
   * Create a grip for the given string.
   *
   * @param string aString
   *        The string you want to create the grip for.
   * @param object aPool
   *        An ActorPool where the new actor instance is added.
   * @return object
   *         A LongStringActor object that wraps the given string.
   */
  longStringGrip: function WCA_longStringGrip(aString, aPool)
  {
    let actor = new LongStringActor(aString, this);
    aPool.addActor(actor);
    return actor.grip();
  },

  /**
   * Get an object actor by its ID.
   *
   * @param string aActorID
   * @return object
   */
  getActorByID: function WCA_getActorByID(aActorID)
  {
    return this._actorPool.get(aActorID);
  },

  /**
   * Release an actor.
   *
   * @param object aActor
   *        The actor instance you want to release.
   */
  releaseActor: function WCA_releaseActor(aActor)
  {
    this._actorPool.removeActor(aActor.actorID);
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
      }
    }
    return {
      startedListeners: startedListeners,
      nativeConsoleAPI: this.hasNativeConsoleAPI(this.window),
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
                    "FileActivity"];

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
    let timestamp = Date.now();

    let evalOptions = {
      bindObjectActor: aRequest.bindObjectActor,
      frameActor: aRequest.frameActor,
    };
    let evalInfo = this.evalWithDebugger(input, evalOptions);
    let evalResult = evalInfo.result;
    let helperResult = this._jstermHelpers.helperResult;
    delete this._jstermHelpers.helperResult;

    let result, error, errorMessage;
    if (evalResult) {
      if ("return" in evalResult) {
        result = evalResult.return;
      }
      else if ("yield" in evalResult) {
        result = evalResult.yield;
      }
      else if ("throw" in evalResult) {
        error = evalResult.throw;
        let errorToString = evalInfo.window
                            .evalInGlobalWithBindings("ex + ''", {ex: error});
        if (errorToString && typeof errorToString.return == "string") {
          errorMessage = errorToString.return;
        }
      }
    }

    return {
      from: this.actorID,
      input: input,
      result: this.createValueGrip(result),
      timestamp: timestamp,
      exception: error ? this.createValueGrip(error) : null,
      exceptionMessage: errorMessage,
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
    // TODO: Bug 842682 - use the debugger API for autocomplete in the Web
    // Console, and provide suggestions from the selected debugger stack frame.
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
   * Create the Debugger.Object for the current window.
   * @private
   */
  _createGlobal: function WCA__createGlobal()
  {
    let windowId = WebConsoleUtils.getInnerWindowId(this.window);
    if (this._globalWindowId == windowId) {
      return;
    }

    this._globalWindowId = windowId;

    this._dbgWindow = this.dbg.addDebuggee(this.window);
    this.dbg.removeDebuggee(this.window);

    // Update the JSTerm helpers.
    this._jstermHelpers = this._getJSTermHelpers(this._dbgWindow);
  },

  /**
   * Create an object with the API we expose to the JSTermHelpers constructor.
   * This object inherits properties and methods from the Web Console actor.
   *
   * @private
   * @param object aDebuggerObject
   *        A Debugger.Object that wraps a content global. This is used for the
   *        JSTerm helpers.
   * @return object
   */
  _getJSTermHelpers: function WCA__getJSTermHelpers(aDebuggerObject)
  {
    let helpers = Object.create(this);
    helpers.sandbox = Object.create(null);
    helpers._dbgWindow = aDebuggerObject;
    JSTermHelpers(helpers);

    // Make sure the helpers can be used during eval.
    for (let name in helpers.sandbox) {
      let desc = Object.getOwnPropertyDescriptor(helpers.sandbox, name);
      if (desc.get || desc.set) {
        continue;
      }
      helpers.sandbox[name] = helpers.makeDebuggeeValue(desc.value);
    }
    return helpers;
  },

  /**
   * Evaluates a string using the debugger API.
   *
   * To allow the variables view to update properties from the web console we
   * provide the "bindObjectActor" mechanism: the Web Console tells the
   * ObjectActor ID for which it desires to evaluate an expression. The
   * Debugger.Object pointed at by the actor ID is bound such that it is
   * available during expression evaluation (evalInGlobalWithBindings()).
   *
   * Example:
   *   _self['foobar'] = 'test'
   * where |_self| refers to the desired object.
   *
   * The |frameActor| property allows the Web Console client to provide the
   * frame actor ID, such that the expression can be evaluated in the
   * user-selected stack frame.
   *
   * For the above to work we need the debugger and the web console to share
   * a connection, otherwise the Web Console actor will not find the frame
   * actor.
   *
   * The Debugger.Frame comes from the jsdebugger's Debugger instance, which
   * is different from the Web Console's Debugger instance. This means that
   * for evaluation to work, we need to create a new instance for  the jsterm
   * helpers - they need to be Debugger.Objects coming from the jsdebugger's
   * Debugger instance.
   *
   * @param string aString
   *        String to evaluate.
   * @param object [aOptions]
   *        Options for evaluation:
   *        - bindObjectActor: the ObjectActor ID to use for evaluation.
   *          |evalWithBindings()| will be called with one additional binding:
   *          |_self| which will point to the Debugger.Object of the given
   *          ObjectActor.
   *        - frameActor: the FrameActor ID to use for evaluation. The given
   *        debugger frame is used for evaluation, instead of the global window.
   * @return object
   *         An object that holds the following properties:
   *         - dbg: the debugger where the string was evaluated.
   *         - frame: (optional) the frame where the string was evaluated.
   *         - window: the Debugger.Object for the global where the string was
   *         evaluated.
   *         - result: the result of the evaluation.
   */
  evalWithDebugger: function WCA_evalWithDebugger(aString, aOptions = {})
  {
    this._createGlobal();

    // The help function needs to be easy to guess, so we make the () optional.
    if (aString.trim() == "help" || aString.trim() == "?") {
      aString = "help()";
    }

    let bindSelf = null;

    if (aOptions.bindObjectActor) {
      let objActor = this.getActorByID(aOptions.bindObjectActor);
      if (objActor) {
        bindSelf = objActor.obj;
      }
    }

    let helpers = this._jstermHelpers;
    let found$ = false, found$$ = false;
    let frame = null, frameActor = null;
    if (aOptions.frameActor) {
      frameActor = this.conn.getActor(aOptions.frameActor);
      if (frameActor) {
        frame = frameActor.frame;
      }
      else {
        Cu.reportError("Web Console Actor: the frame actor was not found: " +
                       aOptions.frameActor);
      }
    }

    let dbg = this.dbg;
    let dbgWindow = this._dbgWindow;

    if (frame) {
      // Avoid having bindings from a different Debugger. The Debugger.Frame
      // comes from the jsdebugger's Debugger instance.
      dbg = frameActor.threadActor.dbg;
      dbgWindow = dbg.addDebuggee(this.window);
      helpers = this._getJSTermHelpers(dbgWindow);

      let env = frame.environment;
      if (env) {
        found$ = !!env.find("$");
        found$$ = !!env.find("$$");
      }
    }
    else {
      found$ = !!this._dbgWindow.getOwnPropertyDescriptor("$");
      found$$ = !!this._dbgWindow.getOwnPropertyDescriptor("$$");
    }

    let bindings = helpers.sandbox;
    if (bindSelf) {
      let jsObj = bindSelf.unsafeDereference();
      bindings._self = helpers.makeDebuggeeValue(jsObj);
    }

    let $ = null, $$ = null;
    if (found$) {
      $ = bindings.$;
      delete bindings.$;
    }
    if (found$$) {
      $$ = bindings.$$;
      delete bindings.$$;
    }

    helpers.helperResult = null;
    helpers.evalInput = aString;

    let result;
    if (frame) {
      result = frame.evalWithBindings(aString, bindings);
    }
    else {
      result = this._dbgWindow.evalInGlobalWithBindings(aString, bindings);
    }

    delete helpers.evalInput;
    if (helpers != this._jstermHelpers) {
      this._jstermHelpers.helperResult = helpers.helperResult;
      delete helpers.helperResult;
    }

    if ($) {
      bindings.$ = $;
    }
    if ($$) {
      bindings.$$ = $$;
    }

    if (bindings._self) {
      delete bindings._self;
    }

    return {
      result: result,
      dbg: dbg,
      frame: frame,
      window: dbgWindow,
    };
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
    this._actorPool.addActor(actor);

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
    let result = WebConsoleUtils.cloneObject(aMessage);
    delete result.wrappedJSObject;

    result.arguments = Array.map(aMessage.arguments || [],
      function(aObj) {
        let dbgObj = this.makeDebuggeeValue(aObj);
        return this.createValueGrip(dbgObj);
      }, this);

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
    let window = null;
    try {
      window = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
             .getInterface(Ci.nsIWebNavigation).QueryInterface(Ci.nsIDocShell)
             .chromeEventHandler.ownerDocument.defaultView;
    }
    catch (ex) {
      // The above can fail because chromeEventHandler is not available for all
      // kinds of |this.window|.
    }

    return window;
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
  this._longStringActors = new Set();

  this._discardRequestBody = aNetworkEvent.discardRequestBody;
  this._discardResponseBody = aNetworkEvent.discardResponseBody;
}

NetworkEventActor.prototype =
{
  _request: null,
  _response: null,
  _timings: null,
  _longStringActors: null,

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
    for (let grip of this._longStringActors) {
      let actor = this.parent.getActorByID(grip.actor);
      if (actor) {
        this.parent.releaseActor(actor);
      }
    }
    this._longStringActors = new Set();
    this.parent.releaseActor(this);
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
    this._prepareHeaders(aHeaders);

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
    this._prepareHeaders(aCookies);

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
    aPostData.text = this._createStringGrip(aPostData.text);
    if (typeof aPostData.text == "object") {
      this._longStringActors.add(aPostData.text);
    }

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
    this._prepareHeaders(aHeaders);

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
    this._prepareHeaders(aCookies);

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
    aContent.text = this._createStringGrip(aContent.text);
    if (typeof aContent.text == "object") {
      this._longStringActors.add(aContent.text);
    }

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

  /**
   * Prepare the headers array to be sent to the client by using the
   * LongStringActor for the header values, when needed.
   *
   * @private
   * @param array aHeaders
   */
  _prepareHeaders: function NEA__prepareHeaders(aHeaders)
  {
    for (let header of aHeaders) {
      header.value = this._createStringGrip(header.value);
      if (typeof header.value == "object") {
        this._longStringActors.add(header.value);
      }
    }
  },

  /**
   * Create a long string grip if needed for the given string.
   *
   * @private
   * @param string aString
   *        The string you want to create a long string grip for.
   * @return string|object
   *         A string is returned if |aString| is not a long string.
   *         A LongStringActor grip is returned if |aString| is a long string.
   */
  _createStringGrip: function NEA__createStringGrip(aString)
  {
    if (this.parent._stringIsLong(aString)) {
      return this.parent.longStringGrip(aString, this.parent._actorPool);
    }
    return aString;
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

