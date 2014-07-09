/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Toolkit glue for the remote debugging protocol, loaded into the
 * debugging global.
 */
let { Ci, Cc, CC, Cu, Cr } = require("chrome");
let Services = require("Services");
let { ActorPool } = require("devtools/server/actors/common");
let { DebuggerTransport, LocalDebuggerTransport, ChildDebuggerTransport } =
  require("devtools/toolkit/transport/transport");
let DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
let { dumpn, dumpv, dbg_assert } = DevToolsUtils;
let Services = require("Services");
let EventEmitter = require("devtools/toolkit/event-emitter");
let Debugger = require("Debugger");

// Until all Debugger server code is converted to SDK modules,
// imports Components.* alias from chrome module.
var { Ci, Cc, CC, Cu, Cr } = require("chrome");
// On B2G, `this` != Global scope, so `Ci` won't be binded on `this`
// (i.e. this.Ci is undefined) Then later, when using loadSubScript,
// Ci,... won't be defined for sub scripts.
this.Ci = Ci;
this.Cc = Cc;
this.CC = CC;
this.Cu = Cu;
this.Cr = Cr;
this.Debugger = Debugger;
this.Services = Services;
this.ActorPool = ActorPool;
this.DevToolsUtils = DevToolsUtils;
this.dumpn = dumpn;
this.dumpv = dumpv;
this.dbg_assert = dbg_assert;

// Overload `Components` to prevent SDK loader exception on Components
// object usage
Object.defineProperty(this, "Components", {
  get: function () require("chrome").components
});

const DBG_STRINGS_URI = "chrome://global/locale/devtools/debugger.properties";

DevToolsUtils.defineLazyGetter(this, "nsFile", () => {
  return CC("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
});

if (isWorker) {
  dumpn.wantLogging = true;
  dumpv.wantVerbose = true;
} else {
  const LOG_PREF = "devtools.debugger.log";
  const VERBOSE_PREF = "devtools.debugger.log.verbose";

  dumpn.wantLogging = Services.prefs.getBoolPref(LOG_PREF);
  dumpv.wantVerbose =
    Services.prefs.getPrefType(VERBOSE_PREF) !== Services.prefs.PREF_INVALID &&
    Services.prefs.getBoolPref(VERBOSE_PREF);
}

function loadSubScript(aURL)
{
  try {
    let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
      .getService(Ci.mozIJSSubScriptLoader);
    loader.loadSubScript(aURL, this);
  } catch(e) {
    let errorStr = "Error loading: " + aURL + ":\n" +
                   (e.fileName ? "at " + e.fileName + " : " + e.lineNumber + "\n" : "") +
                   e + " - " + e.stack + "\n";
    dump(errorStr);
    reportError(errorStr);
    throw e;
  }
}

let events = require("sdk/event/core");
let {defer, resolve, reject, all} = require("devtools/toolkit/deprecated-sync-thenables");
this.defer = defer;
this.resolve = resolve;
this.reject = reject;
this.all = all;

// XPCOM constructors
DevToolsUtils.defineLazyGetter(this, "ServerSocket", () => {
  return CC("@mozilla.org/network/server-socket;1",
            "nsIServerSocket",
            "initSpecialConnection");
});

DevToolsUtils.defineLazyGetter(this, "UnixDomainServerSocket", () => {
  return CC("@mozilla.org/network/server-socket;1",
            "nsIServerSocket",
            "initWithFilename");
});

var gRegisteredModules = Object.create(null);

/**
 * The ModuleAPI object is passed to modules loaded using the
 * DebuggerServer.registerModule() API.  Modules can use this
 * object to register actor factories.
 * Factories registered through the module API will be removed
 * when the module is unregistered or when the server is
 * destroyed.
 */
function ModuleAPI() {
  let activeTabActors = new Set();
  let activeGlobalActors = new Set();

  return {
    // See DebuggerServer.setRootActor for a description.
    setRootActor: function(factory) {
      DebuggerServer.setRootActor(factory);
    },

    // See DebuggerServer.addGlobalActor for a description.
    addGlobalActor: function(factory, name) {
      DebuggerServer.addGlobalActor(factory, name);
      activeGlobalActors.add(factory);
    },
    // See DebuggerServer.removeGlobalActor for a description.
    removeGlobalActor: function(factory) {
      DebuggerServer.removeGlobalActor(factory);
      activeGlobalActors.delete(factory);
    },

    // See DebuggerServer.addTabActor for a description.
    addTabActor: function(factory, name) {
      DebuggerServer.addTabActor(factory, name);
      activeTabActors.add(factory);
    },
    // See DebuggerServer.removeTabActor for a description.
    removeTabActor: function(factory) {
      DebuggerServer.removeTabActor(factory);
      activeTabActors.delete(factory);
    },

    // Destroy the module API object, unregistering any
    // factories registered by the module.
    destroy: function() {
      for (let factory of activeTabActors) {
        DebuggerServer.removeTabActor(factory);
      }
      activeTabActors = null;
      for (let factory of activeGlobalActors) {
        DebuggerServer.removeGlobalActor(factory);
      }
      activeGlobalActors = null;
    }
  }
};

/***
 * Public API
 */
var DebuggerServer = {
  _listener: null,
  _initialized: false,
  _transportInitialized: false,
  // Number of currently open TCP connections.
  _socketConnections: 0,
  // Map of global actor names to actor constructors provided by extensions.
  globalActorFactories: {},
  // Map of tab actor names to actor constructors provided by extensions.
  tabActorFactories: {},

  LONG_STRING_LENGTH: 10000,
  LONG_STRING_INITIAL_LENGTH: 1000,
  LONG_STRING_READ_LENGTH: 65 * 1024,

  /**
   * A handler function that prompts the user to accept or decline the incoming
   * connection.
   */
  _allowConnection: null,

  /**
   * The windowtype of the chrome window to use for actors that use the global
   * window (i.e the global style editor). Set this to your main window type,
   * for example "navigator:browser".
   */
  chromeWindowType: null,

  /**
   * Prompt the user to accept or decline the incoming connection. This is the
   * default implementation that products embedding the debugger server may
   * choose to override.
   *
   * @return true if the connection should be permitted, false otherwise
   */
  _defaultAllowConnection: function DS__defaultAllowConnection() {
    let bundle = Services.strings.createBundle(DBG_STRINGS_URI)
    let title = bundle.GetStringFromName("remoteIncomingPromptTitle");
    let msg = bundle.GetStringFromName("remoteIncomingPromptMessage");
    let disableButton = bundle.GetStringFromName("remoteIncomingPromptDisable");
    let prompt = Services.prompt;
    let flags = prompt.BUTTON_POS_0 * prompt.BUTTON_TITLE_OK +
                prompt.BUTTON_POS_1 * prompt.BUTTON_TITLE_CANCEL +
                prompt.BUTTON_POS_2 * prompt.BUTTON_TITLE_IS_STRING +
                prompt.BUTTON_POS_1_DEFAULT;
    let result = prompt.confirmEx(null, title, msg, flags, null, null,
                                  disableButton, null, { value: false });
    if (result == 0) {
      return true;
    }
    if (result == 2) {
      DebuggerServer.closeListener(true);
      Services.prefs.setBoolPref("devtools.debugger.remote-enabled", false);
    }
    return false;
  },

  /**
   * Initialize the debugger server.
   *
   * @param function aAllowConnectionCallback
   *        The embedder-provider callback, that decides whether an incoming
   *        remote protocol conection should be allowed or refused.
   */
  init: function DS_init(aAllowConnectionCallback) {
    if (this.initialized) {
      return;
    }

    this.initTransport(aAllowConnectionCallback);

    this._initialized = true;
  },

  protocol: require("devtools/server/protocol"),

  /**
   * Initialize the debugger server's transport variables.  This can be
   * in place of init() for cases where the jsdebugger isn't needed.
   *
   * @param function aAllowConnectionCallback
   *        The embedder-provider callback, that decides whether an incoming
   *        remote protocol conection should be allowed or refused.
   */
  initTransport: function DS_initTransport(aAllowConnectionCallback) {
    if (this._transportInitialized) {
      return;
    }

    this._connections = {};
    this._nextConnID = 0;
    this._transportInitialized = true;
    this._allowConnection = aAllowConnectionCallback ?
                            aAllowConnectionCallback :
                            this._defaultAllowConnection;
  },

  get initialized() this._initialized,

  /**
   * Performs cleanup tasks before shutting down the debugger server. Such tasks
   * include clearing any actor constructors added at runtime. This method
   * should be called whenever a debugger server is no longer useful, to avoid
   * memory leaks. After this method returns, the debugger server must be
   * initialized again before use.
   */
  destroy: function DS_destroy() {
    if (!this._initialized) {
      return;
    }

    for (let connID of Object.getOwnPropertyNames(this._connections)) {
      this._connections[connID].close();
    }

    for (let id of Object.getOwnPropertyNames(gRegisteredModules)) {
      let mod = gRegisteredModules[id];
      mod.module.unregister(mod.api);
    }
    gRegisteredModules = {};

    this.closeListener();
    this.globalActorFactories = {};
    this.tabActorFactories = {};
    this._allowConnection = null;
    this._transportInitialized = false;
    this._initialized = false;

    dumpn("Debugger server is shut down.");
  },

  /**
   * Load a subscript into the debugging global.
   *
   * @param aURL string A url that will be loaded as a subscript into the
   *        debugging global.  The user must load at least one script
   *        that implements a createRootActor() function to create the
   *        server's root actor.
   */
  addActors: function DS_addActors(aURL) {
    loadSubScript.call(this, aURL);
  },

  /**
   * Register a CommonJS module with the debugger server.
   * @param id string
   *    The ID of a CommonJS module.  This module must export
   *    'register' and 'unregister' functions.
   */
  registerModule: function(id) {
    if (id in gRegisteredModules) {
      throw new Error("Tried to register a module twice: " + id + "\n");
    }

    let moduleAPI = ModuleAPI();
    let mod = require(id);
    mod.register(moduleAPI);
    gRegisteredModules[id] = { module: mod, api: moduleAPI };
  },

  /**
   * Returns true if a module id has been registered.
   */
  isModuleRegistered: function(id) {
    return (id in gRegisteredModules);
  },

  /**
   * Unregister a previously-loaded CommonJS module from the debugger server.
   */
  unregisterModule: function(id) {
    let mod = gRegisteredModules[id];
    if (!mod) {
      throw new Error("Tried to unregister a module that was not previously registered.");
    }
    mod.module.unregister(mod.api);
    mod.api.destroy();
    delete gRegisteredModules[id];
  },

  /**
   * Install Firefox-specific actors.
   *
   * /!\ Be careful when adding a new actor, especially global actors.
   * Any new global actor will be exposed and returned by the root actor.
   *
   * That's the reason why tab actors aren't loaded on demand via
   * restrictPrivileges=true, to prevent exposing them on b2g parent process's
   * root actor.
   */
  addBrowserActors: function(aWindowType = "navigator:browser", restrictPrivileges = false) {
    this.chromeWindowType = aWindowType;
    this.registerModule("devtools/server/actors/webbrowser");

    if (!restrictPrivileges) {
      this.addTabActors();
      let { ChromeDebuggerActor } = require("devtools/server/actors/script");
      this.addGlobalActor(ChromeDebuggerActor, "chromeDebugger");
      this.registerModule("devtools/server/actors/preference");
    }

    this.addActors("resource://gre/modules/devtools/server/actors/webapps.js");
    this.registerModule("devtools/server/actors/device");
  },

  /**
   * Install tab actors in documents loaded in content childs
   */
  addChildActors: function () {
    // In case of apps being loaded in parent process, DebuggerServer is already
    // initialized and browser actors are already loaded,
    // but childtab.js hasn't been loaded yet.
    if (!DebuggerServer.tabActorFactories.hasOwnProperty("consoleActor")) {
      this.addTabActors();
    }
    // But webbrowser.js and childtab.js aren't loaded from shell.js.
    if (!this.isModuleRegistered("devtools/server/actors/webbrowser")) {
      this.registerModule("devtools/server/actors/webbrowser");
    }
    if (!("ContentActor" in this)) {
      this.addActors("resource://gre/modules/devtools/server/actors/childtab.js");
    }
  },

  /**
   * Install tab actors.
   */
  addTabActors: function() {
    this.registerModule("devtools/server/actors/script");
    this.registerModule("devtools/server/actors/webconsole");
    this.registerModule("devtools/server/actors/inspector");
    this.registerModule("devtools/server/actors/call-watcher");
    this.registerModule("devtools/server/actors/canvas");
    this.registerModule("devtools/server/actors/webgl");
    this.registerModule("devtools/server/actors/webaudio");
    this.registerModule("devtools/server/actors/stylesheets");
    this.registerModule("devtools/server/actors/styleeditor");
    this.registerModule("devtools/server/actors/storage");
    this.registerModule("devtools/server/actors/gcli");
    this.registerModule("devtools/server/actors/tracer");
    this.registerModule("devtools/server/actors/memory");
    this.registerModule("devtools/server/actors/framerate");
    this.registerModule("devtools/server/actors/eventlooplag");
    this.registerModule("devtools/server/actors/layout");
    this.registerModule("devtools/server/actors/csscoverage");
    if ("nsIProfiler" in Ci) {
      this.addActors("resource://gre/modules/devtools/server/actors/profiler.js");
    }
  },

  /**
   * Passes a set of options to the BrowserAddonActors for the given ID.
   *
   * @param aId string
   *        The ID of the add-on to pass the options to
   * @param aOptions object
   *        The options.
   * @return a promise that will be resolved when complete.
   */
  setAddonOptions: function DS_setAddonOptions(aId, aOptions) {
    if (!this._initialized) {
      return;
    }

    let promises = [];

    // Pass to all connections
    for (let connID of Object.getOwnPropertyNames(this._connections)) {
      promises.push(this._connections[connID].setAddonOptions(aId, aOptions));
    }

    return all(promises);
  },

  /**
   * Listens on the given port or socket file for remote debugger connections.
   *
   * @param aPortOrPath int, string
   *        If given an integer, the port to listen on.
   *        Otherwise, the path to the unix socket domain file to listen on.
   */
  openListener: function DS_openListener(aPortOrPath) {
    if (!Services.prefs.getBoolPref("devtools.debugger.remote-enabled")) {
      return false;
    }
    this._checkInit();

    // Return early if the server is already listening.
    if (this._listener) {
      return true;
    }

    let flags = Ci.nsIServerSocket.KeepWhenOffline;
    // A preference setting can force binding on the loopback interface.
    if (Services.prefs.getBoolPref("devtools.debugger.force-local")) {
      flags |= Ci.nsIServerSocket.LoopbackOnly;
    }

    try {
      let backlog = 4;
      let socket;
      let port = Number(aPortOrPath);
      if (port) {
        socket = new ServerSocket(port, flags, backlog);
      } else {
        let file = nsFile(aPortOrPath);
        if (file.exists())
          file.remove(false);
        socket = new UnixDomainServerSocket(file, parseInt("666", 8), backlog);
      }
      socket.asyncListen(this);
      this._listener = socket;
    } catch (e) {
      dumpn("Could not start debugging listener on '" + aPortOrPath + "': " + e);
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
    this._socketConnections++;

    return true;
  },

  /**
   * Close a previously-opened TCP listener.
   *
   * @param aForce boolean [optional]
   *        If set to true, then the socket will be closed, regardless of the
   *        number of open connections.
   */
  closeListener: function DS_closeListener(aForce) {
    if (!this._listener || this._socketConnections == 0) {
      return false;
    }

    // Only close the listener when the last connection is closed, or if the
    // aForce flag is passed.
    if (--this._socketConnections == 0 || aForce) {
      this._listener.close();
      this._listener = null;
      this._socketConnections = 0;
    }

    return true;
  },

  /**
   * Creates a new connection to the local debugger speaking over a fake
   * transport. This connection results in straightforward calls to the onPacket
   * handlers of each side.
   *
   * @param aPrefix string [optional]
   *    If given, all actors in this connection will have names starting
   *    with |aPrefix + '/'|.
   * @returns a client-side DebuggerTransport for communicating with
   *    the newly-created connection.
   */
  connectPipe: function DS_connectPipe(aPrefix) {
    this._checkInit();

    let serverTransport = new LocalDebuggerTransport;
    let clientTransport = new LocalDebuggerTransport(serverTransport);
    serverTransport.other = clientTransport;
    let connection = this._onConnection(serverTransport, aPrefix);

    // I'm putting this here because I trust you.
    //
    // There are times, when using a local connection, when you're going
    // to be tempted to just get direct access to the server.  Resist that
    // temptation!  If you succumb to that temptation, you will make the
    // fine developers that work on Fennec and Firefox OS sad.  They're
    // professionals, they'll try to act like they understand, but deep
    // down you'll know that you hurt them.
    //
    // This reference allows you to give in to that temptation.  There are
    // times this makes sense: tests, for example, and while porting a
    // previously local-only codebase to the remote protocol.
    //
    // But every time you use this, you will feel the shame of having
    // used a property that starts with a '_'.
    clientTransport._serverConnection = connection;

    return clientTransport;
  },

  /**
   * In a content child process, create a new connection that exchanges
   * nsIMessageSender messages with our parent process.
   *
   * @param aPrefix
   *    The prefix we should use in our nsIMessageSender message names and
   *    actor names. This connection will use messages named
   *    "debug:<prefix>:packet", and all its actors will have names
   *    beginning with "<prefix>/".
   */
  connectToParent: function(aPrefix, aMessageManager) {
    this._checkInit();

    let transport = new ChildDebuggerTransport(aMessageManager, aPrefix);
    return this._onConnection(transport, aPrefix, true);
  },

  /**
   * Connect to a child process.
   *
   * @param object aConnection
   *        The debugger server connection to use.
   * @param nsIDOMElement aFrame
   *        The browser element that holds the child process.
   * @param function [aOnDisconnect]
   *        Optional function to invoke when the child is disconnected.
   * @return object
   *         A promise object that is resolved once the connection is
   *         established.
   */
  connectToChild: function(aConnection, aFrame, aOnDisconnect) {
    let deferred = defer();

    let mm = aFrame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader
             .messageManager;
    mm.loadFrameScript("resource://gre/modules/devtools/server/child.js", false);

    let actor, childTransport;
    let prefix = aConnection.allocID("child");
    let netMonitor = null;

    let onActorCreated = DevToolsUtils.makeInfallible(function (msg) {
      mm.removeMessageListener("debug:actor", onActorCreated);

      // Pipe Debugger message from/to parent/child via the message manager
      childTransport = new ChildDebuggerTransport(mm, prefix);
      childTransport.hooks = {
        onPacket: aConnection.send.bind(aConnection),
        onClosed: function () {}
      };
      childTransport.ready();

      aConnection.setForwarding(prefix, childTransport);

      dumpn("establishing forwarding for app with prefix " + prefix);

      actor = msg.json.actor;

      let { NetworkMonitorManager } = require("devtools/toolkit/webconsole/network-monitor");
      netMonitor = new NetworkMonitorManager(aFrame, actor.actor);

      deferred.resolve(actor);
    }).bind(this);
    mm.addMessageListener("debug:actor", onActorCreated);

    let onMessageManagerDisconnect = DevToolsUtils.makeInfallible(function (subject, topic, data) {
      if (subject == mm) {
        Services.obs.removeObserver(onMessageManagerDisconnect, topic);
        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          childTransport = null;
          aConnection.cancelForwarding(prefix);
        } else {
          // Otherwise, the app has been closed before the actor
          // had a chance to be created, so we are not able to create
          // the actor.
          deferred.resolve(null);
        }
        if (actor) {
          // The ContentActor within the child process doesn't necessary
          // have to time to uninitialize itself when the app is closed/killed.
          // So ensure telling the client that the related actor is detached.
          aConnection.send({ from: actor.actor, type: "tabDetached" });
          actor = null;
        }

        if (netMonitor) {
          netMonitor.destroy();
          netMonitor = null;
        }

        if (aOnDisconnect) {
          aOnDisconnect(mm);
        }
      }
    }).bind(this);
    Services.obs.addObserver(onMessageManagerDisconnect,
                             "message-manager-disconnect", false);

    events.once(aConnection, "closed", () => {
      if (childTransport) {
        // When the client disconnects, we have to unplug the dedicated
        // ChildDebuggerTransport...
        childTransport.close();
        childTransport = null;
        aConnection.cancelForwarding(prefix);

        // ... and notify the child process to clean the tab actors.
        mm.sendAsyncMessage("debug:disconnect");
      }
      Services.obs.removeObserver(onMessageManagerDisconnect, "message-manager-disconnect");
    });

    mm.sendAsyncMessage("debug:connect", { prefix: prefix });

    return deferred.promise;
  },

  // nsIServerSocketListener implementation

  onSocketAccepted:
  DevToolsUtils.makeInfallible(function DS_onSocketAccepted(aSocket, aTransport) {
    if (Services.prefs.getBoolPref("devtools.debugger.prompt-connection") && !this._allowConnection()) {
      return;
    }
    dumpn("New debugging connection on " + aTransport.host + ":" + aTransport.port);

    let input = aTransport.openInputStream(0, 0, 0);
    let output = aTransport.openOutputStream(0, 0, 0);
    let transport = new DebuggerTransport(input, output);
    DebuggerServer._onConnection(transport);
  }, "DebuggerServer.onSocketAccepted"),

  onStopListening: function DS_onStopListening(aSocket, status) {
    dumpn("onStopListening, status: " + status);
  },

  /**
   * Raises an exception if the server has not been properly initialized.
   */
  _checkInit: function DS_checkInit() {
    if (!this._transportInitialized) {
      throw "DebuggerServer has not been initialized.";
    }

    if (!this.createRootActor) {
      throw "Use DebuggerServer.addActors() to add a root actor implementation.";
    }
  },

  /**
   * Create a new debugger connection for the given transport. Called after
   * connectPipe(), from connectToParent, or from an incoming socket
   * connection handler.
   *
   * If present, |aForwardingPrefix| is a forwarding prefix that a parent
   * server is using to recognizes messages intended for this server. Ensure
   * that all our actors have names beginning with |aForwardingPrefix + '/'|.
   * In particular, the root actor's name will be |aForwardingPrefix + '/root'|.
   */
  _onConnection: function DS_onConnection(aTransport, aForwardingPrefix, aNoRootActor = false) {
    let connID;
    if (aForwardingPrefix) {
      connID = aForwardingPrefix + "/";
    } else {
      connID = "conn" + this._nextConnID++ + '.';
    }
    let conn = new DebuggerServerConnection(connID, aTransport);
    this._connections[connID] = conn;

    // Create a root actor for the connection and send the hello packet.
    if (!aNoRootActor) {
      conn.rootActor = this.createRootActor(conn);
      if (aForwardingPrefix)
        conn.rootActor.actorID = aForwardingPrefix + "/root";
      else
        conn.rootActor.actorID = "root";
      conn.addActor(conn.rootActor);
      aTransport.send(conn.rootActor.sayHello());
    }
    aTransport.ready();

    this.emit("connectionchange", "opened", conn);
    return conn;
  },

  /**
   * Remove the connection from the debugging server.
   */
  _connectionClosed: function DS_connectionClosed(aConnection) {
    delete this._connections[aConnection.prefix];
    this.emit("connectionchange", "closed", aConnection);
  },

  // DebuggerServer extension API.

  setRootActor: function DS_setRootActor(aFunction) {
    this.createRootActor = aFunction;
  },

  /**
   * Registers handlers for new tab-scoped request types defined dynamically.
   * This is used for example by add-ons to augment the functionality of the tab
   * actor. Note that the name or actorPrefix of the request type is not allowed
   * to clash with existing protocol packet properties, like 'title', 'url' or
   * 'actor', since that would break the protocol.
   *
   * @param aFunction function
   *        The constructor function for this request type. This expects to be
   *        called as a constructor (i.e. with 'new'), and passed two
   *        arguments: the DebuggerServerConnection, and the BrowserTabActor
   *        with which it will be associated.
   *
   * @param aName string [optional]
   *        The name of the new request type. If this is not present, the
   *        actorPrefix property of the constructor prototype is used.
   */
  addTabActor: function DS_addTabActor(aFunction, aName) {
    let name = aName ? aName : aFunction.prototype.actorPrefix;
    if (["title", "url", "actor"].indexOf(name) != -1) {
      throw Error(name + " is not allowed");
    }
    if (DebuggerServer.tabActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    DebuggerServer.tabActorFactories[name] = aFunction;
  },

  /**
   * Unregisters the handler for the specified tab-scoped request type.
   * This may be used for example by add-ons when shutting down or upgrading.
   *
   * @param aFunction function
   *        The constructor function for this request type.
   */
  removeTabActor: function DS_removeTabActor(aFunction) {
    for (let name in DebuggerServer.tabActorFactories) {
      let handler = DebuggerServer.tabActorFactories[name];
      if (handler.name == aFunction.name) {
        delete DebuggerServer.tabActorFactories[name];
      }
    }
  },

  /**
   * Registers handlers for new browser-scoped request types defined
   * dynamically. This is used for example by add-ons to augment the
   * functionality of the root actor. Note that the name or actorPrefix of the
   * request type is not allowed to clash with existing protocol packet
   * properties, like 'from', 'tabs' or 'selected', since that would break the
   * protocol.
   *
   * @param aFunction function
   *        The constructor function for this request type. This expects to be
   *        called as a constructor (i.e. with 'new'), and passed two
   *        arguments: the DebuggerServerConnection, and the BrowserRootActor
   *        with which it will be associated.
   *
   * @param aName string [optional]
   *        The name of the new request type. If this is not present, the
   *        actorPrefix property of the constructor prototype is used.
   */
  addGlobalActor: function DS_addGlobalActor(aFunction, aName) {
    let name = aName ? aName : aFunction.prototype.actorPrefix;
    if (["from", "tabs", "selected"].indexOf(name) != -1) {
      throw Error(name + " is not allowed");
    }
    if (DebuggerServer.globalActorFactories.hasOwnProperty(name)) {
      throw Error(name + " already exists");
    }
    DebuggerServer.globalActorFactories[name] = aFunction;
  },

  /**
   * Unregisters the handler for the specified browser-scoped request type.
   * This may be used for example by add-ons when shutting down or upgrading.
   *
   * @param aFunction function
   *        The constructor function for this request type.
   */
  removeGlobalActor: function DS_removeGlobalActor(aFunction) {
    for (let name in DebuggerServer.globalActorFactories) {
      let handler = DebuggerServer.globalActorFactories[name];
      if (handler.name == aFunction.name) {
        delete DebuggerServer.globalActorFactories[name];
      }
    }
  }
};

EventEmitter.decorate(DebuggerServer);

if (this.exports) {
  exports.DebuggerServer = DebuggerServer;
}
// Needed on B2G (See header note)
this.DebuggerServer = DebuggerServer;

// When using DebuggerServer.addActors, some symbols are expected to be in
// the scope of the added actor even before the corresponding modules are
// loaded, so let's explicitly bind the expected symbols here.
let includes = ["Components", "Ci", "Cu", "require", "Services", "DebuggerServer",
                "ActorPool", "DevToolsUtils"];
includes.forEach(name => {
  DebuggerServer[name] = this[name];
});

// Export ActorPool for requirers of main.js
if (this.exports) {
  exports.ActorPool = ActorPool;
}

/**
 * Creates a DebuggerServerConnection.
 *
 * Represents a connection to this debugging global from a client.
 * Manages a set of actors and actor pools, allocates actor ids, and
 * handles incoming requests.
 *
 * @param aPrefix string
 *        All actor IDs created by this connection should be prefixed
 *        with aPrefix.
 * @param aTransport transport
 *        Packet transport for the debugging protocol.
 */
function DebuggerServerConnection(aPrefix, aTransport)
{
  this._prefix = aPrefix;
  this._transport = aTransport;
  this._transport.hooks = this;
  this._nextID = 1;

  this._actorPool = new ActorPool(this);
  this._extraPools = [];

  // Responses to a given actor must be returned the the client
  // in the same order as the requests that they're replying to, but
  // Implementations might finish serving requests in a different
  // order.  To keep things in order we generate a promise for each
  // request, chained to the promise for the request before it.
  // This map stores the latest request promise in the chain, keyed
  // by an actor ID string.
  this._actorResponses = new Map;

  /*
   * We can forward packets to other servers, if the actors on that server
   * all use a distinct prefix on their names. This is a map from prefixes
   * to transports: it maps a prefix P to a transport T if T conveys
   * packets to the server whose actors' names all begin with P + "/".
   */
  this._forwardingPrefixes = new Map;
}

DebuggerServerConnection.prototype = {
  _prefix: null,
  get prefix() { return this._prefix },

  _transport: null,
  get transport() { return this._transport },

  close: function() {
    this._transport.close();
  },

  send: function DSC_send(aPacket) {
    this.transport.send(aPacket);
  },

  /**
   * Used when sending a bulk reply from an actor.
   * @see DebuggerTransport.prototype.startBulkSend
   */
  startBulkSend: function(header) {
    return this.transport.startBulkSend(header);
  },

  allocID: function DSC_allocID(aPrefix) {
    return this.prefix + (aPrefix || '') + this._nextID++;
  },

  /**
   * Add a map of actor IDs to the connection.
   */
  addActorPool: function DSC_addActorPool(aActorPool) {
    this._extraPools.push(aActorPool);
  },

  /**
   * Remove a previously-added pool of actors to the connection.
   *
   * @param ActorPool aActorPool
   *        The ActorPool instance you want to remove.
   * @param boolean aNoCleanup [optional]
   *        True if you don't want to disconnect each actor from the pool, false
   *        otherwise.
   */
  removeActorPool: function DSC_removeActorPool(aActorPool, aNoCleanup) {
    let index = this._extraPools.lastIndexOf(aActorPool);
    if (index > -1) {
      let pool = this._extraPools.splice(index, 1);
      if (!aNoCleanup) {
        pool.map(function(p) { p.cleanup(); });
      }
    }
  },

  /**
   * Add an actor to the default actor pool for this connection.
   */
  addActor: function DSC_addActor(aActor) {
    this._actorPool.addActor(aActor);
  },

  /**
   * Remove an actor to the default actor pool for this connection.
   */
  removeActor: function DSC_removeActor(aActor) {
    this._actorPool.removeActor(aActor);
  },

  /**
   * Match the api expected by the protocol library.
   */
  unmanage: function(aActor) {
    return this.removeActor(aActor);
  },

  /**
   * Look up an actor implementation for an actorID.  Will search
   * all the actor pools registered with the connection.
   *
   * @param aActorID string
   *        Actor ID to look up.
   */
  getActor: function DSC_getActor(aActorID) {
    let pool = this.poolFor(aActorID);
    if (pool) {
      return pool.get(aActorID);
    }

    if (aActorID === "root") {
      return this.rootActor;
    }

    return null;
  },

  _getOrCreateActor: function(actorID) {
    let actor = this.getActor(actorID);
    if (!actor) {
      this.transport.send({ from: actorID ? actorID : "root",
                            error: "noSuchActor",
                            message: "No such actor for ID: " + actorID });
      return;
    }

    // Dyamically-loaded actors have to be created lazily.
    if (typeof actor == "function") {
      let instance;
      try {
        instance = new actor();
      } catch (e) {
        this.transport.send(this._unknownError(
          "Error occurred while creating actor '" + actor.name,
          e));
      }
      instance.parentID = actor.parentID;
      // We want the newly-constructed actor to completely replace the factory
      // actor. Reusing the existing actor ID will make sure ActorPool.addActor
      // does the right thing.
      instance.actorID = actor.actorID;
      actor.registeredPool.addActor(instance);
      actor = instance;
    }

    return actor;
  },

  poolFor: function DSC_actorPool(aActorID) {
    if (this._actorPool && this._actorPool.has(aActorID)) {
      return this._actorPool;
    }

    for (let pool of this._extraPools) {
      if (pool.has(aActorID)) {
        return pool;
      }
    }
    return null;
  },

  _unknownError: function DSC__unknownError(aPrefix, aError) {
    let errorString = aPrefix + ": " + DevToolsUtils.safeErrorString(aError);
    reportError(errorString);
    dumpn(errorString);
    return {
      error: "unknownError",
      message: errorString
    };
  },

  _queueResponse: function(from, type, response) {
    let pendingResponse = this._actorResponses.get(from) || resolve(null);
    let responsePromise = pendingResponse.then(() => {
      return response;
    }).then(aResponse => {
      if (!aResponse.from) {
        aResponse.from = from;
      }
      this.transport.send(aResponse);
    }).then(null, (e) => {
      let errorPacket = this._unknownError(
        "error occurred while processing '" + type,
        e);
      errorPacket.from = from;
      this.transport.send(errorPacket);
    });

    this._actorResponses.set(from, responsePromise);
  },

  /**
   * Passes a set of options to the BrowserAddonActors for the given ID.
   *
   * @param aId string
   *        The ID of the add-on to pass the options to
   * @param aOptions object
   *        The options.
   * @return a promise that will be resolved when complete.
   */
  setAddonOptions: function DSC_setAddonOptions(aId, aOptions) {
    let addonList = this.rootActor._parameters.addonList;
    if (!addonList) {
      return resolve();
    }
    return addonList.getList().then((addonActors) => {
      for (let actor of addonActors) {
        if (actor.id != aId) {
          continue;
        }
        actor.setOptions(aOptions);
        return;
      }
    });
  },

  /* Forwarding packets to other transports based on actor name prefixes. */

  /*
   * Arrange to forward packets to another server. This is how we
   * forward debugging connections to child processes.
   *
   * If we receive a packet for an actor whose name begins with |aPrefix|
   * followed by '/', then we will forward that packet to |aTransport|.
   *
   * This overrides any prior forwarding for |aPrefix|.
   *
   * @param aPrefix string
   *    The actor name prefix, not including the '/'.
   * @param aTransport object
   *    A packet transport to which we should forward packets to actors
   *    whose names begin with |(aPrefix + '/').|
   */
  setForwarding: function(aPrefix, aTransport) {
    this._forwardingPrefixes.set(aPrefix, aTransport);
  },

  /*
   * Stop forwarding messages to actors whose names begin with
   * |aPrefix+'/'|. Such messages will now elicit 'noSuchActor' errors.
   */
  cancelForwarding: function(aPrefix) {
    this._forwardingPrefixes.delete(aPrefix);
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param aPacket object
   *        The incoming packet.
   */
  onPacket: function DSC_onPacket(aPacket) {
    // If the actor's name begins with a prefix we've been asked to
    // forward, do so.
    //
    // Note that the presence of a prefix alone doesn't indicate that
    // forwarding is needed: in DebuggerServerConnection instances in child
    // processes, every actor has a prefixed name.
    if (this._forwardingPrefixes.size > 0) {
      let separator = aPacket.to.indexOf('/');
      if (separator >= 0) {
        let forwardTo = this._forwardingPrefixes.get(aPacket.to.substring(0, separator));
        if (forwardTo) {
          forwardTo.send(aPacket);
          return;
        }
      }
    }

    let actor = this._getOrCreateActor(aPacket.to);
    if (!actor) {
      return;
    }

    var ret = null;

    // handle "requestTypes" RDP request.
    if (aPacket.type == "requestTypes") {
      ret = { from: actor.actorID, requestTypes: Object.keys(actor.requestTypes) };
    } else if (actor.requestTypes && actor.requestTypes[aPacket.type]) {
      // Dispatch the request to the actor.
      try {
        this.currentPacket = aPacket;
        ret = actor.requestTypes[aPacket.type].bind(actor)(aPacket, this);
      } catch(e) {
        this.transport.send(this._unknownError(
          "error occurred while processing '" + aPacket.type,
          e));
      } finally {
        this.currentPacket = undefined;
      }
    } else {
      ret = { error: "unrecognizedPacketType",
              message: ("Actor " + actor.actorID +
                        " does not recognize the packet type " +
                        aPacket.type) };
    }

    // There will not be a return value if a bulk reply is sent.
    if (ret) {
      this._queueResponse(aPacket.to, aPacket.type, ret);
    }
  },

  /**
   * Called by the DebuggerTransport to dispatch incoming bulk packets as
   * appropriate.
   *
   * @param packet object
   *        The incoming packet, which contains:
   *        * actor:  Name of actor that will receive the packet
   *        * type:   Name of actor's method that should be called on receipt
   *        * length: Size of the data to be read
   *        * stream: This input stream should only be used directly if you can
   *                  ensure that you will read exactly |length| bytes and will
   *                  not close the stream when reading is complete
   *        * done:   If you use the stream directly (instead of |copyTo|
   *                  below), you must signal completion by resolving /
   *                  rejecting this deferred.  If it's rejected, the transport
   *                  will be closed.  If an Error is supplied as a rejection
   *                  value, it will be logged via |dumpn|.  If you do use
   *                  |copyTo|, resolving is taken care of for you when copying
   *                  completes.
   *        * copyTo: A helper function for getting your data out of the stream
   *                  that meets the stream handling requirements above, and has
   *                  the following signature:
   *          @param  output nsIAsyncOutputStream
   *                  The stream to copy to.
   *          @return Promise
   *                  The promise is resolved when copying completes or rejected
   *                  if any (unexpected) errors occur.
   *                  This object also emits "progress" events for each chunk
   *                  that is copied.  See stream-utils.js.
   */
  onBulkPacket: function(packet) {
    let { actor: actorKey, type, length } = packet;

    let actor = this._getOrCreateActor(actorKey);
    if (!actor) {
      return;
    }

    // Dispatch the request to the actor.
    let ret;
    if (actor.requestTypes && actor.requestTypes[type]) {
      try {
        ret = actor.requestTypes[type].call(actor, packet);
      } catch(e) {
        this.transport.send(this._unknownError(
          "error occurred while processing bulk packet '" + type, e));
        packet.done.reject(e);
      }
    } else {
      let message = "Actor " + actorKey +
                    " does not recognize the bulk packet type " + type;
      ret = { error: "unrecognizedPacketType",
              message: message };
      packet.done.reject(new Error(message));
    }

    // If there is a JSON response, queue it for sending back to the client.
    if (ret) {
      this._queueResponse(actorKey, type, ret);
    }
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param aStatus nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed: function DSC_onClosed(aStatus) {
    dumpn("Cleaning up connection.");
    if (!this._actorPool) {
      // Ignore this call if the connection is already closed.
      return;
    }
    events.emit(this, "closed", aStatus);

    this._actorPool.cleanup();
    this._actorPool = null;
    this._extraPools.map(function(p) { p.cleanup(); });
    this._extraPools = null;

    DebuggerServer._connectionClosed(this);
  },

  /*
   * Debugging helper for inspecting the state of the actor pools.
   */
  _dumpPools: function DSC_dumpPools() {
    dumpn("/-------------------- dumping pools:");
    if (this._actorPool) {
      dumpn("--------------------- actorPool actors: " +
            uneval(Object.keys(this._actorPool._actors)));
    }
    for each (let pool in this._extraPools)
      dumpn("--------------------- extraPool actors: " +
            uneval(Object.keys(pool._actors)));
  },

  /*
   * Debugging helper for inspecting the state of an actor pool.
   */
  _dumpPool: function DSC_dumpPools(aPool) {
    dumpn("/-------------------- dumping pool:");
    dumpn("--------------------- actorPool actors: " +
          uneval(Object.keys(aPool._actors)));
  }
};
