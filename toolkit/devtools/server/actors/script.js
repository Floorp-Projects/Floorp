/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Cc, Ci, Cu, components, ChromeWorker } = require("chrome");
const { ActorPool, getOffsetColumn } = require("devtools/server/actors/common");
const { DebuggerServer } = require("devtools/server/main");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
const { dbg_assert, dumpn, update, fetch } = DevToolsUtils;
const { dirname, joinURI } = require("devtools/toolkit/path");
const promise = require("promise");
const PromiseDebugging = require("PromiseDebugging");
const xpcInspector = require("xpcInspector");
const mapURIToAddonID = require("./utils/map-uri-to-addon-id");
const ScriptStore = require("./utils/ScriptStore");

const { defer, resolve, reject, all } = require("devtools/toolkit/deprecated-sync-thenables");

loader.lazyGetter(this, "Debugger", () => {
  let Debugger = require("Debugger");
  hackDebugger(Debugger);
  return Debugger;
});
loader.lazyRequireGetter(this, "SourceMapConsumer", "source-map", true);
loader.lazyRequireGetter(this, "SourceMapGenerator", "source-map", true);
loader.lazyRequireGetter(this, "CssLogic", "devtools/styleinspector/css-logic", true);

let TYPED_ARRAY_CLASSES = ["Uint8Array", "Uint8ClampedArray", "Uint16Array",
      "Uint32Array", "Int8Array", "Int16Array", "Int32Array", "Float32Array",
      "Float64Array"];

// Number of items to preview in objects, arrays, maps, sets, lists,
// collections, etc.
let OBJECT_PREVIEW_MAX_ITEMS = 10;

/**
 * Call PromiseDebugging.getState on this Debugger.Object's referent and wrap
 * the resulting `value` or `reason` properties in a Debugger.Object instance.
 *
 * See dom/webidl/PromiseDebugging.webidl
 *
 * @returns Object
 *          An object of one of the following forms:
 *          - { state: "pending" }
 *          - { state: "fulfilled", value }
 *          - { state: "rejected", reason }
 */
function getPromiseState(obj) {
  if (obj.class != "Promise") {
    throw new Error(
      "Can't call `getPromiseState` on `Debugger.Object`s that don't " +
      "refer to Promise objects.");
  }

  const state = PromiseDebugging.getState(obj.unsafeDereference());
  return {
    state: state.state,
    value: obj.makeDebuggeeValue(state.value),
    reason: obj.makeDebuggeeValue(state.reason)
  };
};

/**
 * A BreakpointActorMap is a map from locations to instances of BreakpointActor.
 */
function BreakpointActorMap() {
  this._size = 0;
  this._actors = {};
}

BreakpointActorMap.prototype = {
  /**
   * Return the number of instances of BreakpointActor in this instance of
   * BreakpointActorMap.
   *
   * @returns Number
   *          The number of instances of BreakpointACtor in this instance of
   *          BreakpointActorMap.
   */
  get size() {
    return this._size;
  },

  /**
   * Generate all instances of BreakpointActor that match the given query in
   * this instance of BreakpointActorMap.
   *
   * @param Object query
   *        An optional object with the following properties:
   *        - source (optional)
   *        - line (optional, requires source)
   *        - column (optional, requires line)
   */
  findActors: function* (query = {}) {
    function* findKeys(object, key) {
      if (key !== undefined) {
        if (key in object) {
          yield key;
        }
      }
      else {
        for (let key of Object.keys(object)) {
          yield key;
        }
      }
    }

    query.sourceActorID = query.sourceActor ? query.sourceActor.actorID : undefined;
    query.beginColumn = query.column ? query.column : undefined;
    query.endColumn = query.column ? query.column + 1 : undefined;

    for (let sourceActorID of findKeys(this._actors, query.sourceActorID))
    for (let line of findKeys(this._actors[sourceActorID], query.line))
    for (let beginColumn of findKeys(this._actors[sourceActorID][line], query.beginColumn))
    for (let endColumn of findKeys(this._actors[sourceActorID][line][beginColumn], query.endColumn)) {
      yield this._actors[sourceActorID][line][beginColumn][endColumn];
    }
  },

  /**
   * Return the instance of BreakpointActor at the given location in this
   * instance of BreakpointActorMap.
   *
   * @param Object location
   *        An object with the following properties:
   *        - source
   *        - line
   *        - column (optional)
   *
   * @returns BreakpointActor actor
   *          The instance of BreakpointActor at the given location.
   */
  getActor: function (location) {
    for (let actor of this.findActors(location)) {
      return actor;
    }

    return null;
  },

  /**
   * Set the given instance of BreakpointActor to the given location in this
   * instance of BreakpointActorMap.
   *
   * @param Object location
   *        An object with the following properties:
   *        - source
   *        - line
   *        - column (optional)
   *
   * @param BreakpointActor actor
   *        The instance of BreakpointActor to be set to the given location.
   */
  setActor: function (location, actor) {
    let { sourceActor, line, column } = location;

    let sourceActorID = sourceActor.actorID;
    let beginColumn = column ? column : 0;
    let endColumn = column ? column + 1 : Infinity;

    if (!this._actors[sourceActorID]) {
      this._actors[sourceActorID] = [];
    }
    if (!this._actors[sourceActorID][line]) {
      this._actors[sourceActorID][line] = [];
    }
    if (!this._actors[sourceActorID][line][beginColumn]) {
      this._actors[sourceActorID][line][beginColumn] = [];
    }
    if (!this._actors[sourceActorID][line][beginColumn][endColumn]) {
      ++this._size;
    }
    this._actors[sourceActorID][line][beginColumn][endColumn] = actor;
  },

  /**
   * Delete the instance of BreakpointActor from the given location in this
   * instance of BreakpointActorMap.
   *
   * @param Object location
   *        An object with the following properties:
   *        - source
   *        - line
   *        - column (optional)
   */
  deleteActor: function (location) {
    let { sourceActor, line, column } = location;

    let sourceActorID = sourceActor.actorID;
    let beginColumn = column ? column : 0;
    let endColumn = column ? column + 1 : Infinity;

    if (this._actors[sourceActorID]) {
      if (this._actors[sourceActorID][line]) {
        if (this._actors[sourceActorID][line][beginColumn]) {
          if (this._actors[sourceActorID][line][beginColumn][endColumn]) {
            --this._size;
          }
          delete this._actors[sourceActorID][line][beginColumn][endColumn];
          if (Object.keys(this._actors[sourceActorID][line][beginColumn]).length === 0) {
            delete this._actors[sourceActorID][line][beginColumn];
          }
        }
        if (Object.keys(this._actors[sourceActorID][line]).length === 0) {
          delete this._actors[sourceActorID][line];
        }
      }
    }
  }
};

exports.BreakpointActorMap = BreakpointActorMap;

/**
 * Keeps track of persistent sources across reloads and ties different
 * source instances to the same actor id so that things like
 * breakpoints survive reloads. ThreadSources uses this to force the
 * same actorID on a SourceActor.
 */
function SourceActorStore() {
  // source identifier --> actor id
  this._sourceActorIds = Object.create(null);
}

SourceActorStore.prototype = {
  /**
   * Lookup an existing actor id that represents this source, if available.
   */
  getReusableActorId: function(aSource, aOriginalUrl) {
    let url = this.getUniqueKey(aSource, aOriginalUrl);
    if (url && url in this._sourceActorIds) {
      return this._sourceActorIds[url];
    }
    return null;
  },

  /**
   * Update a source with an actorID.
   */
  setReusableActorId: function(aSource, aOriginalUrl, actorID) {
    let url = this.getUniqueKey(aSource, aOriginalUrl);
    if (url) {
      this._sourceActorIds[url] = actorID;
    }
  },

  /**
   * Make a unique URL from a source that identifies it across reloads.
   */
  getUniqueKey: function(aSource, aOriginalUrl) {
    if (aOriginalUrl) {
      // Original source from a sourcemap.
      return aOriginalUrl;
    }
    else {
      return getSourceURL(aSource);
    }
  }
};

exports.SourceActorStore = SourceActorStore;

/**
 * Manages pushing event loops and automatically pops and exits them in the
 * correct order as they are resolved.
 *
 * @param ThreadActor thread
 *        The thread actor instance that owns this EventLoopStack.
 * @param DebuggerServerConnection connection
 *        The remote protocol connection associated with this event loop stack.
 * @param Object hooks
 *        An object with the following properties:
 *          - url: The URL string of the debuggee we are spinning an event loop
 *                 for.
 *          - preNest: function called before entering a nested event loop
 *          - postNest: function called after exiting a nested event loop
 */
function EventLoopStack({ thread, connection, hooks }) {
  this._hooks = hooks;
  this._thread = thread;
  this._connection = connection;
}

EventLoopStack.prototype = {
  /**
   * The number of nested event loops on the stack.
   */
  get size() {
    return xpcInspector.eventLoopNestLevel;
  },

  /**
   * The URL of the debuggee who pushed the event loop on top of the stack.
   */
  get lastPausedUrl() {
    let url = null;
    if (this.size > 0) {
      try {
        url = xpcInspector.lastNestRequestor.url
      } catch (e) {
        // The tab's URL getter may throw if the tab is destroyed by the time
        // this code runs, but we don't really care at this point.
        dumpn(e);
      }
    }
    return url;
  },

  /**
   * The DebuggerServerConnection of the debugger who pushed the event loop on
   * top of the stack
   */
  get lastConnection() {
    return xpcInspector.lastNestRequestor._connection;
  },

  /**
   * Push a new nested event loop onto the stack.
   *
   * @returns EventLoop
   */
  push: function () {
    return new EventLoop({
      thread: this._thread,
      connection: this._connection,
      hooks: this._hooks
    });
  }
};

/**
 * An object that represents a nested event loop. It is used as the nest
 * requestor with nsIJSInspector instances.
 *
 * @param ThreadActor thread
 *        The thread actor that is creating this nested event loop.
 * @param DebuggerServerConnection connection
 *        The remote protocol connection associated with this event loop.
 * @param Object hooks
 *        The same hooks object passed into EventLoopStack during its
 *        initialization.
 */
function EventLoop({ thread, connection, hooks }) {
  this._thread = thread;
  this._hooks = hooks;
  this._connection = connection;

  this.enter = this.enter.bind(this);
  this.resolve = this.resolve.bind(this);
}

EventLoop.prototype = {
  entered: false,
  resolved: false,
  get url() { return this._hooks.url; },

  /**
   * Enter this nested event loop.
   */
  enter: function () {
    let nestData = this._hooks.preNest
      ? this._hooks.preNest()
      : null;

    this.entered = true;
    xpcInspector.enterNestedEventLoop(this);

    // Keep exiting nested event loops while the last requestor is resolved.
    if (xpcInspector.eventLoopNestLevel > 0) {
      const { resolved } = xpcInspector.lastNestRequestor;
      if (resolved) {
        xpcInspector.exitNestedEventLoop();
      }
    }

    dbg_assert(this._thread.state === "running",
               "Should be in the running state");

    if (this._hooks.postNest) {
      this._hooks.postNest(nestData);
    }
  },

  /**
   * Resolve this nested event loop.
   *
   * @returns boolean
   *          True if we exited this nested event loop because it was on top of
   *          the stack, false if there is another nested event loop above this
   *          one that hasn't resolved yet.
   */
  resolve: function () {
    if (!this.entered) {
      throw new Error("Can't resolve an event loop before it has been entered!");
    }
    if (this.resolved) {
      throw new Error("Already resolved this nested event loop!");
    }
    this.resolved = true;
    if (this === xpcInspector.lastNestRequestor) {
      xpcInspector.exitNestedEventLoop();
      return true;
    }
    return false;
  },
};

/**
 * JSD2 actors.
 */

/**
 * Creates a ThreadActor.
 *
 * ThreadActors manage a JSInspector object and manage execution/inspection
 * of debuggees.
 *
 * @param aParent object
 *        This |ThreadActor|'s parent actor. It must implement the following
 *        properties:
 *          - url: The URL string of the debuggee.
 *          - window: The global window object.
 *          - preNest: Function called before entering a nested event loop.
 *          - postNest: Function called after exiting a nested event loop.
 *          - makeDebugger: A function that takes no arguments and instantiates
 *            a Debugger that manages its globals on its own.
 * @param aGlobal object [optional]
 *        An optional (for content debugging only) reference to the content
 *        window.
 */
function ThreadActor(aParent, aGlobal)
{
  this._state = "detached";
  this._frameActors = [];
  this._parent = aParent;
  this._dbg = null;
  this._gripDepth = 0;
  this._threadLifetimePool = null;
  this._tabClosed = false;
  this._scripts = null;
  this._sources = null;

  this._options = {
    useSourceMaps: false,
    autoBlackBox: false
  };

  this.breakpointActorMap = new BreakpointActorMap;
  this.sourceActorStore = new SourceActorStore;
  this.blackBoxedSources = new Set;
  this.prettyPrintedSources = new Map;

  // A map of actorID -> actor for breakpoints created and managed by the
  // server.
  this._hiddenBreakpoints = new Map;

  this.global = aGlobal;

  this._allEventsListener = this._allEventsListener.bind(this);
  this.onNewGlobal = this.onNewGlobal.bind(this);
  this.onNewSource = this.onNewSource.bind(this);
  this.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
  this.onDebuggerStatement = this.onDebuggerStatement.bind(this);
  this.onNewScript = this.onNewScript.bind(this);
  // Set a wrappedJSObject property so |this| can be sent via the observer svc
  // for the xpcshell harness.
  this.wrappedJSObject = this;
}

ThreadActor.prototype = {
  // Used by the ObjectActor to keep track of the depth of grip() calls.
  _gripDepth: null,

  actorPrefix: "context",

  get dbg() {
    if (!this._dbg) {
      this._dbg = this._parent.makeDebugger();
      this._dbg.uncaughtExceptionHook = this.uncaughtExceptionHook;
      this._dbg.onDebuggerStatement = this.onDebuggerStatement;
      this._dbg.onNewScript = this.onNewScript;
      this._dbg.on("newGlobal", this.onNewGlobal);
      // Keep the debugger disabled until a client attaches.
      this._dbg.enabled = this._state != "detached";
    }
    return this._dbg;
  },

  get globalDebugObject() {
    return this.dbg.makeGlobalObjectReference(this._parent.window);
  },

  get state() { return this._state; },
  get attached() this.state == "attached" ||
                 this.state == "running" ||
                 this.state == "paused",

  get threadLifetimePool() {
    if (!this._threadLifetimePool) {
      this._threadLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._threadLifetimePool);
      this._threadLifetimePool.objectActors = new WeakMap();
    }
    return this._threadLifetimePool;
  },

  get scripts() {
    if (!this._scripts) {
      this._scripts = new ScriptStore();
      this._scripts.addScripts(this.dbg.findScripts());
    }
    return this._scripts;
  },

  get sources() {
    if (!this._sources) {
      this._sources = new ThreadSources(this, this._options,
                                        this._allowSource, this.onNewSource);
    }
    return this._sources;
  },

  get youngestFrame() {
    if (this.state != "paused") {
      return null;
    }
    return this.dbg.getNewestFrame();
  },

  _prettyPrintWorker: null,
  get prettyPrintWorker() {
    if (!this._prettyPrintWorker) {
      this._prettyPrintWorker = new ChromeWorker(
        "resource://gre/modules/devtools/server/actors/pretty-print-worker.js");

      this._prettyPrintWorker.addEventListener(
        "error", this._onPrettyPrintError, false);

      if (dumpn.wantLogging) {
        this._prettyPrintWorker.addEventListener("message", this._onPrettyPrintMsg, false);

        const postMsg = this._prettyPrintWorker.postMessage;
        this._prettyPrintWorker.postMessage = data => {
          dumpn("Sending message to prettyPrintWorker: "
                + JSON.stringify(data, null, 2) + "\n");
          return postMsg.call(this._prettyPrintWorker, data);
        };
      }
    }
    return this._prettyPrintWorker;
  },

  _onPrettyPrintError: function ({ message, filename, lineno }) {
    reportError(new Error(message + " @ " + filename + ":" + lineno));
  },

  _onPrettyPrintMsg: function ({ data }) {
    dumpn("Received message from prettyPrintWorker: "
          + JSON.stringify(data, null, 2) + "\n");
  },

  /**
   * Keep track of all of the nested event loops we use to pause the debuggee
   * when we hit a breakpoint/debugger statement/etc in one place so we can
   * resolve them when we get resume packets. We have more than one (and keep
   * them in a stack) because we can pause within client evals.
   */
  _threadPauseEventLoops: null,
  _pushThreadPause: function () {
    if (!this._threadPauseEventLoops) {
      this._threadPauseEventLoops = [];
    }
    const eventLoop = this._nestedEventLoops.push();
    this._threadPauseEventLoops.push(eventLoop);
    eventLoop.enter();
  },
  _popThreadPause: function () {
    const eventLoop = this._threadPauseEventLoops.pop();
    dbg_assert(eventLoop, "Should have an event loop.");
    eventLoop.resolve();
  },

  /**
   * Remove all debuggees and clear out the thread's sources.
   */
  clearDebuggees: function () {
    if (this._dbg) {
      this.dbg.removeAllDebuggees();
    }
    this._sources = null;
    this._scripts = null;
  },

  /**
   * Listener for our |Debugger|'s "newGlobal" event.
   */
  onNewGlobal: function (aGlobal) {
    // Notify the client.
    this.conn.send({
      from: this.actorID,
      type: "newGlobal",
      // TODO: after bug 801084 lands see if we need to JSONify this.
      hostAnnotations: aGlobal.hostAnnotations
    });
  },

  disconnect: function () {
    dumpn("in ThreadActor.prototype.disconnect");
    if (this._state == "paused") {
      this.onResume();
    }

    // Blow away our source actor ID store because those IDs are only
    // valid for this connection. This is ok because we never keep
    // things like breakpoints across connections.
    this._sourceActorStore = null;

    this.clearDebuggees();
    this.conn.removeActorPool(this._threadLifetimePool);
    this._threadLifetimePool = null;

    if (this._prettyPrintWorker) {
      this._prettyPrintWorker.removeEventListener(
        "error", this._onPrettyPrintError, false);
      this._prettyPrintWorker.removeEventListener(
        "message", this._onPrettyPrintMsg, false);
      this._prettyPrintWorker.terminate();
      this._prettyPrintWorker = null;
    }

    if (!this._dbg) {
      return;
    }
    this._dbg.enabled = false;
    this._dbg = null;
  },

  /**
   * Disconnect the debugger and put the actor in the exited state.
   */
  exit: function () {
    this.disconnect();
    this._state = "exited";
  },

  // Request handlers
  onAttach: function (aRequest) {
    if (this.state === "exited") {
      return { type: "exited" };
    }

    if (this.state !== "detached") {
      return { error: "wrongState",
               message: "Current state is " + this.state };
    }

    this._state = "attached";

    update(this._options, aRequest.options || {});

    // Initialize an event loop stack. This can't be done in the constructor,
    // because this.conn is not yet initialized by the actor pool at that time.
    this._nestedEventLoops = new EventLoopStack({
      hooks: this._parent,
      connection: this.conn,
      thread: this
    });

    this.dbg.addDebuggees();
    this.dbg.enabled = true;
    try {
      // Put ourselves in the paused state.
      let packet = this._paused();
      if (!packet) {
        return { error: "notAttached" };
      }
      packet.why = { type: "attached" };

      this._restoreBreakpoints();

      // Send the response to the attach request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send(packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch (e) {
      reportError(e);
      return { error: "notAttached", message: e.toString() };
    }
  },

  onDetach: function (aRequest) {
    this.disconnect();
    this._state = "detached";

    dumpn("ThreadActor.prototype.onDetach: returning 'detached' packet");
    return {
      type: "detached"
    };
  },

  onReconfigure: function (aRequest) {
    if (this.state == "exited") {
      return { error: "wrongState" };
    }

    update(this._options, aRequest.options || {});
    // Clear existing sources, so they can be recreated on next access.
    this._sources = null;

    return {};
  },

  /**
   * Pause the debuggee, by entering a nested event loop, and return a 'paused'
   * packet to the client.
   *
   * @param Debugger.Frame aFrame
   *        The newest debuggee frame in the stack.
   * @param object aReason
   *        An object with a 'type' property containing the reason for the pause.
   * @param function onPacket
   *        Hook to modify the packet before it is sent. Feel free to return a
   *        promise.
   */
  _pauseAndRespond: function (aFrame, aReason, onPacket=function (k) { return k; }) {
    try {
      let packet = this._paused(aFrame);
      if (!packet) {
        return undefined;
      }
      packet.why = aReason;

      let loc = this.sources.getFrameLocation(aFrame);
      this.sources.getOriginalLocation(loc).then(aOrigPosition => {
        if (!aOrigPosition.sourceActor) {
          // The only time the source actor will be null is if there
          // was a sourcemap and it tried to look up the original
          // location but there was no original URL. This is a strange
          // scenario so we simply don't pause.
          DevToolsUtils.reportException(
            'ThreadActor',
            new Error('Attempted to pause in a script with a sourcemap but ' +
                      'could not find original location.')
          );

          return undefined;
        }

        packet.frame.where = {
          source: aOrigPosition.sourceActor.form(),
          line: aOrigPosition.line,
          column: aOrigPosition.column
        };
        resolve(onPacket(packet))
          .then(null, error => {
            reportError(error);
            return {
              error: "unknownError",
              message: error.message + "\n" + error.stack
            };
          })
          .then(packet => {
            this.conn.send(packet);
          });
      });

      this._pushThreadPause();
    } catch(e) {
      reportError(e, "Got an exception during TA__pauseAndRespond: ");
    }

    // If the browser tab has been closed, terminate the debuggee script
    // instead of continuing. Executing JS after the content window is gone is
    // a bad idea.
    return this._tabClosed ? null : undefined;
  },

  /**
   * Handle resume requests that include a forceCompletion request.
   *
   * @param Object aRequest
   *        The request packet received over the RDP.
   * @returns A response packet.
   */
  _forceCompletion: function (aRequest) {
    // TODO: remove this when Debugger.Frame.prototype.pop is implemented in
    // bug 736733.
    return {
      error: "notImplemented",
      message: "forced completion is not yet implemented."
    };
  },

  _makeOnEnterFrame: function ({ pauseAndRespond }) {
    return aFrame => {
      const generatedLocation = this.sources.getFrameLocation(aFrame);
      let { sourceActor } = this.synchronize(this.sources.getOriginalLocation(
        generatedLocation));
      let url = sourceActor.url;

      return this.sources.isBlackBoxed(url)
        ? undefined
        : pauseAndRespond(aFrame);
    };
  },

  _makeOnPop: function ({ thread, pauseAndRespond, createValueGrip }) {
    return function (aCompletion) {
      // onPop is called with 'this' set to the current frame.

      const generatedLocation = thread.sources.getFrameLocation(this);
      const { sourceActor } = thread.synchronize(thread.sources.getOriginalLocation(
        generatedLocation));
      const url = sourceActor.url;

      if (thread.sources.isBlackBoxed(url)) {
        return undefined;
      }

      // Note that we're popping this frame; we need to watch for
      // subsequent step events on its caller.
      this.reportedPop = true;

      return pauseAndRespond(this, aPacket => {
        aPacket.why.frameFinished = {};
        if (!aCompletion) {
          aPacket.why.frameFinished.terminated = true;
        } else if (aCompletion.hasOwnProperty("return")) {
          aPacket.why.frameFinished.return = createValueGrip(aCompletion.return);
        } else if (aCompletion.hasOwnProperty("yield")) {
          aPacket.why.frameFinished.return = createValueGrip(aCompletion.yield);
        } else {
          aPacket.why.frameFinished.throw = createValueGrip(aCompletion.throw);
        }
        return aPacket;
      });
    };
  },

  _makeOnStep: function ({ thread, pauseAndRespond, startFrame,
                           startLocation, steppingType }) {
    // Breaking in place: we should always pause.
    if (steppingType === "break") {
      return function () {
        return pauseAndRespond(this);
      };
    }

    // Otherwise take what a "step" means into consideration.
    return function () {
      // onStep is called with 'this' set to the current frame.

      const generatedLocation = thread.sources.getFrameLocation(this);
      const newLocation = thread.synchronize(thread.sources.getOriginalLocation(
        generatedLocation));

      // Cases when we should pause because we have executed enough to consider
      // a "step" to have occured:
      //
      // 1.1. We change frames.
      // 1.2. We change URLs (can happen without changing frames thanks to
      //      source mapping).
      // 1.3. We change lines.
      //
      // Cases when we should always continue execution, even if one of the
      // above cases is true:
      //
      // 2.1. We are in a source mapped region, but inside a null mapping
      //      (doesn't correlate to any region of original source)
      // 2.2. The source we are in is black boxed.

      // Cases 2.1 and 2.2
      if (newLocation.url == null
          || thread.sources.isBlackBoxed(newLocation.url)) {
        return undefined;
      }

      // Cases 1.1, 1.2 and 1.3
      if (this !== startFrame
          || startLocation.url !== newLocation.url
          || startLocation.line !== newLocation.line) {
        return pauseAndRespond(this);
      }

      // Otherwise, let execution continue (we haven't executed enough code to
      // consider this a "step" yet).
      return undefined;
    };
  },

  /**
   * Define the JS hook functions for stepping.
   */
  _makeSteppingHooks: function (aStartLocation, steppingType) {
    // Bind these methods and state because some of the hooks are called
    // with 'this' set to the current frame. Rather than repeating the
    // binding in each _makeOnX method, just do it once here and pass it
    // in to each function.
    const steppingHookState = {
      pauseAndRespond: (aFrame, onPacket=(k)=>k) => {
        return this._pauseAndRespond(aFrame, { type: "resumeLimit" }, onPacket);
      },
      createValueGrip: this.createValueGrip.bind(this),
      thread: this,
      startFrame: this.youngestFrame,
      startLocation: aStartLocation,
      steppingType: steppingType
    };

    return {
      onEnterFrame: this._makeOnEnterFrame(steppingHookState),
      onPop: this._makeOnPop(steppingHookState),
      onStep: this._makeOnStep(steppingHookState)
    };
  },

  /**
   * Handle attaching the various stepping hooks we need to attach when we
   * receive a resume request with a resumeLimit property.
   *
   * @param Object aRequest
   *        The request packet received over the RDP.
   * @returns A promise that resolves to true once the hooks are attached, or is
   *          rejected with an error packet.
   */
  _handleResumeLimit: function (aRequest) {
    let steppingType = aRequest.resumeLimit.type;
    if (["break", "step", "next", "finish"].indexOf(steppingType) == -1) {
      return reject({ error: "badParameterType",
                      message: "Unknown resumeLimit type" });
    }

    const generatedLocation = this.sources.getFrameLocation(this.youngestFrame);
    return this.sources.getOriginalLocation(generatedLocation)
      .then(originalLocation => {
        const { onEnterFrame, onPop, onStep } = this._makeSteppingHooks(originalLocation,
                                                                        steppingType);

        // Make sure there is still a frame on the stack if we are to continue
        // stepping.
        let stepFrame = this._getNextStepFrame(this.youngestFrame);
        if (stepFrame) {
          switch (steppingType) {
            case "step":
              this.dbg.onEnterFrame = onEnterFrame;
              // Fall through.
            case "break":
            case "next":
              if (stepFrame.script) {
                  stepFrame.onStep = onStep;
              }
              stepFrame.onPop = onPop;
              break;
            case "finish":
              stepFrame.onPop = onPop;
          }
        }

        return true;
      });
  },

  /**
   * Clear the onStep and onPop hooks from the given frame and all of the frames
   * below it.
   *
   * @param Debugger.Frame aFrame
   *        The frame we want to clear the stepping hooks from.
   */
  _clearSteppingHooks: function (aFrame) {
    if (aFrame && aFrame.live) {
      while (aFrame) {
        aFrame.onStep = undefined;
        aFrame.onPop = undefined;
        aFrame = aFrame.older;
      }
    }
  },

  /**
   * Listen to the debuggee's DOM events if we received a request to do so.
   *
   * @param Object aRequest
   *        The resume request packet received over the RDP.
   */
  _maybeListenToEvents: function (aRequest) {
    // Break-on-DOMEvents is only supported in content debugging.
    let events = aRequest.pauseOnDOMEvents;
    if (this.global && events &&
        (events == "*" ||
        (Array.isArray(events) && events.length))) {
      this._pauseOnDOMEvents = events;
      let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);
      els.addListenerForAllEvents(this.global, this._allEventsListener, true);
    }
  },

  /**
   * Handle a protocol request to resume execution of the debuggee.
   */
  onResume: function (aRequest) {
    if (this._state !== "paused") {
      return {
        error: "wrongState",
        message: "Can't resume when debuggee isn't paused. Current state is '"
          + this._state + "'"
      };
    }

    // In case of multiple nested event loops (due to multiple debuggers open in
    // different tabs or multiple debugger clients connected to the same tab)
    // only allow resumption in a LIFO order.
    if (this._nestedEventLoops.size && this._nestedEventLoops.lastPausedUrl
        && (this._nestedEventLoops.lastPausedUrl !== this._parent.url
            || this._nestedEventLoops.lastConnection !== this.conn)) {
      return {
        error: "wrongOrder",
        message: "trying to resume in the wrong order.",
        lastPausedUrl: this._nestedEventLoops.lastPausedUrl
      };
    }

    if (aRequest && aRequest.forceCompletion) {
      return this._forceCompletion(aRequest);
    }

    let resumeLimitHandled;
    if (aRequest && aRequest.resumeLimit) {
      resumeLimitHandled = this._handleResumeLimit(aRequest)
    } else {
      this._clearSteppingHooks(this.youngestFrame);
      resumeLimitHandled = resolve(true);
    }

    return resumeLimitHandled.then(() => {
      if (aRequest) {
        this._options.pauseOnExceptions = aRequest.pauseOnExceptions;
        this._options.ignoreCaughtExceptions = aRequest.ignoreCaughtExceptions;
        this.maybePauseOnExceptions();
        this._maybeListenToEvents(aRequest);
      }

      let packet = this._resumed();
      this._popThreadPause();
      // Tell anyone who cares of the resume (as of now, that's the xpcshell
      // harness)
      if (Services.obs) {
        Services.obs.notifyObservers(this, "devtools-thread-resumed", null);
      }
      return packet;
    }, error => {
      return error instanceof Error
        ? { error: "unknownError",
            message: DevToolsUtils.safeErrorString(error) }
        // It is a known error, and the promise was rejected with an error
        // packet.
        : error;
    });
  },

  /**
   * Spin up a nested event loop so we can synchronously resolve a promise.
   *
   * @param aPromise
   *        The promise we want to resolve.
   * @returns The promise's resolution.
   */
  synchronize: function(aPromise) {
    let needNest = true;
    let eventLoop;
    let returnVal;

    aPromise
      .then((aResolvedVal) => {
        needNest = false;
        returnVal = aResolvedVal;
      })
      .then(null, (aError) => {
        reportError(aError, "Error inside synchronize:");
      })
      .then(() => {
        if (eventLoop) {
          eventLoop.resolve();
        }
      });

    if (needNest) {
      eventLoop = this._nestedEventLoops.push();
      eventLoop.enter();
    }

    return returnVal;
  },

  /**
   * Set the debugging hook to pause on exceptions if configured to do so.
   */
  maybePauseOnExceptions: function() {
    if (this._options.pauseOnExceptions) {
      this.dbg.onExceptionUnwind = this.onExceptionUnwind.bind(this);
    }
  },

  /**
   * A listener that gets called for every event fired on the page, when a list
   * of interesting events was provided with the pauseOnDOMEvents property. It
   * is used to set server-managed breakpoints on any existing event listeners
   * for those events.
   *
   * @param Event event
   *        The event that was fired.
   */
  _allEventsListener: function(event) {
    if (this._pauseOnDOMEvents == "*" ||
        this._pauseOnDOMEvents.indexOf(event.type) != -1) {
      for (let listener of this._getAllEventListeners(event.target)) {
        if (event.type == listener.type || this._pauseOnDOMEvents == "*") {
          this._breakOnEnter(listener.script);
        }
      }
    }
  },

  /**
   * Return an array containing all the event listeners attached to the
   * specified event target and its ancestors in the event target chain.
   *
   * @param EventTarget eventTarget
   *        The target the event was dispatched on.
   * @returns Array
   */
  _getAllEventListeners: function(eventTarget) {
    let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);

    let targets = els.getEventTargetChainFor(eventTarget);
    let listeners = [];

    for (let target of targets) {
      let handlers = els.getListenerInfoFor(target);
      for (let handler of handlers) {
        // Null is returned for all-events handlers, and native event listeners
        // don't provide any listenerObject, which makes them not that useful to
        // a JS debugger.
        if (!handler || !handler.listenerObject || !handler.type)
          continue;
        // Create a listener-like object suitable for our purposes.
        let l = Object.create(null);
        l.type = handler.type;
        let listener = handler.listenerObject;
        let listenerDO = this.globalDebugObject.makeDebuggeeValue(listener);
        // If the listener is an object with a 'handleEvent' method, use that.
        if (listenerDO.class == "Object" || listenerDO.class == "XULElement") {
          // For some events we don't have permission to access the
          // 'handleEvent' property when running in content scope.
          if (!listenerDO.unwrap()) {
            continue;
          }
          let heDesc;
          while (!heDesc && listenerDO) {
            heDesc = listenerDO.getOwnPropertyDescriptor("handleEvent");
            listenerDO = listenerDO.proto;
          }
          if (heDesc && heDesc.value) {
            listenerDO = heDesc.value;
          }
        }
        // When the listener is a bound function, we are actually interested in
        // the target function.
        while (listenerDO.isBoundFunction) {
          listenerDO = listenerDO.boundTargetFunction;
        }
        l.script = listenerDO.script;
        // Chrome listeners won't be converted to debuggee values, since their
        // compartment is not added as a debuggee.
        if (!l.script)
          continue;
        listeners.push(l);
      }
    }
    return listeners;
  },

  /**
   * Set a breakpoint on the first line of the given script that has an entry
   * point.
   */
  _breakOnEnter: function(script) {
    let offsets = script.getAllOffsets();
    for (let line = 0, n = offsets.length; line < n; line++) {
      if (offsets[line]) {
        // N.B. Hidden breakpoints do not have an original location, and are not
        // stored in the breakpoint actor map.
        let actor = new BreakpointActor(this);
        this.threadLifetimePool.addActor(actor);
        let scripts = this.scripts.getScriptsBySourceAndLine(script.source, line);
        let entryPoints = findEntryPointsForLine(scripts, line);
        setBreakpointOnEntryPoints(this, actor, entryPoints);
        this._hiddenBreakpoints.set(actor.actorID, actor);
        break;
      }
    }
  },

  /**
   * Helper method that returns the next frame when stepping.
   */
  _getNextStepFrame: function (aFrame) {
    let stepFrame = aFrame.reportedPop ? aFrame.older : aFrame;
    if (!stepFrame || !stepFrame.script) {
      stepFrame = null;
    }
    return stepFrame;
  },

  onClientEvaluate: function (aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Debuggee must be paused to evaluate code." };
    }

    let frame = this._requestFrame(aRequest.frame);
    if (!frame) {
      return { error: "unknownFrame",
               message: "Evaluation frame not found" };
    }

    if (!frame.environment) {
      return { error: "notDebuggee",
               message: "cannot access the environment of this frame." };
    }

    let youngest = this.youngestFrame;

    // Put ourselves back in the running state and inform the client.
    let resumedPacket = this._resumed();
    this.conn.send(resumedPacket);

    // Run the expression.
    // XXX: test syntax errors
    let completion = frame.eval(aRequest.expression);

    // Put ourselves back in the pause state.
    let packet = this._paused(youngest);
    packet.why = { type: "clientEvaluated",
                   frameFinished: this.createProtocolCompletionValue(completion) };

    // Return back to our previous pause's event loop.
    return packet;
  },

  onFrames: function (aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Stack frames are only available while the debuggee is paused."};
    }

    let start = aRequest.start ? aRequest.start : 0;
    let count = aRequest.count;

    // Find the starting frame...
    let frame = this.youngestFrame;
    let i = 0;
    while (frame && (i < start)) {
      frame = frame.older;
      i++;
    }

    // Return request.count frames, or all remaining
    // frames if count is not defined.
    let frames = [];
    let promises = [];
    for (; frame && (!count || i < (start + count)); i++, frame=frame.older) {
      let form = this._createFrameActor(frame).form();
      form.depth = i;
      frames.push(form);

      let promise = this.sources.getOriginalLocation({
        sourceActor: this.sources.createNonSourceMappedActor(frame.script.source),
        line: form.where.line,
        column: form.where.column
      }).then((aOrigLocation) => {
        let sourceForm = aOrigLocation.sourceActor.form();
        form.where = {
          source: sourceForm,
          line: aOrigLocation.line,
          column: aOrigLocation.column
        };
        form.source = sourceForm;
      });
      promises.push(promise);
    }

    return all(promises).then(function () {
      return { frames: frames };
    });
  },

  onReleaseMany: function (aRequest) {
    if (!aRequest.actors) {
      return { error: "missingParameter",
               message: "no actors were specified" };
    }

    let res;
    for each (let actorID in aRequest.actors) {
      let actor = this.threadLifetimePool.get(actorID);
      if (!actor) {
        if (!res) {
          res = { error: "notReleasable",
                  message: "Only thread-lifetime actors can be released." };
        }
        continue;
      }
      actor.onRelease();
    }
    return res ? res : {};
  },

  /**
   * Get the script and source lists from the debugger.
   */
  _discoverSources: function () {
    // Only get one script per Debugger.Source.
    const sourcesToScripts = new Map();
    const scripts = this.scripts.getAllScripts();
    for (let i = 0, len = scripts.length; i < len; i++) {
      let s = scripts[i];
      if (s.source) {
        sourcesToScripts.set(s.source, s);
      }
    }

    return all([this.sources.createSourceActors(script.source)
                for (script of sourcesToScripts.values())]);
  },

  onSources: function (aRequest) {
    return this._discoverSources().then(() => {
      return {
        sources: [s.form() for (s of this.sources.iter())]
      };
    });
  },

  /**
   * Disassociate all breakpoint actors from their scripts and clear the
   * breakpoint handlers. This method can be used when the thread actor intends
   * to keep the breakpoint store, but needs to clear any actual breakpoints,
   * e.g. due to a page navigation. This way the breakpoint actors' script
   * caches won't hold on to the Debugger.Script objects leaking memory.
   */
  disableAllBreakpoints: function () {
    for (let bpActor of this.breakpointActorMap.findActors()) {
      bpActor.removeScripts();
    }
  },


  /**
   * Handle a protocol request to pause the debuggee.
   */
  onInterrupt: function (aRequest) {
    if (this.state == "exited") {
      return { type: "exited" };
    } else if (this.state == "paused") {
      // TODO: return the actual reason for the existing pause.
      return { type: "paused", why: { type: "alreadyPaused" } };
    } else if (this.state != "running") {
      return { error: "wrongState",
               message: "Received interrupt request in " + this.state +
                        " state." };
    }

    try {
      // Put ourselves in the paused state.
      let packet = this._paused();
      if (!packet) {
        return { error: "notInterrupted" };
      }
      packet.why = { type: "interrupted" };

      // Send the response to the interrupt request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send(packet);

      // Start a nested event loop.
      this._pushThreadPause();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch (e) {
      reportError(e);
      return { error: "notInterrupted", message: e.toString() };
    }
  },

  /**
   * Handle a protocol request to retrieve all the event listeners on the page.
   */
  onEventListeners: function (aRequest) {
    // This request is only supported in content debugging.
    if (!this.global) {
      return {
        error: "notImplemented",
        message: "eventListeners request is only supported in content debugging"
      };
    }

    let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);

    let nodes = this.global.document.getElementsByTagName("*");
    nodes = [this.global].concat([].slice.call(nodes));
    let listeners = [];

    for (let node of nodes) {
      let handlers = els.getListenerInfoFor(node);

      for (let handler of handlers) {
        // Create a form object for serializing the listener via the protocol.
        let listenerForm = Object.create(null);
        let listener = handler.listenerObject;
        // Native event listeners don't provide any listenerObject or type and
        // are not that useful to a JS debugger.
        if (!listener || !handler.type) {
          continue;
        }

        // There will be no tagName if the event listener is set on the window.
        let selector = node.tagName ? CssLogic.findCssSelector(node) : "window";
        let nodeDO = this.globalDebugObject.makeDebuggeeValue(node);
        listenerForm.node = {
          selector: selector,
          object: this.createValueGrip(nodeDO)
        };
        listenerForm.type = handler.type;
        listenerForm.capturing = handler.capturing;
        listenerForm.allowsUntrusted = handler.allowsUntrusted;
        listenerForm.inSystemEventGroup = handler.inSystemEventGroup;
        let handlerName = "on" + listenerForm.type;
        listenerForm.isEventHandler = false;
        if (typeof node.hasAttribute !== "undefined") {
          listenerForm.isEventHandler = !!node.hasAttribute(handlerName);
        }
        if (!!node[handlerName]) {
          listenerForm.isEventHandler = !!node[handlerName];
        }
        // Get the Debugger.Object for the listener object.
        let listenerDO = this.globalDebugObject.makeDebuggeeValue(listener);
        // If the listener is an object with a 'handleEvent' method, use that.
        if (listenerDO.class == "Object" || listenerDO.class == "XULElement") {
          // For some events we don't have permission to access the
          // 'handleEvent' property when running in content scope.
          if (!listenerDO.unwrap()) {
            continue;
          }
          let heDesc;
          while (!heDesc && listenerDO) {
            heDesc = listenerDO.getOwnPropertyDescriptor("handleEvent");
            listenerDO = listenerDO.proto;
          }
          if (heDesc && heDesc.value) {
            listenerDO = heDesc.value;
          }
        }
        // When the listener is a bound function, we are actually interested in
        // the target function.
        while (listenerDO.isBoundFunction) {
          listenerDO = listenerDO.boundTargetFunction;
        }
        listenerForm.function = this.createValueGrip(listenerDO);
        listeners.push(listenerForm);
      }
    }
    return { listeners: listeners };
  },

  /**
   * Return the Debug.Frame for a frame mentioned by the protocol.
   */
  _requestFrame: function (aFrameID) {
    if (!aFrameID) {
      return this.youngestFrame;
    }

    if (this._framePool.has(aFrameID)) {
      return this._framePool.get(aFrameID).frame;
    }

    return undefined;
  },

  _paused: function (aFrame) {
    // We don't handle nested pauses correctly.  Don't try - if we're
    // paused, just continue running whatever code triggered the pause.
    // We don't want to actually have nested pauses (although we
    // have nested event loops).  If code runs in the debuggee during
    // a pause, it should cause the actor to resume (dropping
    // pause-lifetime actors etc) and then repause when complete.

    if (this.state === "paused") {
      return undefined;
    }

    // Clear stepping hooks.
    this.dbg.onEnterFrame = undefined;
    this.dbg.onExceptionUnwind = undefined;
    if (aFrame) {
      aFrame.onStep = undefined;
      aFrame.onPop = undefined;
    }

    // Clear DOM event breakpoints.
    // XPCShell tests don't use actual DOM windows for globals and cause
    // removeListenerForAllEvents to throw.
    if (this.global && !this.global.toString().contains("Sandbox")) {
      let els = Cc["@mozilla.org/eventlistenerservice;1"]
                .getService(Ci.nsIEventListenerService);
      els.removeListenerForAllEvents(this.global, this._allEventsListener, true);
      for (let [,bp] of this._hiddenBreakpoints) {
        bp.onDelete();
      }
      this._hiddenBreakpoints.clear();
    }

    this._state = "paused";

    // Create the actor pool that will hold the pause actor and its
    // children.
    dbg_assert(!this._pausePool, "No pause pool should exist yet");
    this._pausePool = new ActorPool(this.conn);
    this.conn.addActorPool(this._pausePool);

    // Give children of the pause pool a quick link back to the
    // thread...
    this._pausePool.threadActor = this;

    // Create the pause actor itself...
    dbg_assert(!this._pauseActor, "No pause actor should exist yet");
    this._pauseActor = new PauseActor(this._pausePool);
    this._pausePool.addActor(this._pauseActor);

    // Update the list of frames.
    let poppedFrames = this._updateFrames();

    // Send off the paused packet and spin an event loop.
    let packet = { from: this.actorID,
                   type: "paused",
                   actor: this._pauseActor.actorID };
    if (aFrame) {
      packet.frame = this._createFrameActor(aFrame).form();
    }

    if (poppedFrames) {
      packet.poppedFrames = poppedFrames;
    }

    return packet;
  },

  _resumed: function () {
    this._state = "running";

    // Drop the actors in the pause actor pool.
    this.conn.removeActorPool(this._pausePool);

    this._pausePool = null;
    this._pauseActor = null;

    return { from: this.actorID, type: "resumed" };
  },

  /**
   * Expire frame actors for frames that have been popped.
   *
   * @returns A list of actor IDs whose frames have been popped.
   */
  _updateFrames: function () {
    let popped = [];

    // Create the actor pool that will hold the still-living frames.
    let framePool = new ActorPool(this.conn);
    let frameList = [];

    for each (let frameActor in this._frameActors) {
      if (frameActor.frame.live) {
        framePool.addActor(frameActor);
        frameList.push(frameActor);
      } else {
        popped.push(frameActor.actorID);
      }
    }

    // Remove the old frame actor pool, this will expire
    // any actors that weren't added to the new pool.
    if (this._framePool) {
      this.conn.removeActorPool(this._framePool);
    }

    this._frameActors = frameList;
    this._framePool = framePool;
    this.conn.addActorPool(framePool);

    return popped;
  },

  _createFrameActor: function (aFrame) {
    if (aFrame.actor) {
      return aFrame.actor;
    }

    let actor = new FrameActor(aFrame, this);
    this._frameActors.push(actor);
    this._framePool.addActor(actor);
    aFrame.actor = actor;

    return actor;
  },

  /**
   * Create and return an environment actor that corresponds to the provided
   * Debugger.Environment.
   * @param Debugger.Environment aEnvironment
   *        The lexical environment we want to extract.
   * @param object aPool
   *        The pool where the newly-created actor will be placed.
   * @return The EnvironmentActor for aEnvironment or undefined for host
   *         functions or functions scoped to a non-debuggee global.
   */
  createEnvironmentActor: function (aEnvironment, aPool) {
    if (!aEnvironment) {
      return undefined;
    }

    if (aEnvironment.actor) {
      return aEnvironment.actor;
    }

    let actor = new EnvironmentActor(aEnvironment, this);
    aPool.addActor(actor);
    aEnvironment.actor = actor;

    return actor;
  },

  /**
   * Create a grip for the given debuggee value.  If the value is an
   * object, will create an actor with the given lifetime.
   */
  createValueGrip: function (aValue, aPool=false) {
    if (!aPool) {
      aPool = this._pausePool;
    }

    switch (typeof aValue) {
      case "boolean":
        return aValue;

      case "string":
        if (this._stringIsLong(aValue)) {
          return this.longStringGrip(aValue, aPool);
        }
        return aValue;

      case "number":
        if (aValue === Infinity) {
          return { type: "Infinity" };
        } else if (aValue === -Infinity) {
          return { type: "-Infinity" };
        } else if (Number.isNaN(aValue)) {
          return { type: "NaN" };
        } else if (!aValue && 1 / aValue === -Infinity) {
          return { type: "-0" };
        }
        return aValue;

      case "undefined":
        return { type: "undefined" };

      case "object":
        if (aValue === null) {
          return { type: "null" };
        }
      else if(aValue.optimizedOut ||
              aValue.uninitialized ||
              aValue.missingArguments) {
          // The slot is optimized out, an uninitialized binding, or
          // arguments on a dead scope
          return {
            type: "null",
            optimizedOut: aValue.optimizedOut,
            uninitialized: aValue.uninitialized,
            missingArguments: aValue.missingArguments
          };
        }
        return this.objectGrip(aValue, aPool);

      case "symbol":
        let form = {
          type: "symbol"
        };
        let name = getSymbolName(aValue);
        if (name !== undefined) {
          form.name = this.createValueGrip(name);
        }
        return form;

      default:
        dbg_assert(false, "Failed to provide a grip for: " + aValue);
        return null;
    }
  },

  /**
   * Return a protocol completion value representing the given
   * Debugger-provided completion value.
   */
  createProtocolCompletionValue: function (aCompletion) {
    let protoValue = {};
    if (aCompletion == null) {
      protoValue.terminated = true;
    } else if ("return" in aCompletion) {
      protoValue.return = this.createValueGrip(aCompletion.return);
    } else if ("throw" in aCompletion) {
      protoValue.throw = this.createValueGrip(aCompletion.throw);
    } else {
      protoValue.return = this.createValueGrip(aCompletion.yield);
    }
    return protoValue;
  },

  /**
   * Create a grip for the given debuggee object.
   *
   * @param aValue Debugger.Object
   *        The debuggee object value.
   * @param aPool ActorPool
   *        The actor pool where the new object actor will be added.
   */
  objectGrip: function (aValue, aPool) {
    if (!aPool.objectActors) {
      aPool.objectActors = new WeakMap();
    }

    if (aPool.objectActors.has(aValue)) {
      return aPool.objectActors.get(aValue).grip();
    } else if (this.threadLifetimePool.objectActors.has(aValue)) {
      return this.threadLifetimePool.objectActors.get(aValue).grip();
    }

    let actor = new PauseScopedObjectActor(aValue, this);
    aPool.addActor(actor);
    aPool.objectActors.set(aValue, actor);
    return actor.grip();
  },

  /**
   * Create a grip for the given debuggee object with a pause lifetime.
   *
   * @param aValue Debugger.Object
   *        The debuggee object value.
   */
  pauseObjectGrip: function (aValue) {
    if (!this._pausePool) {
      throw "Object grip requested while not paused.";
    }

    return this.objectGrip(aValue, this._pausePool);
  },

  /**
   * Extend the lifetime of the provided object actor to thread lifetime.
   *
   * @param aActor object
   *        The object actor.
   */
  threadObjectGrip: function (aActor) {
    // We want to reuse the existing actor ID, so we just remove it from the
    // current pool's weak map and then let pool.addActor do the rest.
    aActor.registeredPool.objectActors.delete(aActor.obj);
    this.threadLifetimePool.addActor(aActor);
    this.threadLifetimePool.objectActors.set(aActor.obj, aActor);
  },

  /**
   * Handle a protocol request to promote multiple pause-lifetime grips to
   * thread-lifetime grips.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onThreadGrips: function (aRequest) {
    if (this.state != "paused") {
      return { error: "wrongState" };
    }

    if (!aRequest.actors) {
      return { error: "missingParameter",
               message: "no actors were specified" };
    }

    for (let actorID of aRequest.actors) {
      let actor = this._pausePool.get(actorID);
      if (actor) {
        this.threadObjectGrip(actor);
      }
    }
    return {};
  },

  /**
   * Create a grip for the given string.
   *
   * @param aString String
   *        The string we are creating a grip for.
   * @param aPool ActorPool
   *        The actor pool where the new actor will be added.
   */
  longStringGrip: function (aString, aPool) {
    if (!aPool.longStringActors) {
      aPool.longStringActors = {};
    }

    if (aPool.longStringActors.hasOwnProperty(aString)) {
      return aPool.longStringActors[aString].grip();
    }

    let actor = new LongStringActor(aString, this);
    aPool.addActor(actor);
    aPool.longStringActors[aString] = actor;
    return actor.grip();
  },

  /**
   * Create a long string grip that is scoped to a pause.
   *
   * @param aString String
   *        The string we are creating a grip for.
   */
  pauseLongStringGrip: function (aString) {
    return this.longStringGrip(aString, this._pausePool);
  },

  /**
   * Create a long string grip that is scoped to a thread.
   *
   * @param aString String
   *        The string we are creating a grip for.
   */
  threadLongStringGrip: function (aString) {
    return this.longStringGrip(aString, this._threadLifetimePool);
  },

  /**
   * Returns true if the string is long enough to use a LongStringActor instead
   * of passing the value directly over the protocol.
   *
   * @param aString String
   *        The string we are checking the length of.
   */
  _stringIsLong: function (aString) {
    return aString.length >= DebuggerServer.LONG_STRING_LENGTH;
  },

  // JS Debugger API hooks.

  /**
   * A function that the engine calls when a call to a debug event hook,
   * breakpoint handler, watchpoint handler, or similar function throws some
   * exception.
   *
   * @param aException exception
   *        The exception that was thrown in the debugger code.
   */
  uncaughtExceptionHook: function (aException) {
    dumpn("Got an exception: " + aException.message + "\n" + aException.stack);
  },

  /**
   * A function that the engine calls when a debugger statement has been
   * executed in the specified frame.
   *
   * @param aFrame Debugger.Frame
   *        The stack frame that contained the debugger statement.
   */
  onDebuggerStatement: function (aFrame) {
    // Don't pause if we are currently stepping (in or over) or the frame is
    // black-boxed.
    const generatedLocation = this.sources.getFrameLocation(aFrame);
    const { sourceActor } = this.synchronize(this.sources.getOriginalLocation(
      generatedLocation));
    const url = sourceActor ? sourceActor.url : null;

    return this.sources.isBlackBoxed(url) || aFrame.onStep
      ? undefined
      : this._pauseAndRespond(aFrame, { type: "debuggerStatement" });
  },

  /**
   * A function that the engine calls when an exception has been thrown and has
   * propagated to the specified frame.
   *
   * @param aFrame Debugger.Frame
   *        The youngest remaining stack frame.
   * @param aValue object
   *        The exception that was thrown.
   */
  onExceptionUnwind: function (aFrame, aValue) {
    let willBeCaught = false;
    for (let frame = aFrame; frame != null; frame = frame.older) {
      if (frame.script.isInCatchScope(frame.offset)) {
        willBeCaught = true;
        break;
      }
    }

    if (willBeCaught && this._options.ignoreCaughtExceptions) {
      return undefined;
    }

    const generatedLocation = this.sources.getFrameLocation(aFrame);
    const { sourceActor } = this.synchronize(this.sources.getOriginalLocation(
      generatedLocation));
    const url = sourceActor ? sourceActor.url : null;

    if (this.sources.isBlackBoxed(url)) {
      return undefined;
    }

    try {
      let packet = this._paused(aFrame);
      if (!packet) {
        return undefined;
      }

      packet.why = { type: "exception",
                     exception: this.createValueGrip(aValue) };
      this.conn.send(packet);

      this._pushThreadPause();
    } catch(e) {
      reportError(e, "Got an exception during TA_onExceptionUnwind: ");
    }

    return undefined;
  },

  /**
   * A function that the engine calls when a new script has been loaded into the
   * scope of the specified debuggee global.
   *
   * @param aScript Debugger.Script
   *        The source script that has been loaded into a debuggee compartment.
   * @param aGlobal Debugger.Object
   *        A Debugger.Object instance whose referent is the global object.
   */
  onNewScript: function (aScript, aGlobal) {
    // XXX: The scripts must be added to the ScriptStore before restoring
    // breakpoints in _addScript. If we try to add them to the ScriptStore
    // inside _addScript, we can accidentally set a breakpoint in a top level
    // script as a "closest match" because we wouldn't have added the child
    // scripts to the ScriptStore yet.
    this.scripts.addScripts(this.dbg.findScripts({ source: aScript.source }));

    this._addScript(aScript);

    // `onNewScript` is only fired for top-level scripts (AKA staticLevel == 0),
    // but top-level scripts have the wrong `lineCount` sometimes (bug 979094),
    // so iterate over the immediate children to activate breakpoints for now
    // (TODO bug 1124258: don't do this when `lineCount` bug is fixed)
    for (let s of aScript.getChildScripts()) {
      this._addScript(s);
    }
  },

  onNewSource: function (aSource) {
    this.conn.send({
      from: this.actorID,
      type: "newSource",
      source: aSource.form()
    });
  },

  /**
   * Check if scripts from the provided source URL are allowed to be stored in
   * the cache.
   *
   * @param aSourceUrl String
   *        The url of the script's source that will be stored.
   * @returns true, if the script can be added, false otherwise.
   */
  _allowSource: function (aSource) {
    return !isHiddenSource(aSource);
  },

  /**
   * Restore any pre-existing breakpoints to the scripts that we have access to.
   */
  _restoreBreakpoints: function () {
    if (this.breakpointActorMap.size === 0) {
      return;
    }

    for (let s of this.scripts.getAllScripts()) {
      this._addScript(s);
    }
  },

  /**
   * Add the provided script to the server cache.
   *
   * @param aScript Debugger.Script
   *        The source script that will be stored.
   * @returns true, if the script was added; false otherwise.
   */
  _addScript: function (aScript) {
    if (!this._allowSource(aScript.source)) {
      return false;
    }

    // Set any stored breakpoints.
    let endLine = aScript.startLine + aScript.lineCount - 1;
    let source = this.sources.createNonSourceMappedActor(aScript.source);
    for (let bpActor of this.breakpointActorMap.findActors({ sourceActor: source })) {
      // Limit the search to the line numbers contained in the new script.
      if (bpActor.generatedLocation.line >= aScript.startLine
          && bpActor.generatedLocation.line <= endLine) {
        source.setBreakpointForActor(bpActor);
      }
    }

    // Go ahead and establish the source actors for this script, which
    // fetches sourcemaps if available and sends onNewSource
    // notifications
    this.sources.createSourceActors(aScript.source);

    return true;
  },


  /**
   * Get prototypes and properties of multiple objects.
   */
  onPrototypesAndProperties: function (aRequest) {
    let result = {};
    for (let actorID of aRequest.actors) {
      // This code assumes that there are no lazily loaded actors returned
      // by this call.
      let actor = this.conn.getActor(actorID);
      if (!actor) {
        return { from: this.actorID,
                 error: "noSuchActor" };
      }
      let handler = actor.onPrototypeAndProperties;
      if (!handler) {
        return { from: this.actorID,
                 error: "unrecognizedPacketType",
                 message: ('Actor "' + actorID +
                           '" does not recognize the packet type ' +
                           '"prototypeAndProperties"') };
      }
      result[actorID] = handler.call(actor, {});
    }
    return { from: this.actorID,
             actors: result };
  }
};

ThreadActor.prototype.requestTypes = {
  "attach": ThreadActor.prototype.onAttach,
  "detach": ThreadActor.prototype.onDetach,
  "reconfigure": ThreadActor.prototype.onReconfigure,
  "resume": ThreadActor.prototype.onResume,
  "clientEvaluate": ThreadActor.prototype.onClientEvaluate,
  "frames": ThreadActor.prototype.onFrames,
  "interrupt": ThreadActor.prototype.onInterrupt,
  "eventListeners": ThreadActor.prototype.onEventListeners,
  "releaseMany": ThreadActor.prototype.onReleaseMany,
  "sources": ThreadActor.prototype.onSources,
  "threadGrips": ThreadActor.prototype.onThreadGrips,
  "prototypesAndProperties": ThreadActor.prototype.onPrototypesAndProperties
};

exports.ThreadActor = ThreadActor;

/**
 * Creates a PauseActor.
 *
 * PauseActors exist for the lifetime of a given debuggee pause.  Used to
 * scope pause-lifetime grips.
 *
 * @param ActorPool aPool
 *        The actor pool created for this pause.
 */
function PauseActor(aPool)
{
  this.pool = aPool;
}

PauseActor.prototype = {
  actorPrefix: "pause"
};


/**
 * A base actor for any actors that should only respond receive messages in the
 * paused state. Subclasses may expose a `threadActor` which is used to help
 * determine when we are in a paused state. Subclasses should set their own
 * "constructor" property if they want better error messages. You should never
 * instantiate a PauseScopedActor directly, only through subclasses.
 */
function PauseScopedActor()
{
}

/**
 * A function decorator for creating methods to handle protocol messages that
 * should only be received while in the paused state.
 *
 * @param aMethod Function
 *        The function we are decorating.
 */
PauseScopedActor.withPaused = function (aMethod) {
  return function () {
    if (this.isPaused()) {
      return aMethod.apply(this, arguments);
    } else {
      return this._wrongState();
    }
  };
};

PauseScopedActor.prototype = {

  /**
   * Returns true if we are in the paused state.
   */
  isPaused: function () {
    // When there is not a ThreadActor available (like in the webconsole) we
    // have to be optimistic and assume that we are paused so that we can
    // respond to requests.
    return this.threadActor ? this.threadActor.state === "paused" : true;
  },

  /**
   * Returns the wrongState response packet for this actor.
   */
  _wrongState: function () {
    return {
      error: "wrongState",
      message: this.constructor.name +
        " actors can only be accessed while the thread is paused."
    };
  }
};

/**
 * Resolve a URI back to physical file.
 *
 * Of course, this works only for URIs pointing to local resources.
 *
 * @param  aURI
 *         URI to resolve
 * @return
 *         resolved nsIURI
 */
function resolveURIToLocalPath(aURI) {
  let resolved;
  switch (aURI.scheme) {
    case "jar":
    case "file":
      return aURI;

    case "chrome":
      resolved = Cc["@mozilla.org/chrome/chrome-registry;1"].
                 getService(Ci.nsIChromeRegistry).convertChromeURL(aURI);
      return resolveURIToLocalPath(resolved);

    case "resource":
      resolved = Cc["@mozilla.org/network/protocol;1?name=resource"].
                 getService(Ci.nsIResProtocolHandler).resolveURI(aURI);
      aURI = Services.io.newURI(resolved, null, null);
      return resolveURIToLocalPath(aURI);

    default:
      return null;
  }
}

/**
 * A SourceActor provides information about the source of a script. There
 * are two kinds of source actors: ones that represent real source objects,
 * and ones that represent non-existant "original" sources when the real
 * sources are sourcemapped. When a source is sourcemapped, actors are
 * created for both the "generated" and "original" sources, and the client will
 * only see the original sources. We separate these because there isn't
 * a 1:1 mapping of generated to original sources; one generated source
 * may represent N original sources, so we need to create N + 1 separate
 * actors.
 *
 * There are 4 different scenarios for sources that you should
 * understand:
 *
 * - A single non-sourcemapped source that is not inlined in HTML
 *   (separate JS file, eval'ed code, etc)
 * - A single sourcemapped source which creates N original sources
 * - An HTML page with multiple inline scripts, which are distinct
 *   sources, but should be represented as a single source
 * - A pretty-printed source (which may or may not be an original
 *   sourcemapped source), which generates a sourcemap for itself
 *
 * The complexity of `SourceActor` and `ThreadSources` are to handle
 * all of thise cases and hopefully internalize the complexities.
 *
 * @param Debugger.Source source
 *        The source object we are representing.
 * @param ThreadActor thread
 *        The current thread actor.
 * @param String originalUrl
 *        Optional. For sourcemapped urls, the original url this is representing.
 * @param Debugger.Source generatedSource
 *        Optional, passed in when aSourceMap is also passed in. The generated
 *        source object that introduced this source.
 * @param String contentType
 *        Optional. The content type of this source, if immediately available.
 */
function SourceActor({ source, thread, originalUrl, generatedSource,
                       contentType }) {
  this._threadActor = thread;
  this._originalUrl = originalUrl;
  this._source = source;
  this._generatedSource = generatedSource;
  this._contentType = contentType;

  this.onSource = this.onSource.bind(this);
  this._invertSourceMap = this._invertSourceMap.bind(this);
  this._encodeAndSetSourceMapURL = this._encodeAndSetSourceMapURL.bind(this);
  this._getSourceText = this._getSourceText.bind(this);

  this._mapSourceToAddon();

  if (this.threadActor.sources.isPrettyPrinted(this.url)) {
    this._init = this.onPrettyPrint({
      indent: this.threadActor.sources.prettyPrintIndent(this.url)
    }).then(null, error => {
      DevToolsUtils.reportException("SourceActor", error);
    });
  } else {
    this._init = null;
  }
}

SourceActor.prototype = {
  constructor: SourceActor,
  actorPrefix: "source",

  _oldSourceMap: null,
  _init: null,
  _addonID: null,
  _addonPath: null,

  get threadActor() { return this._threadActor; },
  get dbg() { return this.threadActor.dbg; },
  get scripts() { return this.threadActor.scripts; },
  get source() { return this._source; },
  get generatedSource() { return this._generatedSource; },
  get breakpointActorMap() { return this.threadActor.breakpointActorMap; },
  get url() {
    if (this.source) {
      return getSourceURL(this.source);
    }
    return this._originalUrl;
  },
  get addonID() { return this._addonID; },
  get addonPath() { return this._addonPath; },

  get prettyPrintWorker() {
    return this.threadActor.prettyPrintWorker;
  },

  form: function () {
    let source = this.source || this.generatedSource;
    // This might not have a source or a generatedSource because we
    // treat HTML pages with inline scripts as a special SourceActor
    // that doesn't have either
    let introductionUrl = null;
    if (source && source.introductionScript) {
      introductionUrl = source.introductionScript.source.url;
    }

    return {
      actor: this.actorID,
      url: this.url ? this.url.split(" -> ").pop() : null,
      addonID: this._addonID,
      addonPath: this._addonPath,
      isBlackBoxed: this.threadActor.sources.isBlackBoxed(this.url),
      isPrettyPrinted: this.threadActor.sources.isPrettyPrinted(this.url),
      introductionUrl: introductionUrl ? introductionUrl.split(" -> ").pop() : null,
      introductionType: source ? source.introductionType : null
    };
  },

  disconnect: function () {
    if (this.registeredPool && this.registeredPool.sourceActors) {
      delete this.registeredPool.sourceActors[this.actorID];
    }
  },

  _mapSourceToAddon: function() {
    try {
      var nsuri = Services.io.newURI(this.url.split(" -> ").pop(), null, null);
    }
    catch (e) {
      // We can't do anything with an invalid URI
      return;
    }

    let localURI = resolveURIToLocalPath(nsuri);

    let id = {};
    if (localURI && mapURIToAddonID(localURI, id)) {
      this._addonID = id.value;

      if (localURI instanceof Ci.nsIJARURI) {
        // The path in the add-on is easy for jar: uris
        this._addonPath = localURI.JAREntry;
      }
      else if (localURI instanceof Ci.nsIFileURL) {
        // For file: uris walk up to find the last directory that is part of the
        // add-on
        let target = localURI.file;
        let path = target.leafName;

        // We can assume that the directory containing the source file is part
        // of the add-on
        let root = target.parent;
        let file = root.parent;
        while (file && mapURIToAddonID(Services.io.newFileURI(file), {})) {
          path = root.leafName + "/" + path;
          root = file;
          file = file.parent;
        }

        if (!file) {
          const error = new Error("Could not find the root of the add-on for " + this.url);
          DevToolsUtils.reportException("SourceActor.prototype._mapSourceToAddon", error)
          return;
        }

        this._addonPath = path;
      }
    }
  },

  _getSourceText: function () {
    let toResolvedContent = t => ({
      content: t,
      contentType: this._contentType
    });

    let genSource = this.generatedSource || this.source;
    return this.threadActor.sources.fetchSourceMap(genSource).then(map => {
      let sc;
      if (map && (sc = map.sourceContentFor(this.url))) {
        return toResolvedContent(sc);
      }

      // Use `source.text` if it exists, is not the "no source"
      // string, and the content type of the source is JavaScript. It
      // will be "no source" if the Debugger API wasn't able to load
      // the source because sources were discarded
      // (javascript.options.discardSystemSource == true). Re-fetch
      // non-JS sources to get the contentType from the headers.
      if (this.source &&
          this.source.text !== "[no source]" &&
          this._contentType &&
          this._contentType.indexOf('javascript') !== -1) {
        return toResolvedContent(this.source.text);
      }
      else {
        // XXX bug 865252: Don't load from the cache if this is a source mapped
        // source because we can't guarantee that the cache has the most up to date
        // content for this source like we can if it isn't source mapped.
        let sourceFetched = fetch(this.url, { loadFromCache: !this.source });

        // Record the contentType we just learned during fetching
        return sourceFetched.then(result => {
          this._contentType = result.contentType;
          return result;
        });
      }
    });
  },

  /**
   * Get all executable lines from the current source
   * @return Array - Executable lines of the current script
   **/
  getExecutableLines: function () {
    // Check if the original source is source mapped
    let packet = {
      from: this.actorID
    };

    function sortLines(lines) {
      // Converting the Set into an array
      lines = [line for (line of lines)];
      lines.sort((a, b) => {
        return a - b;
      });
      return lines;
    }

    if (this.generatedSource) {
      return this.threadActor.sources.getSourceMap(this.generatedSource).then(sm => {
        let lines = new Set();

        // Position of executable lines in the generated source
        let offsets = this.getExecutableOffsets(this.generatedSource, false);
        for (let offset of offsets) {
          let {line, source: sourceUrl} = sm.originalPositionFor({
            line: offset.lineNumber,
            column: offset.columnNumber
          });

          if (sourceUrl === this.url) {
            lines.add(line);
          }
        }

        packet.lines = sortLines(lines);
        return packet;
      });
    }

    let lines = this.getExecutableOffsets(this.source, true);
    packet.lines = sortLines(lines);
    return packet;
  },

  /**
   * Extract all executable offsets from the given script
   * @param String url - extract offsets of the script with this url
   * @param Boolean onlyLine - will return only the line number
   * @return Set - Executable offsets/lines of the script
   **/
  getExecutableOffsets: function  (source, onlyLine) {
    let offsets = new Set();
    for (let s of this.threadActor.scripts.getScriptsBySource(source)) {
      for (let offset of s.getAllColumnOffsets()) {
        offsets.add(onlyLine ? offset.lineNumber : offset);
      }
    }

    return offsets;
  },

  /**
   * Handler for the "source" packet.
   */
  onSource: function () {
    return resolve(this._init)
      .then(this._getSourceText)
      .then(({ content, contentType }) => {
        return {
          from: this.actorID,
          source: this.threadActor.createValueGrip(
            content, this.threadActor.threadLifetimePool),
          contentType: contentType
        };
      })
      .then(null, aError => {
        reportError(aError, "Got an exception during SA_onSource: ");
        return {
          "from": this.actorID,
          "error": "loadSourceError",
          "message": "Could not load the source for " + this.url + ".\n"
            + DevToolsUtils.safeErrorString(aError)
        };
      });
  },

  /**
   * Handler for the "prettyPrint" packet.
   */
  onPrettyPrint: function ({ indent }) {
    this.threadActor.sources.prettyPrint(this.url, indent);
    return this._getSourceText()
      .then(this._sendToPrettyPrintWorker(indent))
      .then(this._invertSourceMap)
      .then(this._encodeAndSetSourceMapURL)
      .then(() => {
        // We need to reset `_init` now because we have already done the work of
        // pretty printing, and don't want onSource to wait forever for
        // initialization to complete.
        this._init = null;
      })
      .then(this.onSource)
      .then(null, error => {
        this.onDisablePrettyPrint();
        return {
          from: this.actorID,
          error: "prettyPrintError",
          message: DevToolsUtils.safeErrorString(error)
        };
      });
  },

  /**
   * Return a function that sends a request to the pretty print worker, waits on
   * the worker's response, and then returns the pretty printed code.
   *
   * @param Number aIndent
   *        The number of spaces to indent by the code by, when we send the
   *        request to the pretty print worker.
   * @returns Function
   *          Returns a function which takes an AST, and returns a promise that
   *          is resolved with `{ code, mappings }` where `code` is the pretty
   *          printed code, and `mappings` is an array of source mappings.
   */
  _sendToPrettyPrintWorker: function (aIndent) {
    return ({ content }) => {
      const deferred = promise.defer();
      const id = Math.random();

      const onReply = ({ data }) => {
        if (data.id !== id) {
          return;
        }
        this.prettyPrintWorker.removeEventListener("message", onReply, false);

        if (data.error) {
          deferred.reject(new Error(data.error));
        } else {
          deferred.resolve(data);
        }
      };

      this.prettyPrintWorker.addEventListener("message", onReply, false);
      this.prettyPrintWorker.postMessage({
        id: id,
        url: this.url,
        indent: aIndent,
        source: content
      });

      return deferred.promise;
    };
  },

  /**
   * Invert a source map. So if a source map maps from a to b, return a new
   * source map from b to a. We need to do this because the source map we get
   * from _generatePrettyCodeAndMap goes the opposite way we want it to for
   * debugging.
   *
   * Note that the source map is modified in place.
   */
  _invertSourceMap: function ({ code, mappings }) {
    const generator = new SourceMapGenerator({ file: this.url });
    return DevToolsUtils.yieldingEach(mappings, m => {
      let mapping = {
        generated: {
          line: m.generatedLine,
          column: m.generatedColumn
        }
      };
      if (m.source) {
        mapping.source = m.source;
        mapping.original = {
          line: m.originalLine,
          column: m.originalColumn
        };
        mapping.name = m.name;
      }
      generator.addMapping(mapping);
    }).then(() => {
      generator.setSourceContent(this.url, code);
      const consumer = SourceMapConsumer.fromSourceMap(generator);

      // XXX bug 918802: Monkey punch the source map consumer, because iterating
      // over all mappings and inverting each of them, and then creating a new
      // SourceMapConsumer is slow.

      const getOrigPos = consumer.originalPositionFor.bind(consumer);
      const getGenPos = consumer.generatedPositionFor.bind(consumer);

      consumer.originalPositionFor = ({ line, column }) => {
        const location = getGenPos({
          line: line,
          column: column,
          source: this.url
        });
        location.source = this.url;
        return location;
      };

      consumer.generatedPositionFor = ({ line, column }) => getOrigPos({
        line: line,
        column: column
      });

      return {
        code: code,
        map: consumer
      };
    });
  },

  /**
   * Save the source map back to our thread's ThreadSources object so that
   * stepping, breakpoints, debugger statements, etc can use it. If we are
   * pretty printing a source mapped source, we need to compose the existing
   * source map with our new one.
   */
  _encodeAndSetSourceMapURL: function  ({ map: sm }) {
    let source = this.generatedSource || this.source;
    let sources = this.threadActor.sources;

    return sources.getSourceMap(source).then(prevMap => {
      if (prevMap) {
        // Compose the source maps
        this._oldSourceMapping = {
          url: source.sourceMapURL,
          map: prevMap
        };

        prevMap = SourceMapGenerator.fromSourceMap(prevMap);
        prevMap.applySourceMap(sm, this.url);
        sm = SourceMapConsumer.fromSourceMap(prevMap);
      }

      let sources = this.threadActor.sources;
      sources.clearSourceMapCache(source.sourceMapURL);
      sources.setSourceMapHard(source, null, sm);
    });
  },

  /**
   * Handler for the "disablePrettyPrint" packet.
   */
  onDisablePrettyPrint: function () {
    let source = this.generatedSource || this.source;
    let sources = this.threadActor.sources;
    let sm = sources.getSourceMap(source);

    sources.clearSourceMapCache(source.sourceMapURL, { hard: true });

    if (this._oldSourceMapping) {
      sources.setSourceMapHard(source,
                               this._oldSourceMapping.url,
                               this._oldSourceMapping.map);
       this._oldSourceMapping = null;
    }

    this.threadActor.sources.disablePrettyPrint(this.url);
    return this.onSource();
  },

  /**
   * Handler for the "blackbox" packet.
   */
  onBlackBox: function (aRequest) {
    this.threadActor.sources.blackBox(this.url);
    let packet = {
      from: this.actorID
    };
    if (this.threadActor.state == "paused"
        && this.threadActor.youngestFrame
        && this.threadActor.youngestFrame.script.url == this.url) {
      packet.pausedInSource = true;
    }
    return packet;
  },

  /**
   * Handler for the "unblackbox" packet.
   */
  onUnblackBox: function (aRequest) {
    this.threadActor.sources.unblackBox(this.url);
    return {
      from: this.actorID
    };
  },

  /**
   * Handle a protocol request to set a breakpoint.
   */
  onSetBreakpoint: function(aRequest) {
    if (this.threadActor.state !== "paused") {
      return { error: "wrongState",
               message: "Breakpoints can only be set while the debuggee is paused."};
    }

    return this.setBreakpoint(aRequest.location.line, aRequest.location.column,
                              aRequest.condition);
  },

  /** Get or create the BreakpointActor for the breakpoint at the given location.
   *
   * NB: This will override a pre-existing BreakpointActor's condition with
   * the given the location's condition.
   *
   * @param Object originalLocation
   *        The original location of the breakpoint.
   * @param Object generatedLocation
   *        The generated location of the breakpoint.
   * @returns BreakpointActor
   */
  _getOrCreateBreakpointActor: function (originalLocation, generatedLocation,
                                         condition)
  {
    let actor = this.breakpointActorMap.getActor(generatedLocation);
    if (!actor) {
      actor = new BreakpointActor(this.threadActor, originalLocation,
                                  generatedLocation, condition);
      this.threadActor.threadLifetimePool.addActor(actor);
      this.breakpointActorMap.setActor(generatedLocation, actor);
      return actor;
    }

    actor.condition = condition;
    return actor;
  },

  /**
   * Set breakpoints at the offsets closest to our target location's column.
   *
   * @param Array scripts
   *        The set of Debugger.Script instances to consider.
   * @param Object location
   *        The target location.
   * @param BreakpointActor actor
   *        The BreakpointActor to handle hitting the breakpoints we set.
   * @returns Object
   *          The RDP response.
   */
  _setBreakpointAtColumn: function (scripts, location, actor) {
    // Debugger.Script -> array of offset mappings
    const scriptsAndOffsetMappings = new Map();

    for (let script of scripts) {
      this._findClosestOffsetMappings(location, script, scriptsAndOffsetMappings);
    }

    for (let [script, mappings] of scriptsAndOffsetMappings) {
      for (let offsetMapping of mappings) {
        script.setBreakpoint(offsetMapping.offset, actor);
      }
      actor.addScript(script, this.threadActor);
    }

    return {
      actor: actor.actorID
    };
  },

  /**
   * Find the first line that is associated with bytecode offsets, and is
   * greater than or equal to the given start line.
   *
   * @param Array scripts
   *        The set of Debugger.Script instances to consider.
   * @param Number startLine
   *        The target line.
   * @return Object|null
   *         If we can't find a line matching our constraints, return
   *         null. Otherwise, return an object of the form:
   *           {
   *             line: Number,
   *             entryPoints: [
   *               { script: Debugger.Script, offsets: [offset, ...] },
   *               ...
   *             ]
   *           }
   */
  _findNextLineWithOffsets: function (scripts, startLine) {
    const maxLine = Math.max(...scripts.map(s => s.startLine + s.lineCount));

    for (let line = startLine; line < maxLine; line++) {
      const entryPoints = findEntryPointsForLine(scripts, line);
      if (entryPoints.length) {
        return { line, entryPoints };
      }
    }

    return null;
  },

  /**
   * Get or create a BreakpointActor for the given location, and set it as a
   * breakpoint handler on all scripts that match the given location for which
   * the BreakpointActor is not already a breakpoint handler.
   *
   * It is possible that no scripts match the given location, because they have
   * all been garbage collected. In that case, the BreakpointActor is not set as
   * a breakpoint handler for any script, but is still inserted in the
   * BreakpointActorMap as a pending breakpoint. Whenever a new script is
   * introduced, we call this method again to see if there are now any scripts
   * that matches the given location.
   *
   * The first time we find one or more scripts that matches the given location,
   * we check if any of these scripts has any entry points for the given
   * location. If not, we assume that the given location does not have any code.
   *
   * If the given location does not contain any code, we slide the breakpoint
   * down to the next closest line that does, and update the BreakpointActorMap
   * accordingly. Note that we only do so if the breakpoint actor is still
   * pending (i.e. is not set as a breakpoint handler for any script).
   *
   * @param Number originalLine
   *        The line number of the breakpoint in the original source.
   * @param Number originalColumn
   *        The column number of the breakpoint in the original source.
   * @param String condition
   *        A condition for the breakpoint.
   */
  setBreakpoint: function (originalLine, originalColumn, condition) {
    let originalLocation = {
      sourceActor: this,
      line: originalLine,
      column: originalColumn
    };

    return this.threadActor.sources.getGeneratedLocation(originalLocation)
                                   .then(generatedLocation => {
      let actor = this._getOrCreateBreakpointActor(originalLocation,
                                                   generatedLocation,
                                                   condition);
      return generatedLocation.sourceActor.setBreakpointForActor(actor);
    });
  },

  /*
   * Set the given BreakpointActor as breakpoint handler on all scripts that
   * match the given location for which the BreakpointActor is not already a
   * breakpoint handler.
   *
   * @param BreakpointActor actor
   *        The BreakpointActor to set as breakpoint handler.
   */
  setBreakpointForActor: function (actor) {
    let originalLocation = actor.originalLocation;
    let generatedLocation = {
      sourceActor: this,
      line: actor.generatedLocation.line,
      column: actor.generatedLocation.column
    };

    let { line: generatedLine, column: generatedColumn } = generatedLocation;

    // Find all scripts matching the given location. We will almost always have
    // a `source` object to query, but multiple inline HTML scripts are all
    // represented by a single SourceActor even though they have separate source
    // objects, so we need to query based on the url of the page for them.
    let scripts = this.source
      ? this.scripts.getScriptsBySourceAndLine(this.source, generatedLine)
      : this.scripts.getScriptsByURLAndLine(this._originalUrl, generatedLine);

    if (scripts.length === 0) {
      // Since we did not find any scripts to set the breakpoint on now, return
      // early. When a new script that matches this breakpoint location is
      // introduced, the breakpoint actor will already be in the breakpoint
      // store and the breakpoint will be set at that time. This is similar to
      // GDB's "pending" breakpoints for shared libraries that aren't loaded
      // yet.
      return Promise.resolve({
        actor: actor.actorID
      });
    }

    // Ignore scripts for which the BreakpointActor is already a breakpoint
    // handler.
    scripts = scripts.filter((script) => !actor.hasScript(script));

    let actualLocation;

    // If generatedColumn is something other than 0, assume this is a column
    // breakpoint and do not perform breakpoint sliding.
    if (generatedColumn) {
      this._setBreakpointAtColumn(scripts, generatedLocation, actor);
      actualLocation = generatedLocation;
    } else {
      let result;
      if (actor.scripts.size === 0) {
        // If the BreakpointActor is not a breakpoint handler for any script, its
        // location is not yet fixed. Use breakpoint sliding to select the first
        // line greater than or equal to the requested line that has one or more
        // offsets.
        result = this._findNextLineWithOffsets(scripts, generatedLine);
      } else {
        // If the BreakpointActor is a breakpoint handler for at least one script,
        // breakpoint sliding has already been carried out, so select the
        // requested line, even if it does not have any offsets.
        let entryPoints = findEntryPointsForLine(scripts, generatedLine)
        if (entryPoints) {
          result = {
            line: generatedLine,
            entryPoints: entryPoints
          };
        }
      }

      if (!result) {
        return Promise.resolve({
          error: "noCodeAtLineColumn",
          actor: actor.actorID
        });
      }

      if (result.line !== generatedLine) {
        actualLocation = {
          sourceActor: generatedLocation.sourceActor,
          line: result.line,
          column: generatedLocation.column
        };

        // Check whether we already have a breakpoint actor for the actual
        // location. If we do have an existing actor, then the actor we created
        // above is redundant and must be destroyed. If we do not have an existing
        // actor, we need to update the breakpoint store with the new location.

        let existingActor = this.breakpointActorMap.getActor(actualLocation);
        if (existingActor) {
          actor.onDelete();
          this.breakpointActorMap.deleteActor(generatedLocation);
          actor = existingActor;
        } else {
          actor.generatedLocation = actualLocation;
          this.breakpointActorMap.deleteActor(generatedLocation);
          this.breakpointActorMap.setActor(actualLocation, actor);
          setBreakpointOnEntryPoints(this.threadActor, actor, result.entryPoints);
        }
      } else {
        setBreakpointOnEntryPoints(this.threadActor, actor, result.entryPoints);
        actualLocation = generatedLocation;
      }
    }

    return Promise.resolve().then(() => {
      if (actualLocation.sourceActor.source) {
        return this.threadActor.sources.getOriginalLocation({
          sourceActor: actualLocation.sourceActor,
          line: actualLocation.line,
          column: actualLocation.column
        });
      } else {
        return actualLocation;
      }
    }).then((actualLocation) => {
      let response = { actor: actor.actorID };
      if (actualLocation.sourceActor.url !== originalLocation.sourceActor.url ||
          actualLocation.line !== originalLocation.line)
      {
        response.actualLocation = {
          source: actualLocation.sourceActor.form(),
          line: actualLocation.line,
          column: actualLocation.column
        };
      }
      return response;
    });
  },

  /**
   * Find all of the offset mappings associated with `aScript` that are closest
   * to `aTargetLocation`. If new offset mappings are found that are closer to
   * `aTargetOffset` than the existing offset mappings inside
   * `aScriptsAndOffsetMappings`, we empty that map and only consider the
   * closest offset mappings.
   *
   * In many cases, but not all, this method finds only one closest offset.
   * Consider the following case, where multiple offsets will be found:
   *
   *     0         1         2         3
   *     0123456789012345678901234567890
   *    +-------------------------------
   *   1|function f() {
   *   2|  return g() + h();
   *   3|}
   *
   * The Debugger reports three offsets on line 2 upon which we could set a
   * breakpoint: the `return` statement at column 2, the call expression `g()`
   * at column 9, and the call expression `h()` at column 15. (Careful readers
   * will note that complete source location information isn't saved by
   * SpiderMonkey's frontend, and we don't get an offset associated specifically
   * with the `+` operation.)
   *
   * If our target location is line 2 column 12, the offset for the call to `g`
   * is 3 columns to the left and the offset for the call to `h` is 3 columns to
   * the right. Because they are equally close, we will return both offsets to
   * have breakpoints set upon them.
   *
   * @param Object aTargetLocation
   *        An object of the form { url, line[, column] }.
   * @param Debugger.Script aScript
   *        The script in which we are searching for offsets.
   * @param Map aScriptsAndOffsetMappings
   *        A Map object which maps Debugger.Script instances to arrays of
   *        offset mappings. This is an out param.
   */
  _findClosestOffsetMappings: function (aTargetLocation,
                                aScript,
                                aScriptsAndOffsetMappings) {
    let offsetMappings = aScript.getAllColumnOffsets()
      .filter(({ lineNumber }) => lineNumber === aTargetLocation.line);

    // Attempt to find the current closest offset distance from the target
    // location by grabbing any offset mapping in the map by doing one iteration
    // and then breaking (they all have the same distance from the target
    // location).
    let closestDistance = Infinity;
    if (aScriptsAndOffsetMappings.size) {
      for (let mappings of aScriptsAndOffsetMappings.values()) {
        closestDistance = Math.abs(aTargetLocation.column - mappings[0].columnNumber);
        break;
      }
    }

    for (let mapping of offsetMappings) {
      let currentDistance = Math.abs(aTargetLocation.column - mapping.columnNumber);

      if (currentDistance > closestDistance) {
        continue;
      } else if (currentDistance < closestDistance) {
        closestDistance = currentDistance;
        aScriptsAndOffsetMappings.clear();
        aScriptsAndOffsetMappings.set(aScript, [mapping]);
      } else {
        if (!aScriptsAndOffsetMappings.has(aScript)) {
          aScriptsAndOffsetMappings.set(aScript, []);
        }
        aScriptsAndOffsetMappings.get(aScript).push(mapping);
      }
    }
  }
};

SourceActor.prototype.requestTypes = {
  "source": SourceActor.prototype.onSource,
  "blackbox": SourceActor.prototype.onBlackBox,
  "unblackbox": SourceActor.prototype.onUnblackBox,
  "prettyPrint": SourceActor.prototype.onPrettyPrint,
  "disablePrettyPrint": SourceActor.prototype.onDisablePrettyPrint,
  "getExecutableLines": SourceActor.prototype.getExecutableLines,
  "setBreakpoint": SourceActor.prototype.onSetBreakpoint
};


/**
 * Determine if a given value is non-primitive.
 *
 * @param Any aValue
 *        The value to test.
 * @return Boolean
 *         Whether the value is non-primitive.
 */
function isObject(aValue) {
  const type = typeof aValue;
  return type == "object" ? aValue !== null : type == "function";
}

/**
 * Create a function that can safely stringify Debugger.Objects of a given
 * builtin type.
 *
 * @param Function aCtor
 *        The builtin class constructor.
 * @return Function
 *         The stringifier for the class.
 */
function createBuiltinStringifier(aCtor) {
  return aObj => aCtor.prototype.toString.call(aObj.unsafeDereference());
}

/**
 * Stringify a Debugger.Object-wrapped Error instance.
 *
 * @param Debugger.Object aObj
 *        The object to stringify.
 * @return String
 *         The stringification of the object.
 */
function errorStringify(aObj) {
  let name = DevToolsUtils.getProperty(aObj, "name");
  if (name === "" || name === undefined) {
    name = aObj.class;
  } else if (isObject(name)) {
    name = stringify(name);
  }

  let message = DevToolsUtils.getProperty(aObj, "message");
  if (isObject(message)) {
    message = stringify(message);
  }

  if (message === "" || message === undefined) {
    return name;
  }
  return name + ": " + message;
}

/**
 * Stringify a Debugger.Object based on its class.
 *
 * @param Debugger.Object aObj
 *        The object to stringify.
 * @return String
 *         The stringification for the object.
 */
function stringify(aObj) {
  if (aObj.class == "DeadObject") {
    const error = new Error("Dead object encountered.");
    DevToolsUtils.reportException("stringify", error);
    return "<dead object>";
  }

  const stringifier = stringifiers[aObj.class] || stringifiers.Object;

  try {
    return stringifier(aObj);
  } catch (e) {
    DevToolsUtils.reportException("stringify", e);
    return "<failed to stringify object>";
  }
}

// Used to prevent infinite recursion when an array is found inside itself.
let seen = null;

let stringifiers = {
  Error: errorStringify,
  EvalError: errorStringify,
  RangeError: errorStringify,
  ReferenceError: errorStringify,
  SyntaxError: errorStringify,
  TypeError: errorStringify,
  URIError: errorStringify,
  Boolean: createBuiltinStringifier(Boolean),
  Function: createBuiltinStringifier(Function),
  Number: createBuiltinStringifier(Number),
  RegExp: createBuiltinStringifier(RegExp),
  String: createBuiltinStringifier(String),
  Object: obj => "[object " + obj.class + "]",
  Array: obj => {
    // If we're at the top level then we need to create the Set for tracking
    // previously stringified arrays.
    const topLevel = !seen;
    if (topLevel) {
      seen = new Set();
    } else if (seen.has(obj)) {
      return "";
    }

    seen.add(obj);

    const len = DevToolsUtils.getProperty(obj, "length");
    let string = "";

    // The following check is only required because the debuggee could possibly
    // be a Proxy and return any value. For normal objects, array.length is
    // always a non-negative integer.
    if (typeof len == "number" && len > 0) {
      for (let i = 0; i < len; i++) {
        const desc = obj.getOwnPropertyDescriptor(i);
        if (desc) {
          const { value } = desc;
          if (value != null) {
            string += isObject(value) ? stringify(value) : value;
          }
        }

        if (i < len - 1) {
          string += ",";
        }
      }
    }

    if (topLevel) {
      seen = null;
    }

    return string;
  },
  DOMException: obj => {
    const message = DevToolsUtils.getProperty(obj, "message") || "<no message>";
    const result = (+DevToolsUtils.getProperty(obj, "result")).toString(16);
    const code = DevToolsUtils.getProperty(obj, "code");
    const name = DevToolsUtils.getProperty(obj, "name") || "<unknown>";

    return '[Exception... "' + message + '" ' +
           'code: "' + code +'" ' +
           'nsresult: "0x' + result + ' (' + name + ')"]';
  },
  Promise: obj => {
    const { state, value, reason } = getPromiseState(obj);
    let statePreview = state;
    if (state != "pending") {
      const settledValue = state === "fulfilled" ? value : reason;
      statePreview += ": " + (typeof settledValue === "object" && settledValue !== null
                                ? stringify(settledValue)
                                : settledValue);
    }
    return "Promise (" + statePreview + ")";
  },
};

/**
 * Creates an actor for the specified object.
 *
 * @param aObj Debugger.Object
 *        The debuggee object.
 * @param aThreadActor ThreadActor
 *        The parent thread actor for this object.
 */
function ObjectActor(aObj, aThreadActor)
{
  dbg_assert(!aObj.optimizedOut, "Should not create object actors for optimized out values!");
  this.obj = aObj;
  this.threadActor = aThreadActor;
}

ObjectActor.prototype = {
  actorPrefix: "obj",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function () {
    this.threadActor._gripDepth++;

    let g = {
      "type": "object",
      "class": this.obj.class,
      "actor": this.actorID,
      "extensible": this.obj.isExtensible(),
      "frozen": this.obj.isFrozen(),
      "sealed": this.obj.isSealed()
    };

    if (this.obj.class != "DeadObject") {
      // Expose internal Promise state.
      if (this.obj.class == "Promise") {
        const { state, value, reason } = getPromiseState(this.obj);
        g.promiseState = { state };
        if (state == "fulfilled") {
          g.promiseState.value = this.threadActor.createValueGrip(value);
        } else if (state == "rejected") {
          g.promiseState.reason = this.threadActor.createValueGrip(reason);
        }
      }

      let raw = this.obj.unsafeDereference();

      // If Cu is not defined, we are running on a worker thread, where xrays
      // don't exist.
      if (Cu) {
        raw = Cu.unwaiveXrays(raw);
      }

      if (!DevToolsUtils.isSafeJSObject(raw)) {
        raw = null;
      }

      let previewers = DebuggerServer.ObjectActorPreviewers[this.obj.class] ||
                       DebuggerServer.ObjectActorPreviewers.Object;
      for (let fn of previewers) {
        try {
          if (fn(this, g, raw)) {
            break;
          }
        } catch (e) {
          DevToolsUtils.reportException("ObjectActor.prototype.grip previewer function", e);
        }
      }
    }

    this.threadActor._gripDepth--;
    return g;
  },

  /**
   * Releases this actor from the pool.
   */
  release: function () {
    if (this.registeredPool.objectActors) {
      this.registeredPool.objectActors.delete(this.obj);
    }
    this.registeredPool.removeActor(this);
  },

  /**
   * Handle a protocol request to provide the definition site of this function
   * object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDefinitionSite: function OA_onDefinitionSite(aRequest) {
    if (this.obj.class != "Function") {
      return {
        from: this.actorID,
        error: "objectNotFunction",
        message: this.actorID + " is not a function."
      };
    }

    if (!this.obj.script) {
      return {
        from: this.actorID,
        error: "noScript",
        message: this.actorID + " has no Debugger.Script"
      };
    }

    const generatedLocation = {
      sourceActor: this.threadActor.sources.createNonSourceMappedActor(this.obj.script.source),
      line: this.obj.script.startLine,
      // TODO bug 901138: use Debugger.Script.prototype.startColumn.
      column: 0
    };

    return this.threadActor.sources.getOriginalLocation(generatedLocation)
      .then(({ sourceActor, line, column }) => {

        return {
          from: this.actorID,
          source: sourceActor.form(),
          line: line,
          column: column
        };
      });
  },

  /**
   * Handle a protocol request to provide the names of the properties defined on
   * the object and not its prototype.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onOwnPropertyNames: function (aRequest) {
    return { from: this.actorID,
             ownPropertyNames: this.obj.getOwnPropertyNames() };
  },

  /**
   * Handle a protocol request to provide the prototype and own properties of
   * the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPrototypeAndProperties: function (aRequest) {
    let ownProperties = Object.create(null);
    let names;
    try {
      names = this.obj.getOwnPropertyNames();
    } catch (ex) {
      // The above can throw if this.obj points to a dead object.
      // TODO: we should use Cu.isDeadWrapper() - see bug 885800.
      return { from: this.actorID,
               prototype: this.threadActor.createValueGrip(null),
               ownProperties: ownProperties,
               safeGetterValues: Object.create(null) };
    }
    for (let name of names) {
      ownProperties[name] = this._propertyDescriptor(name);
    }
    return { from: this.actorID,
             prototype: this.threadActor.createValueGrip(this.obj.proto),
             ownProperties: ownProperties,
             safeGetterValues: this._findSafeGetterValues(ownProperties) };
  },

  /**
   * Find the safe getter values for the current Debugger.Object, |this.obj|.
   *
   * @private
   * @param object aOwnProperties
   *        The object that holds the list of known ownProperties for
   *        |this.obj|.
   * @param number [aLimit=0]
   *        Optional limit of getter values to find.
   * @return object
   *         An object that maps property names to safe getter descriptors as
   *         defined by the remote debugging protocol.
   */
  _findSafeGetterValues: function (aOwnProperties, aLimit = 0)
  {
    let safeGetterValues = Object.create(null);
    let obj = this.obj;
    let level = 0, i = 0;

    while (obj) {
      let getters = this._findSafeGetters(obj);
      for (let name of getters) {
        // Avoid overwriting properties from prototypes closer to this.obj. Also
        // avoid providing safeGetterValues from prototypes if property |name|
        // is already defined as an own property.
        if (name in safeGetterValues ||
            (obj != this.obj && name in aOwnProperties)) {
          continue;
        }

        let desc = null, getter = null;
        try {
          desc = obj.getOwnPropertyDescriptor(name);
          getter = desc.get;
        } catch (ex) {
          // The above can throw if the cache becomes stale.
        }
        if (!getter) {
          obj._safeGetters = null;
          continue;
        }

        let result = getter.call(this.obj);
        if (result && !("throw" in result)) {
          let getterValue = undefined;
          if ("return" in result) {
            getterValue = result.return;
          } else if ("yield" in result) {
            getterValue = result.yield;
          }
          // WebIDL attributes specified with the LenientThis extended attribute
          // return undefined and should be ignored.
          if (getterValue !== undefined) {
            safeGetterValues[name] = {
              getterValue: this.threadActor.createValueGrip(getterValue),
              getterPrototypeLevel: level,
              enumerable: desc.enumerable,
              writable: level == 0 ? desc.writable : true,
            };
            if (aLimit && ++i == aLimit) {
              break;
            }
          }
        }
      }
      if (aLimit && i == aLimit) {
        break;
      }

      obj = obj.proto;
      level++;
    }

    return safeGetterValues;
  },

  /**
   * Find the safe getters for a given Debugger.Object. Safe getters are native
   * getters which are safe to execute.
   *
   * @private
   * @param Debugger.Object aObject
   *        The Debugger.Object where you want to find safe getters.
   * @return Set
   *         A Set of names of safe getters. This result is cached for each
   *         Debugger.Object.
   */
  _findSafeGetters: function (aObject)
  {
    if (aObject._safeGetters) {
      return aObject._safeGetters;
    }

    let getters = new Set();
    let names = [];
    try {
      names = aObject.getOwnPropertyNames()
    } catch (ex) {
      // Calling getOwnPropertyNames() on some wrapped native prototypes is not
      // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
    }

    for (let name of names) {
      let desc = null;
      try {
        desc = aObject.getOwnPropertyDescriptor(name);
      } catch (e) {
        // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
        // allowed (bug 560072).
      }
      if (!desc || desc.value !== undefined || !("get" in desc)) {
        continue;
      }

      if (DevToolsUtils.hasSafeGetter(desc)) {
        getters.add(name);
      }
    }

    aObject._safeGetters = getters;
    return getters;
  },

  /**
   * Handle a protocol request to provide the prototype of the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPrototype: function (aRequest) {
    return { from: this.actorID,
             prototype: this.threadActor.createValueGrip(this.obj.proto) };
  },

  /**
   * Handle a protocol request to provide the property descriptor of the
   * object's specified property.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onProperty: function (aRequest) {
    if (!aRequest.name) {
      return { error: "missingParameter",
               message: "no property name was specified" };
    }

    return { from: this.actorID,
             descriptor: this._propertyDescriptor(aRequest.name) };
  },

  /**
   * Handle a protocol request to provide the display string for the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDisplayString: function (aRequest) {
    const string = stringify(this.obj);
    return { from: this.actorID,
             displayString: this.threadActor.createValueGrip(string) };
  },

  /**
   * A helper method that creates a property descriptor for the provided object,
   * properly formatted for sending in a protocol response.
   *
   * @private
   * @param string aName
   *        The property that the descriptor is generated for.
   * @param boolean [aOnlyEnumerable]
   *        Optional: true if you want a descriptor only for an enumerable
   *        property, false otherwise.
   * @return object|undefined
   *         The property descriptor, or undefined if this is not an enumerable
   *         property and aOnlyEnumerable=true.
   */
  _propertyDescriptor: function (aName, aOnlyEnumerable) {
    let desc;
    try {
      desc = this.obj.getOwnPropertyDescriptor(aName);
    } catch (e) {
      // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
      // allowed (bug 560072). Inform the user with a bogus, but hopefully
      // explanatory, descriptor.
      return {
        configurable: false,
        writable: false,
        enumerable: false,
        value: e.name
      };
    }

    if (!desc || aOnlyEnumerable && !desc.enumerable) {
      return undefined;
    }

    let retval = {
      configurable: desc.configurable,
      enumerable: desc.enumerable
    };

    if ("value" in desc) {
      retval.writable = desc.writable;
      retval.value = this.threadActor.createValueGrip(desc.value);
    } else {
      if ("get" in desc) {
        retval.get = this.threadActor.createValueGrip(desc.get);
      }
      if ("set" in desc) {
        retval.set = this.threadActor.createValueGrip(desc.set);
      }
    }
    return retval;
  },

  /**
   * Handle a protocol request to provide the source code of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDecompile: function (aRequest) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "decompile request is only valid for object grips " +
                        "with a 'Function' class." };
    }

    return { from: this.actorID,
             decompiledCode: this.obj.decompile(!!aRequest.pretty) };
  },

  /**
   * Handle a protocol request to provide the parameters of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onParameterNames: function (aRequest) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "'parameterNames' request is only valid for object " +
                        "grips with a 'Function' class." };
    }

    return { parameterNames: this.obj.parameterNames };
  },

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onRelease: function (aRequest) {
    this.release();
    return {};
  },

  /**
   * Handle a protocol request to provide the lexical scope of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onScope: function (aRequest) {
    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "scope request is only valid for object grips with a" +
                        " 'Function' class." };
    }

    let envActor = this.threadActor.createEnvironmentActor(this.obj.environment,
                                                           this.registeredPool);
    if (!envActor) {
      return { error: "notDebuggee",
               message: "cannot access the environment of this function." };
    }

    return { from: this.actorID, scope: envActor.form() };
  }
};

ObjectActor.prototype.requestTypes = {
  "definitionSite": ObjectActor.prototype.onDefinitionSite,
  "parameterNames": ObjectActor.prototype.onParameterNames,
  "prototypeAndProperties": ObjectActor.prototype.onPrototypeAndProperties,
  "prototype": ObjectActor.prototype.onPrototype,
  "property": ObjectActor.prototype.onProperty,
  "displayString": ObjectActor.prototype.onDisplayString,
  "ownPropertyNames": ObjectActor.prototype.onOwnPropertyNames,
  "decompile": ObjectActor.prototype.onDecompile,
  "release": ObjectActor.prototype.onRelease,
  "scope": ObjectActor.prototype.onScope,
};

exports.ObjectActor = ObjectActor;

/**
 * Functions for adding information to ObjectActor grips for the purpose of
 * having customized output. This object holds arrays mapped by
 * Debugger.Object.prototype.class.
 *
 * In each array you can add functions that take two
 * arguments:
 *   - the ObjectActor instance to make a preview for,
 *   - the grip object being prepared for the client,
 *   - the raw JS object after calling Debugger.Object.unsafeDereference(). This
 *   argument is only provided if the object is safe for reading properties and
 *   executing methods. See DevToolsUtils.isSafeJSObject().
 *
 * Functions must return false if they cannot provide preview
 * information for the debugger object, or true otherwise.
 */
DebuggerServer.ObjectActorPreviewers = {
  String: [function({obj, threadActor}, aGrip) {
    let result = genericObjectPreviewer("String", String, obj, threadActor);
    let length = DevToolsUtils.getProperty(obj, "length");

    if (!result || typeof length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length
    };

    if (threadActor._gripDepth > 1) {
      return true;
    }

    let items = aGrip.preview.items = [];

    const max = Math.min(result.value.length, OBJECT_PREVIEW_MAX_ITEMS);
    for (let i = 0; i < max; i++) {
      let value = threadActor.createValueGrip(result.value[i]);
      items.push(value);
    }

    return true;
  }],

  Boolean: [function({obj, threadActor}, aGrip) {
    let result = genericObjectPreviewer("Boolean", Boolean, obj, threadActor);
    if (result) {
      aGrip.preview = result;
      return true;
    }

    return false;
  }],

  Number: [function({obj, threadActor}, aGrip) {
    let result = genericObjectPreviewer("Number", Number, obj, threadActor);
    if (result) {
      aGrip.preview = result;
      return true;
    }

    return false;
  }],

  Function: [function({obj, threadActor}, aGrip) {
    if (obj.name) {
      aGrip.name = obj.name;
    }

    if (obj.displayName) {
      aGrip.displayName = obj.displayName.substr(0, 500);
    }

    if (obj.parameterNames) {
      aGrip.parameterNames = obj.parameterNames;
    }

    // Check if the developer has added a de-facto standard displayName
    // property for us to use.
    let userDisplayName;
    try {
      userDisplayName = obj.getOwnPropertyDescriptor("displayName");
    } catch (e) {
      // Calling getOwnPropertyDescriptor with displayName might throw
      // with "permission denied" errors for some functions.
      dumpn(e);
    }

    if (userDisplayName && typeof userDisplayName.value == "string" &&
        userDisplayName.value) {
      aGrip.userDisplayName = threadActor.createValueGrip(userDisplayName.value);
    }

    return true;
  }],

  RegExp: [function({obj, threadActor}, aGrip) {
    // Avoid having any special preview for the RegExp.prototype itself.
    if (!obj.proto || obj.proto.class != "RegExp") {
      return false;
    }

    let str = RegExp.prototype.toString.call(obj.unsafeDereference());
    aGrip.displayString = threadActor.createValueGrip(str);
    return true;
  }],

  Date: [function({obj, threadActor}, aGrip) {
    if (!obj.proto || obj.proto.class != "Date") {
      return false;
    }

    let time = Date.prototype.getTime.call(obj.unsafeDereference());

    aGrip.preview = {
      timestamp: threadActor.createValueGrip(time),
    };
    return true;
  }],

  Array: [function({obj, threadActor}, aGrip) {
    let length = DevToolsUtils.getProperty(obj, "length");
    if (typeof length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    if (threadActor._gripDepth > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let items = aGrip.preview.items = [];

    for (let i = 0; i < length; ++i) {
      // Array Xrays filter out various possibly-unsafe properties (like
      // functions, and claim that the value is undefined instead. This
      // is generally the right thing for privileged code accessing untrusted
      // objects, but quite confusing for Object previews. So we manually
      // override this protection by waiving Xrays on the array, and re-applying
      // Xrays on any indexed value props that we pull off of it.
      let desc = Object.getOwnPropertyDescriptor(Cu.waiveXrays(raw), i);
      if (desc && !desc.get && !desc.set) {
        let value = Cu.unwaiveXrays(desc.value);
        value = makeDebuggeeValueIfNeeded(obj, value);
        items.push(threadActor.createValueGrip(value));
      } else {
        items.push(null);
      }

      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // Array

  Set: [function({obj, threadActor}, aGrip) {
    let size = DevToolsUtils.getProperty(obj, "size");
    if (typeof size != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: size,
    };

    // Avoid recursive object grips.
    if (threadActor._gripDepth > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let items = aGrip.preview.items = [];
    // We currently lack XrayWrappers for Set, so when we iterate over
    // the values, the temporary iterator objects get created in the target
    // compartment. However, we _do_ have Xrays to Object now, so we end up
    // Xraying those temporary objects, and filtering access to |it.value|
    // based on whether or not it's Xrayable and/or callable, which breaks
    // the for/of iteration.
    //
    // This code is designed to handle untrusted objects, so we can safely
    // waive Xrays on the iterable, and relying on the Debugger machinery to
    // make sure we handle the resulting objects carefully.
    for (let item of Cu.waiveXrays(Set.prototype.values.call(raw))) {
      item = Cu.unwaiveXrays(item);
      item = makeDebuggeeValueIfNeeded(obj, item);
      items.push(threadActor.createValueGrip(item));
      if (items.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // Set

  Map: [function({obj, threadActor}, aGrip) {
    let size = DevToolsUtils.getProperty(obj, "size");
    if (typeof size != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "MapLike",
      size: size,
    };

    if (threadActor._gripDepth > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let entries = aGrip.preview.entries = [];
    // Iterating over a Map via .entries goes through various intermediate
    // objects - an Iterator object, then a 2-element Array object, then the
    // actual values we care about. We don't have Xrays to Iterator objects,
    // so we get Opaque wrappers for them. And even though we have Xrays to
    // Arrays, the semantics often deny access to the entires based on the
    // nature of the values. So we need waive Xrays for the iterator object
    // and the tupes, and then re-apply them on the underlying values until
    // we fix bug 1023984.
    //
    // Even then though, we might want to continue waiving Xrays here for the
    // same reason we do so for Arrays above - this filtering behavior is likely
    // to be more confusing than beneficial in the case of Object previews.
    for (let keyValuePair of Cu.waiveXrays(Map.prototype.entries.call(raw))) {
      let key = Cu.unwaiveXrays(keyValuePair[0]);
      let value = Cu.unwaiveXrays(keyValuePair[1]);
      key = makeDebuggeeValueIfNeeded(obj, key);
      value = makeDebuggeeValueIfNeeded(obj, value);
      entries.push([threadActor.createValueGrip(key),
                    threadActor.createValueGrip(value)]);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // Map

  DOMStringMap: [function({obj, threadActor}, aGrip, aRawObj) {
    if (!aRawObj) {
      return false;
    }

    let keys = obj.getOwnPropertyNames();
    aGrip.preview = {
      kind: "MapLike",
      size: keys.length,
    };

    if (threadActor._gripDepth > 1) {
      return true;
    }

    let entries = aGrip.preview.entries = [];
    for (let key of keys) {
      let value = makeDebuggeeValueIfNeeded(obj, aRawObj[key]);
      entries.push([key, threadActor.createValueGrip(value)]);
      if (entries.length == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return true;
  }], // DOMStringMap

}; // DebuggerServer.ObjectActorPreviewers

/**
 * Generic previewer for "simple" classes like String, Number and Boolean.
 *
 * @param string aClassName
 *        Class name to expect.
 * @param object aClass
 *        The class to expect, eg. String. The valueOf() method of the class is
 *        invoked on the given object.
 * @param Debugger.Object aObj
 *        The debugger object we need to preview.
 * @param object aThreadActor
 *        The thread actor to use to create a value grip.
 * @return object|null
 *         An object with one property, "value", which holds the value grip that
 *         represents the given object. Null is returned if we cant preview the
 *         object.
 */
function genericObjectPreviewer(aClassName, aClass, aObj, aThreadActor) {
  if (!aObj.proto || aObj.proto.class != aClassName) {
    return null;
  }

  let raw = aObj.unsafeDereference();
  let v = null;
  try {
    v = aClass.prototype.valueOf.call(raw);
  } catch (ex) {
    // valueOf() can throw if the raw JS object is "misbehaved".
    return null;
  }

  if (v !== null) {
    v = aThreadActor.createValueGrip(makeDebuggeeValueIfNeeded(aObj, v));
    return { value: v };
  }

  return null;
}

// Preview functions that do not rely on the object class.
DebuggerServer.ObjectActorPreviewers.Object = [
  function TypedArray({obj, threadActor}, aGrip) {
    if (TYPED_ARRAY_CLASSES.indexOf(obj.class) == -1) {
      return false;
    }

    let length = DevToolsUtils.getProperty(obj, "length");
    if (typeof length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: length,
    };

    if (threadActor._gripDepth > 1) {
      return true;
    }

    let raw = obj.unsafeDereference();
    let global = Cu.getGlobalForObject(DebuggerServer);
    let classProto = global[obj.class].prototype;
    // The Xray machinery for TypedArrays denies indexed access on the grounds
    // that it's slow, and advises callers to do a structured clone instead.
    let safeView = Cu.cloneInto(classProto.subarray.call(raw, 0, OBJECT_PREVIEW_MAX_ITEMS), global);
    let items = aGrip.preview.items = [];
    for (let i = 0; i < safeView.length; i++) {
      items.push(safeView[i]);
    }

    return true;
  },

  function Error({obj, threadActor}, aGrip) {
    switch (obj.class) {
      case "Error":
      case "EvalError":
      case "RangeError":
      case "ReferenceError":
      case "SyntaxError":
      case "TypeError":
      case "URIError":
        let name = DevToolsUtils.getProperty(obj, "name");
        let msg = DevToolsUtils.getProperty(obj, "message");
        let stack = DevToolsUtils.getProperty(obj, "stack");
        let fileName = DevToolsUtils.getProperty(obj, "fileName");
        let lineNumber = DevToolsUtils.getProperty(obj, "lineNumber");
        let columnNumber = DevToolsUtils.getProperty(obj, "columnNumber");
        aGrip.preview = {
          kind: "Error",
          name: threadActor.createValueGrip(name),
          message: threadActor.createValueGrip(msg),
          stack: threadActor.createValueGrip(stack),
          fileName: threadActor.createValueGrip(fileName),
          lineNumber: threadActor.createValueGrip(lineNumber),
          columnNumber: threadActor.createValueGrip(columnNumber),
        };
        return true;
      default:
        return false;
    }
  },

  function CSSMediaRule({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSMediaRule)) {
      return false;
    }
    aGrip.preview = {
      kind: "ObjectWithText",
      text: threadActor.createValueGrip(aRawObj.conditionText),
    };
    return true;
  },

  function CSSStyleRule({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSStyleRule)) {
      return false;
    }
    aGrip.preview = {
      kind: "ObjectWithText",
      text: threadActor.createValueGrip(aRawObj.selectorText),
    };
    return true;
  },

  function ObjectWithURL({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSImportRule ||
                                  aRawObj instanceof Ci.nsIDOMCSSStyleSheet ||
                                  aRawObj instanceof Ci.nsIDOMLocation ||
                                  aRawObj instanceof Ci.nsIDOMWindow)) {
      return false;
    }

    let url;
    if (aRawObj instanceof Ci.nsIDOMWindow && aRawObj.location) {
      url = aRawObj.location.href;
    } else if (aRawObj.href) {
      url = aRawObj.href;
    } else {
      return false;
    }

    aGrip.preview = {
      kind: "ObjectWithURL",
      url: threadActor.createValueGrip(url),
    };

    return true;
  },

  function ArrayLike({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj ||
        obj.class != "DOMStringList" &&
        obj.class != "DOMTokenList" &&
        !(aRawObj instanceof Ci.nsIDOMMozNamedAttrMap ||
          aRawObj instanceof Ci.nsIDOMCSSRuleList ||
          aRawObj instanceof Ci.nsIDOMCSSValueList ||
          aRawObj instanceof Ci.nsIDOMFileList ||
          aRawObj instanceof Ci.nsIDOMFontFaceList ||
          aRawObj instanceof Ci.nsIDOMMediaList ||
          aRawObj instanceof Ci.nsIDOMNodeList ||
          aRawObj instanceof Ci.nsIDOMStyleSheetList)) {
      return false;
    }

    if (typeof aRawObj.length != "number") {
      return false;
    }

    aGrip.preview = {
      kind: "ArrayLike",
      length: aRawObj.length,
    };

    if (threadActor._gripDepth > 1) {
      return true;
    }

    let items = aGrip.preview.items = [];

    for (let i = 0; i < aRawObj.length &&
                    items.length < OBJECT_PREVIEW_MAX_ITEMS; i++) {
      let value = makeDebuggeeValueIfNeeded(obj, aRawObj[i]);
      items.push(threadActor.createValueGrip(value));
    }

    return true;
  }, // ArrayLike

  function CSSStyleDeclaration({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMCSSStyleDeclaration)) {
      return false;
    }

    aGrip.preview = {
      kind: "MapLike",
      size: aRawObj.length,
    };

    let entries = aGrip.preview.entries = [];

    for (let i = 0; i < OBJECT_PREVIEW_MAX_ITEMS &&
                    i < aRawObj.length; i++) {
      let prop = aRawObj[i];
      let value = aRawObj.getPropertyValue(prop);
      entries.push([prop, threadActor.createValueGrip(value)]);
    }

    return true;
  },

  function DOMNode({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || obj.class == "Object" || !aRawObj ||
        !(aRawObj instanceof Ci.nsIDOMNode)) {
      return false;
    }

    let preview = aGrip.preview = {
      kind: "DOMNode",
      nodeType: aRawObj.nodeType,
      nodeName: aRawObj.nodeName,
    };

    if (aRawObj instanceof Ci.nsIDOMDocument && aRawObj.location) {
      preview.location = threadActor.createValueGrip(aRawObj.location.href);
    } else if (aRawObj instanceof Ci.nsIDOMDocumentFragment) {
      preview.childNodesLength = aRawObj.childNodes.length;

      if (threadActor._gripDepth < 2) {
        preview.childNodes = [];
        for (let node of aRawObj.childNodes) {
          let actor = threadActor.createValueGrip(obj.makeDebuggeeValue(node));
          preview.childNodes.push(actor);
          if (preview.childNodes.length == OBJECT_PREVIEW_MAX_ITEMS) {
            break;
          }
        }
      }
    } else if (aRawObj instanceof Ci.nsIDOMElement) {
      // Add preview for DOM element attributes.
      if (aRawObj instanceof Ci.nsIDOMHTMLElement) {
        preview.nodeName = preview.nodeName.toLowerCase();
      }

      let i = 0;
      preview.attributes = {};
      preview.attributesLength = aRawObj.attributes.length;
      for (let attr of aRawObj.attributes) {
        preview.attributes[attr.nodeName] = threadActor.createValueGrip(attr.value);
        if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }
    } else if (aRawObj instanceof Ci.nsIDOMAttr) {
      preview.value = threadActor.createValueGrip(aRawObj.value);
    } else if (aRawObj instanceof Ci.nsIDOMText ||
               aRawObj instanceof Ci.nsIDOMComment) {
      preview.textContent = threadActor.createValueGrip(aRawObj.textContent);
    }

    return true;
  }, // DOMNode

  function DOMEvent({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMEvent)) {
      return false;
    }

    let preview = aGrip.preview = {
      kind: "DOMEvent",
      type: aRawObj.type,
      properties: Object.create(null),
    };

    if (threadActor._gripDepth < 2) {
      let target = obj.makeDebuggeeValue(aRawObj.target);
      preview.target = threadActor.createValueGrip(target);
    }

    let props = [];
    if (aRawObj instanceof Ci.nsIDOMMouseEvent) {
      props.push("buttons", "clientX", "clientY", "layerX", "layerY");
    } else if (aRawObj instanceof Ci.nsIDOMKeyEvent) {
      let modifiers = [];
      if (aRawObj.altKey) {
        modifiers.push("Alt");
      }
      if (aRawObj.ctrlKey) {
        modifiers.push("Control");
      }
      if (aRawObj.metaKey) {
        modifiers.push("Meta");
      }
      if (aRawObj.shiftKey) {
        modifiers.push("Shift");
      }
      preview.eventKind = "key";
      preview.modifiers = modifiers;

      props.push("key", "charCode", "keyCode");
    } else if (aRawObj instanceof Ci.nsIDOMTransitionEvent) {
      props.push("propertyName", "pseudoElement");
    } else if (aRawObj instanceof Ci.nsIDOMAnimationEvent) {
      props.push("animationName", "pseudoElement");
    } else if (aRawObj instanceof Ci.nsIDOMClipboardEvent) {
      props.push("clipboardData");
    }

    // Add event-specific properties.
    for (let prop of props) {
      let value = aRawObj[prop];
      if (value && (typeof value == "object" || typeof value == "function")) {
        // Skip properties pointing to objects.
        if (threadActor._gripDepth > 1) {
          continue;
        }
        value = obj.makeDebuggeeValue(value);
      }
      preview.properties[prop] = threadActor.createValueGrip(value);
    }

    // Add any properties we find on the event object.
    if (!props.length) {
      let i = 0;
      for (let prop in aRawObj) {
        let value = aRawObj[prop];
        if (prop == "target" || prop == "type" || value === null ||
            typeof value == "function") {
          continue;
        }
        if (value && typeof value == "object") {
          if (threadActor._gripDepth > 1) {
            continue;
          }
          value = obj.makeDebuggeeValue(value);
        }
        preview.properties[prop] = threadActor.createValueGrip(value);
        if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
          break;
        }
      }
    }

    return true;
  }, // DOMEvent

  function DOMException({obj, threadActor}, aGrip, aRawObj) {
    if (isWorker || !aRawObj || !(aRawObj instanceof Ci.nsIDOMDOMException)) {
      return false;
    }

    aGrip.preview = {
      kind: "DOMException",
      name: threadActor.createValueGrip(aRawObj.name),
      message: threadActor.createValueGrip(aRawObj.message),
      code: threadActor.createValueGrip(aRawObj.code),
      result: threadActor.createValueGrip(aRawObj.result),
      filename: threadActor.createValueGrip(aRawObj.filename),
      lineNumber: threadActor.createValueGrip(aRawObj.lineNumber),
      columnNumber: threadActor.createValueGrip(aRawObj.columnNumber),
    };

    return true;
  },

  function GenericObject(aObjectActor, aGrip) {
    let {obj, threadActor} = aObjectActor;
    if (aGrip.preview || aGrip.displayString || threadActor._gripDepth > 1) {
      return false;
    }

    let i = 0, names = [];
    let preview = aGrip.preview = {
      kind: "Object",
      ownProperties: Object.create(null),
    };

    try {
      names = obj.getOwnPropertyNames();
    } catch (ex) {
      // Calling getOwnPropertyNames() on some wrapped native prototypes is not
      // allowed: "cannot modify properties of a WrappedNative". See bug 952093.
    }

    preview.ownPropertiesLength = names.length;

    for (let name of names) {
      let desc = aObjectActor._propertyDescriptor(name, true);
      if (!desc) {
        continue;
      }

      preview.ownProperties[name] = desc;
      if (++i == OBJECT_PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    if (i < OBJECT_PREVIEW_MAX_ITEMS) {
      preview.safeGetterValues = aObjectActor.
                                 _findSafeGetterValues(preview.ownProperties,
                                                       OBJECT_PREVIEW_MAX_ITEMS - i);
    }

    return true;
  }, // GenericObject
]; // DebuggerServer.ObjectActorPreviewers.Object

/**
 * Creates a pause-scoped actor for the specified object.
 * @see ObjectActor
 */
function PauseScopedObjectActor()
{
  ObjectActor.apply(this, arguments);
}

PauseScopedObjectActor.prototype = Object.create(PauseScopedActor.prototype);

update(PauseScopedObjectActor.prototype, ObjectActor.prototype);

update(PauseScopedObjectActor.prototype, {
  constructor: PauseScopedObjectActor,
  actorPrefix: "pausedobj",

  onOwnPropertyNames:
    PauseScopedActor.withPaused(ObjectActor.prototype.onOwnPropertyNames),

  onPrototypeAndProperties:
    PauseScopedActor.withPaused(ObjectActor.prototype.onPrototypeAndProperties),

  onPrototype: PauseScopedActor.withPaused(ObjectActor.prototype.onPrototype),
  onProperty: PauseScopedActor.withPaused(ObjectActor.prototype.onProperty),
  onDecompile: PauseScopedActor.withPaused(ObjectActor.prototype.onDecompile),

  onDisplayString:
    PauseScopedActor.withPaused(ObjectActor.prototype.onDisplayString),

  onParameterNames:
    PauseScopedActor.withPaused(ObjectActor.prototype.onParameterNames),

  /**
   * Handle a protocol request to promote a pause-lifetime grip to a
   * thread-lifetime grip.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onThreadGrip: PauseScopedActor.withPaused(function (aRequest) {
    this.threadActor.threadObjectGrip(this);
    return {};
  }),

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onRelease: PauseScopedActor.withPaused(function (aRequest) {
    if (this.registeredPool !== this.threadActor.threadLifetimePool) {
      return { error: "notReleasable",
               message: "Only thread-lifetime actors can be released." };
    }

    this.release();
    return {};
  }),
});

update(PauseScopedObjectActor.prototype.requestTypes, {
  "threadGrip": PauseScopedObjectActor.prototype.onThreadGrip,
});


/**
 * Creates an actor for the specied "very long" string. "Very long" is specified
 * at the server's discretion.
 *
 * @param aString String
 *        The string.
 */
function LongStringActor(aString)
{
  this.string = aString;
  this.stringLength = aString.length;
}

LongStringActor.prototype = {

  actorPrefix: "longString",

  disconnect: function () {
    // Because longStringActors is not a weak map, we won't automatically leave
    // it so we need to manually leave on disconnect so that we don't leak
    // memory.
    if (this.registeredPool && this.registeredPool.longStringActors) {
      delete this.registeredPool.longStringActors[this.actorID];
    }
  },

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function () {
    return {
      "type": "longString",
      "initial": this.string.substring(
        0, DebuggerServer.LONG_STRING_INITIAL_LENGTH),
      "length": this.stringLength,
      "actor": this.actorID
    };
  },

  /**
   * Handle a request to extract part of this actor's string.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onSubstring: function (aRequest) {
    return {
      "from": this.actorID,
      "substring": this.string.substring(aRequest.start, aRequest.end)
    };
  },

  /**
   * Handle a request to release this LongStringActor instance.
   */
  onRelease: function () {
    // TODO: also check if registeredPool === threadActor.threadLifetimePool
    // when the web console moves aray from manually releasing pause-scoped
    // actors.
    if (this.registeredPool.longStringActors) {
      delete this.registeredPool.longStringActors[this.actorID];
    }
    this.registeredPool.removeActor(this);
    return {};
  },
};

LongStringActor.prototype.requestTypes = {
  "substring": LongStringActor.prototype.onSubstring,
  "release": LongStringActor.prototype.onRelease
};

exports.LongStringActor = LongStringActor;

/**
 * Creates an actor for the specified stack frame.
 *
 * @param aFrame Debugger.Frame
 *        The debuggee frame.
 * @param aThreadActor ThreadActor
 *        The parent thread actor for this frame.
 */
function FrameActor(aFrame, aThreadActor)
{
  this.frame = aFrame;
  this.threadActor = aThreadActor;
}

FrameActor.prototype = {
  actorPrefix: "frame",

  /**
   * A pool that contains frame-lifetime objects, like the environment.
   */
  _frameLifetimePool: null,
  get frameLifetimePool() {
    if (!this._frameLifetimePool) {
      this._frameLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._frameLifetimePool);
    }
    return this._frameLifetimePool;
  },

  /**
   * Finalization handler that is called when the actor is being evicted from
   * the pool.
   */
  disconnect: function () {
    this.conn.removeActorPool(this._frameLifetimePool);
    this._frameLifetimePool = null;
  },

  /**
   * Returns a frame form for use in a protocol message.
   */
  form: function () {
    let threadActor = this.threadActor;
    let form = { actor: this.actorID,
                 type: this.frame.type };
    if (this.frame.type === "call") {
      form.callee = threadActor.createValueGrip(this.frame.callee);
    }

    if (this.frame.environment) {
      let envActor = threadActor.createEnvironmentActor(
        this.frame.environment,
        this.frameLifetimePool
      );
      form.environment = envActor.form();
    }
    form.this = threadActor.createValueGrip(this.frame.this);
    form.arguments = this._args();
    if (this.frame.script) {
      var loc = this.threadActor.sources.getFrameLocation(this.frame);
      form.where = {
        source: loc.sourceActor.form(),
        line: loc.line,
        column: loc.column
      };
    }

    if (!this.frame.older) {
      form.oldest = true;
    }

    return form;
  },

  _args: function () {
    if (!this.frame.arguments) {
      return [];
    }

    return [this.threadActor.createValueGrip(arg)
            for each (arg in this.frame.arguments)];
  },

  /**
   * Handle a protocol request to pop this frame from the stack.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPop: function (aRequest) {
    // TODO: remove this when Debugger.Frame.prototype.pop is implemented
    if (typeof this.frame.pop != "function") {
      return { error: "notImplemented",
               message: "Popping frames is not yet implemented." };
    }

    while (this.frame != this.threadActor.dbg.getNewestFrame()) {
      this.threadActor.dbg.getNewestFrame().pop();
    }
    this.frame.pop(aRequest.completionValue);

    // TODO: return the watches property when frame pop watch actors are
    // implemented.
    return { from: this.actorID };
  }
};

FrameActor.prototype.requestTypes = {
  "pop": FrameActor.prototype.onPop,
};


/**
 * Creates a BreakpointActor. BreakpointActors exist for the lifetime of their
 * containing thread and are responsible for deleting breakpoints, handling
 * breakpoint hits and associating breakpoints with scripts.
 *
 * @param ThreadActor aThreadActor
 *        The parent thread actor that contains this breakpoint.
 * @param object aOriginalLocation
 *        An object with the following properties:
 *        - sourceActor: A SourceActor that represents the source
 *        - line: the specified line
 *        - column: the specified column
 * @param object aGeneratedLocation
 *        An object with the following properties:
 *        - sourceActor: A SourceActor that represents the source
 *        - line: the specified line
 *        - column: the specified column
 * @param string aCondition
 *        Optional. A condition which, when false, will cause the breakpoint to
 *        be skipped.
 */
function BreakpointActor(aThreadActor, aOriginalLocation, aGeneratedLocation, aCondition)
{
  // The set of Debugger.Script instances that this breakpoint has been set
  // upon.
  this.scripts = new Set();

  this.threadActor = aThreadActor;
  this.originalLocation = aOriginalLocation;
  this.generatedLocation = aGeneratedLocation;
  this.condition = aCondition;
}

BreakpointActor.prototype = {
  actorPrefix: "breakpoint",
  condition: null,

  hasScript: function (aScript) {
    return this.scripts.has(aScript);
  },

  /**
   * Called when this same breakpoint is added to another Debugger.Script
   * instance.
   *
   * @param aScript Debugger.Script
   *        The new source script on which the breakpoint has been set.
   * @param ThreadActor aThreadActor
   *        The parent thread actor that contains this breakpoint.
   */
  addScript: function (aScript) {
    this.scripts.add(aScript);
  },

  /**
   * Remove the breakpoints from associated scripts and clear the script cache.
   */
  removeScripts: function () {
    for (let script of this.scripts) {
      script.clearBreakpoint(this);
    }
    this.scripts.clear();
  },

  /**
   * Check if this breakpoint has a condition that doesn't error and
   * evaluates to true in aFrame
   *
   * @param aFrame Debugger.Frame
   *        The frame to evaluate the condition in
   */
  isValidCondition: function(aFrame) {
    if (!this.condition) {
      return true;
    }
    var res = aFrame.eval(this.condition);
    return res.return;
  },

  /**
   * A function that the engine calls when a breakpoint has been hit.
   *
   * @param aFrame Debugger.Frame
   *        The stack frame that contained the breakpoint.
   */
  hit: function (aFrame) {
    // Don't pause if we are currently stepping (in or over) or the frame is
    // black-boxed.
    let loc = this.threadActor.sources.getFrameLocation(aFrame);
    let { sourceActor } = this.threadActor.synchronize(
      this.threadActor.sources.getOriginalLocation(loc));
    let url = sourceActor.url;

    if (this.threadActor.sources.isBlackBoxed(url)
        || aFrame.onStep
        || !this.isValidCondition(aFrame)) {
      return undefined;
    }

    let reason = {};
    if (this.threadActor._hiddenBreakpoints.has(this.actorID)) {
      reason.type = "pauseOnDOMEvents";
    } else {
      reason.type = "breakpoint";
      // TODO: add the rest of the breakpoints on that line (bug 676602).
      reason.actors = [ this.actorID ];
    }
    return this.threadActor._pauseAndRespond(aFrame, reason);
  },

  /**
   * Handle a protocol request to remove this breakpoint.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDelete: function (aRequest) {
    // Remove from the breakpoint store.
    if (this.generatedLocation) {
      this.threadActor.breakpointActorMap.deleteActor(this.generatedLocation);
    }
    this.threadActor.threadLifetimePool.removeActor(this);
    // Remove the actual breakpoint from the associated scripts.
    this.removeScripts();
    return { from: this.actorID };
  }
};

BreakpointActor.prototype.requestTypes = {
  "delete": BreakpointActor.prototype.onDelete
};

/**
 * Creates an EnvironmentActor. EnvironmentActors are responsible for listing
 * the bindings introduced by a lexical environment and assigning new values to
 * those identifier bindings.
 *
 * @param Debugger.Environment aEnvironment
 *        The lexical environment that will be used to create the actor.
 * @param ThreadActor aThreadActor
 *        The parent thread actor that contains this environment.
 */
function EnvironmentActor(aEnvironment, aThreadActor)
{
  this.obj = aEnvironment;
  this.threadActor = aThreadActor;
}

EnvironmentActor.prototype = {
  actorPrefix: "environment",

  /**
   * Return an environment form for use in a protocol message.
   */
  form: function () {
    let form = { actor: this.actorID };

    // What is this environment's type?
    if (this.obj.type == "declarative") {
      form.type = this.obj.callee ? "function" : "block";
    } else {
      form.type = this.obj.type;
    }

    // Does this environment have a parent?
    if (this.obj.parent) {
      form.parent = (this.threadActor
                     .createEnvironmentActor(this.obj.parent,
                                             this.registeredPool)
                     .form());
    }

    // Does this environment reflect the properties of an object as variables?
    if (this.obj.type == "object" || this.obj.type == "with") {
      form.object = this.threadActor.createValueGrip(this.obj.object);
    }

    // Is this the environment created for a function call?
    if (this.obj.callee) {
      form.function = this.threadActor.createValueGrip(this.obj.callee);
    }

    // Shall we list this environment's bindings?
    if (this.obj.type == "declarative") {
      form.bindings = this._bindings();
    }

    return form;
  },

  /**
   * Return the identifier bindings object as required by the remote protocol
   * specification.
   */
  _bindings: function () {
    let bindings = { arguments: [], variables: {} };

    // TODO: this part should be removed in favor of the commented-out part
    // below when getVariableDescriptor lands (bug 725815).
    if (typeof this.obj.getVariable != "function") {
    //if (typeof this.obj.getVariableDescriptor != "function") {
      return bindings;
    }

    let parameterNames;
    if (this.obj.callee) {
      parameterNames = this.obj.callee.parameterNames;
    }
    for each (let name in parameterNames) {
      let arg = {};
      let value = this.obj.getVariable(name);

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands (bug 725815).
      let desc = {
        value: value,
        configurable: false,
        writable: !(value && value.optimizedOut),
        enumerable: true
      };

      // let desc = this.obj.getVariableDescriptor(name);
      let descForm = {
        enumerable: true,
        configurable: desc.configurable
      };
      if ("value" in desc) {
        descForm.value = this.threadActor.createValueGrip(desc.value);
        descForm.writable = desc.writable;
      } else {
        descForm.get = this.threadActor.createValueGrip(desc.get);
        descForm.set = this.threadActor.createValueGrip(desc.set);
      }
      arg[name] = descForm;
      bindings.arguments.push(arg);
    }

    for each (let name in this.obj.names()) {
      if (bindings.arguments.some(function exists(element) {
                                    return !!element[name];
                                  })) {
        continue;
      }

      let value = this.obj.getVariable(name);

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands.
      let desc = {
        value: value,
        configurable: false,
        writable: !(value &&
                    (value.optimizedOut ||
                     value.uninitialized ||
                     value.missingArguments)),
        enumerable: true
      };

      //let desc = this.obj.getVariableDescriptor(name);
      let descForm = {
        enumerable: true,
        configurable: desc.configurable
      };
      if ("value" in desc) {
        descForm.value = this.threadActor.createValueGrip(desc.value);
        descForm.writable = desc.writable;
      } else {
        descForm.get = this.threadActor.createValueGrip(desc.get || undefined);
        descForm.set = this.threadActor.createValueGrip(desc.set || undefined);
      }
      bindings.variables[name] = descForm;
    }

    return bindings;
  },

  /**
   * Handle a protocol request to change the value of a variable bound in this
   * lexical environment.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onAssign: function (aRequest) {
    // TODO: enable the commented-out part when getVariableDescriptor lands
    // (bug 725815).
    /*let desc = this.obj.getVariableDescriptor(aRequest.name);

    if (!desc.writable) {
      return { error: "immutableBinding",
               message: "Changing the value of an immutable binding is not " +
                        "allowed" };
    }*/

    try {
      this.obj.setVariable(aRequest.name, aRequest.value);
    } catch (e if e instanceof Debugger.DebuggeeWouldRun) {
        return { error: "threadWouldRun",
                 cause: e.cause ? e.cause : "setter",
                 message: "Assigning a value would cause the debuggee to run" };
    }
    return { from: this.actorID };
  },

  /**
   * Handle a protocol request to fully enumerate the bindings introduced by the
   * lexical environment.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onBindings: function (aRequest) {
    return { from: this.actorID,
             bindings: this._bindings() };
  }
};

EnvironmentActor.prototype.requestTypes = {
  "assign": EnvironmentActor.prototype.onAssign,
  "bindings": EnvironmentActor.prototype.onBindings
};

exports.EnvironmentActor = EnvironmentActor;

function hackDebugger(Debugger) {
  // TODO: Improve native code instead of hacking on top of it

  /**
   * Override the toString method in order to get more meaningful script output
   * for debugging the debugger.
   */
  Debugger.Script.prototype.toString = function() {
    let output = "";
    if (this.url) {
      output += this.url;
    }
    if (typeof this.staticLevel != "undefined") {
      output += ":L" + this.staticLevel;
    }
    if (typeof this.startLine != "undefined") {
      output += ":" + this.startLine;
      if (this.lineCount && this.lineCount > 1) {
        output += "-" + (this.startLine + this.lineCount - 1);
      }
    }
    if (typeof this.startLine != "undefined") {
      output += ":" + this.startLine;
      if (this.lineCount && this.lineCount > 1) {
        output += "-" + (this.startLine + this.lineCount - 1);
      }
    }
    if (this.strictMode) {
      output += ":strict";
    }
    return output;
  };

  /**
   * Helper property for quickly getting to the line number a stack frame is
   * currently paused at.
   */
  Object.defineProperty(Debugger.Frame.prototype, "line", {
    configurable: true,
    get: function() {
      if (this.script) {
        return this.script.getOffsetLine(this.offset);
      } else {
        return null;
      }
    }
  });
}


/**
 * Creates an actor for handling chrome debugging. ChromeDebuggerActor is a
 * thin wrapper over ThreadActor, slightly changing some of its behavior.
 *
 * @param aConnection object
 *        The DebuggerServerConnection with which this ChromeDebuggerActor
 *        is associated. (Currently unused, but required to make this
 *        constructor usable with addGlobalActor.)
 *
 * @param aParent object
 *        This actor's parent actor. See ThreadActor for a list of expected
 *        properties.
 */
function ChromeDebuggerActor(aConnection, aParent)
{
  ThreadActor.call(this, aParent);
}

ChromeDebuggerActor.prototype = Object.create(ThreadActor.prototype);

update(ChromeDebuggerActor.prototype, {
  constructor: ChromeDebuggerActor,

  // A constant prefix that will be used to form the actor ID by the server.
  actorPrefix: "chromeDebugger"
});

exports.ChromeDebuggerActor = ChromeDebuggerActor;

/**
 * Creates an actor for handling add-on debugging. AddonThreadActor is
 * a thin wrapper over ThreadActor.
 *
 * @param aConnection object
 *        The DebuggerServerConnection with which this AddonThreadActor
 *        is associated. (Currently unused, but required to make this
 *        constructor usable with addGlobalActor.)
 *
 * @param aParent object
 *        This actor's parent actor. See ThreadActor for a list of expected
 *        properties.
 */
function AddonThreadActor(aConnect, aParent) {
  ThreadActor.call(this, aParent);
}

AddonThreadActor.prototype = Object.create(ThreadActor.prototype);

update(AddonThreadActor.prototype, {
  constructor: AddonThreadActor,

  // A constant prefix that will be used to form the actor ID by the server.
  actorPrefix: "addonThread",

  /**
   * Override the eligibility check for scripts and sources to make
   * sure every script and source with a URL is stored when debugging
   * add-ons.
   */
  _allowSource: function(aSource) {
    let url = aSource.url;

    if (isHiddenSource(aSource)) {
      return false;
    }

    // XPIProvider.jsm evals some code in every add-on's bootstrap.js. Hide it.
    if (url === "resource://gre/modules/addons/XPIProvider.jsm") {
      return false;
    }

    return true;
  },

});

exports.AddonThreadActor = AddonThreadActor;

/**
 * Manages the sources for a thread. Handles source maps, locations in the
 * sources, etc for ThreadActors.
 */
function ThreadSources(aThreadActor, aOptions, aAllowPredicate,
                       aOnNewSource) {
  this._thread = aThreadActor;
  this._useSourceMaps = aOptions.useSourceMaps;
  this._autoBlackBox = aOptions.autoBlackBox;
  this._allow = aAllowPredicate;
  this._onNewSource = DevToolsUtils.makeInfallible(
    aOnNewSource,
    "ThreadSources.prototype._onNewSource"
  );
  this._anonSourceMapId = 1;

  // generated Debugger.Source -> promise of SourceMapConsumer
  this._sourceMaps = new Map();
  // sourceMapURL -> promise of SourceMapConsumer
  this._sourceMapCache = Object.create(null);
  // Debugger.Source -> SourceActor
  this._sourceActors = new Map();
  // url -> SourceActor
  this._sourceMappedSourceActors = Object.create(null);
}

/**
 * Matches strings of the form "foo.min.js" or "foo-min.js", etc. If the regular
 * expression matches, we can be fairly sure that the source is minified, and
 * treat it as such.
 */
const MINIFIED_SOURCE_REGEXP = /\bmin\.js$/;

ThreadSources.prototype = {
  /**
   * Return the source actor representing the `source` (or
   * `originalUrl`), creating one if none exists already. May return
   * null if the source is disallowed.
   *
   * @param Debugger.Source source
   *        The source to make an actor for
   * @param String originalUrl
   *        The original source URL of a sourcemapped source
   * @param optional Debguger.Source generatedSource
   *        The generated source that introduced this source via source map,
   *        if any.
   * @param optional String contentType
   *        The content type of the source, if immediately available.
   * @returns a SourceActor representing the source or null.
   */
  source: function  ({ source, originalUrl, generatedSource,
              isInlineSource, contentType }) {
    dbg_assert(source || (originalUrl && generatedSource),
               "ThreadSources.prototype.source needs an originalUrl or a source");

    if (source) {
      // If a source is passed, we are creating an actor for a real
      // source, which may or may not be sourcemapped.

      if (!this._allow(source)) {
        return null;
      }

      // It's a hack, but inline HTML scripts each have real sources,
      // but we want to represent all of them as one source as the
      // HTML page. The actor representing this fake HTML source is
      // stored in this array, which always has a URL, so check it
      // first.
      if (source.url in this._sourceMappedSourceActors) {
        return this._sourceMappedSourceActors[source.url];
      }

      if (isInlineSource) {
        // If it's an inline source, the fake HTML source hasn't been
        // created yet (would have returned above), so flip this source
        // into a sourcemapped state by giving it an `originalUrl` which
        // is the HTML url.
        originalUrl = source.url;
        source = null;
      }
      else if (this._sourceActors.has(source)) {
        return this._sourceActors.get(source);
      }
    }
    else if (originalUrl) {
      // Not all "original" scripts are distinctly separate from the
      // generated script. Pretty-printed sources have a sourcemap for
      // themselves, so we need to make sure there a real source
      // doesn't already exist with this URL.
      for (let [source, actor] of this._sourceActors) {
        if (source.url === originalUrl) {
          return actor;
        }
      }

      if (originalUrl in this._sourceMappedSourceActors) {
        return this._sourceMappedSourceActors[originalUrl];
      }
    }

    let actor = new SourceActor({
      thread: this._thread,
      source: source,
      originalUrl: originalUrl,
      generatedSource: generatedSource,
      contentType: contentType
    });

    let sourceActorStore = this._thread.sourceActorStore;
    var id = sourceActorStore.getReusableActorId(source, originalUrl);
    if (id) {
      actor.actorID = id;
    }

    this._thread.threadLifetimePool.addActor(actor);
    sourceActorStore.setReusableActorId(source, originalUrl, actor.actorID);

    if (this._autoBlackBox && this._isMinifiedURL(actor.url)) {
      this.blackBox(actor.url);
    }

    if (source) {
      this._sourceActors.set(source, actor);
    }
    else {
      this._sourceMappedSourceActors[originalUrl] = actor;
    }

    this._emitNewSource(actor);
    return actor;
  },

  _emitNewSource: function(actor) {
    if(!actor.source) {
      // Always notify if we don't have a source because that means
      // it's something that has been sourcemapped, or it represents
      // the HTML file that contains inline sources.
      this._onNewSource(actor);
    }
    else {
      // If sourcemapping is enabled and a source has sourcemaps, we
      // create `SourceActor` instances for both the original and
      // generated sources. The source actors for the generated
      // sources are only for internal use, however; breakpoints are
      // managed by these internal actors. We only want to notify the
      // user of the original sources though, so if the actor has a
      // `Debugger.Source` instance and a valid source map (meaning
      // it's a generated source), don't send the notification.
      this.fetchSourceMap(actor.source).then(map => {
        if(!map) {
          this._onNewSource(actor);
        }
      });
    }
  },

  getSourceActor: function(source) {
    if (source.url in this._sourceMappedSourceActors) {
      return this._sourceMappedSourceActors[source.url];
    }

    if (this._sourceActors.has(source)) {
      return this._sourceActors.get(source);
    }

    throw new Error('getSource: could not find source actor for ' +
                    (source.url || 'source'));
  },

  getSourceActorByURL: function(url) {
    if (url) {
      for (let [source, actor] of this._sourceActors) {
        if (source.url === url) {
          return actor;
        }
      }

      if (url in this._sourceMappedSourceActors) {
        return this._sourceMappedSourceActors[url];
      }
    }

    throw new Error('getSourceByURL: could not find source for ' + url);
  },

  /**
   * Returns true if the URL likely points to a minified resource, false
   * otherwise.
   *
   * @param String aURL
   *        The URL to test.
   * @returns Boolean
   */
  _isMinifiedURL: function (aURL) {
    try {
      let url = Services.io.newURI(aURL, null, null)
                           .QueryInterface(Ci.nsIURL);
      return MINIFIED_SOURCE_REGEXP.test(url.fileName);
    } catch (e) {
      // Not a valid URL so don't try to parse out the filename, just test the
      // whole thing with the minified source regexp.
      return MINIFIED_SOURCE_REGEXP.test(aURL);
    }
  },

  /**
   * Create a source actor representing this source. This ignores
   * source mapping and always returns an actor representing this real
   * source. Use `createSourceActors` if you want to respect source maps.
   *
   * @param Debugger.Source aSource
   *        The source instance to create an actor for.
   * @returns SourceActor
   */
  createNonSourceMappedActor: function (aSource) {
    // Don't use getSourceURL because we don't want to consider the
    // displayURL property if it's an eval source. We only want to
    // consider real URLs, otherwise if there is a URL but it's
    // invalid the code below will not set the content type, and we
    // will later try to fetch the contents of the URL to figure out
    // the content type, but it's a made up URL for eval sources.
    let url = isEvalSource(aSource) ? null : aSource.url;
    let spec = { source: aSource };

    // XXX bug 915433: We can't rely on Debugger.Source.prototype.text
    // if the source is an HTML-embedded <script> tag. Since we don't
    // have an API implemented to detect whether this is the case, we
    // need to be conservative and only treat valid js files as real
    // sources. Otherwise, use the `originalUrl` property to treat it
    // as an HTML source that manages multiple inline sources.
    if (url) {
      try {
        let urlInfo = Services.io.newURI(url, null, null).QueryInterface(Ci.nsIURL);
        if (urlInfo.fileExtension === "html") {
          spec.isInlineSource = true;
        }
        else if (urlInfo.fileExtension === "js") {
          spec.contentType = "text/javascript";
        }
      } catch(ex) {
        // Not a valid URI.

        // bug 1124536: fix getSourceText on scripts associated "javascript:SOURCE" urls
        // (e.g. 'evaluate(sandbox, sourcecode, "javascript:"+sourcecode)' )
        if (url.indexOf("javascript:") === 0) {
          spec.contentType = "text/javascript";
        } 
      }
    }
    else {
      // Assume the content is javascript if there's no URL
      spec.contentType = "text/javascript";
    }

    return this.source(spec);
  },

  /**
   * This is an internal function that returns a promise of an array
   * of source actors representing all the source mapped sources of
   * `aSource`, or `null` if the source is not sourcemapped or
   * sourcemapping is disabled. Users should call `createSourceActors`
   * instead of this.
   *
   * @param Debugger.Source aSource
   *        The source instance to create actors for.
   * @return Promise of an array of source actors
   */
  _createSourceMappedActors: function (aSource) {
    if (!this._useSourceMaps || !aSource.sourceMapURL) {
      return resolve(null);
    }

    return this.fetchSourceMap(aSource)
      .then(map => {
        if (map) {
          return [
            this.source({ originalUrl: s, generatedSource: aSource })
            for (s of map.sources)
          ].filter(isNotNull);
        }
        return null;
      });
  },

  /**
   * Creates the source actors representing the appropriate sources
   * of `aSource`. If sourcemapped, returns actors for all of the original
   * sources, otherwise returns a 1-element array with the actor for
   * `aSource`.
   *
   * @param Debugger.Source aSource
   *        The source instance to create actors for.
   * @param Promise of an array of source actors
   */
  createSourceActors: function(aSource) {
    return this._createSourceMappedActors(aSource).then(actors => {
      let actor = this.createNonSourceMappedActor(aSource);
      return (actors || [actor]).filter(isNotNull);
    });
  },

  /**
   * Return a promise of a SourceMapConsumer for the source map for
   * `aSource`; if we already have such a promise extant, return that.
   * This will fetch the source map if we don't have a cached object
   * and source maps are enabled (see `_fetchSourceMap`).
   *
   * @param Debugger.Source aSource
   *        The source instance to get sourcemaps for.
   * @return Promise of a SourceMapConsumer
   */
  fetchSourceMap: function (aSource) {
    if (this._sourceMaps.has(aSource)) {
      return this._sourceMaps.get(aSource);
    }
    else if (!aSource || !aSource.sourceMapURL) {
      return resolve(null);
    }

    let sourceMapURL = aSource.sourceMapURL;
    if (aSource.url) {
      sourceMapURL = this._normalize(sourceMapURL, aSource.url);
    }
    let result = this._fetchSourceMap(sourceMapURL, aSource.url);

    // The promises in `_sourceMaps` must be the exact same instances
    // as returned by `_fetchSourceMap` for `clearSourceMapCache` to work.
    this._sourceMaps.set(aSource, result);
    return result;
  },

  /**
   * Return a promise of a SourceMapConsumer for the source map for
   * `aSource`. The resolved result may be null if the source does not
   * have a source map or source maps are disabled.
   */
  getSourceMap: function(aSource) {
    return resolve(this._sourceMaps.get(aSource));
  },

  /**
   * Set a SourceMapConsumer for the source map for
   * |aSource|.
   */
  setSourceMap: function(aSource, aMap) {
    this._sourceMaps.set(aSource, resolve(aMap));
  },

  /**
   * Return a promise of a SourceMapConsumer for the source map located at
   * |aAbsSourceMapURL|, which must be absolute. If there is already such a
   * promise extant, return it. This will not fetch if source maps are
   * disabled.
   *
   * @param string aAbsSourceMapURL
   *        The source map URL, in absolute form, not relative.
   * @param string aScriptURL
   *        When the source map URL is a data URI, there is no sourceRoot on the
   *        source map, and the source map's sources are relative, we resolve
   *        them from aScriptURL.
   */
  _fetchSourceMap: function (aAbsSourceMapURL, aSourceURL) {
    if (this._sourceMapCache[aAbsSourceMapURL]) {
      return this._sourceMapCache[aAbsSourceMapURL];
    }
    else if (!this._useSourceMaps) {
      return resolve(null);
    }

    let fetching = fetch(aAbsSourceMapURL, { loadFromCache: false })
      .then(({ content }) => {
        let map = new SourceMapConsumer(content);
        this._setSourceMapRoot(map, aAbsSourceMapURL, aSourceURL);
        return map;
      })
      .then(null, error => {
        if (!DevToolsUtils.reportingDisabled) {
          DevToolsUtils.reportException("ThreadSources.prototype._fetchSourceMap", error);
        }
        return null;
      });
    this._sourceMapCache[aAbsSourceMapURL] = fetching;
    return fetching;
  },

  /**
   * Sets the source map's sourceRoot to be relative to the source map url.
   */
  _setSourceMapRoot: function (aSourceMap, aAbsSourceMapURL, aScriptURL) {
    const base = this._dirname(
      aAbsSourceMapURL.indexOf("data:") === 0
        ? aScriptURL
        : aAbsSourceMapURL);
    aSourceMap.sourceRoot = aSourceMap.sourceRoot
      ? this._normalize(aSourceMap.sourceRoot, base)
      : base;
  },

  _dirname: function (aPath) {
    return Services.io.newURI(
      ".", null, Services.io.newURI(aPath, null, null)).spec;
  },

  /**
   * Clears the source map cache. Source maps are cached by URL so
   * they can be reused across separate Debugger instances (once in
   * this cache, they will never be reparsed again). They are
   * also cached by Debugger.Source objects for usefulness. By default
   * this just removes the Debugger.Source cache, but you can remove
   * the lower-level URL cache with the `hard` option.
   *
   * @param aSourceMapURL string
   *        The source map URL to uncache
   * @param opts object
   *        An object with the following properties:
   *        - hard: Also remove the lower-level URL cache, which will
   *          make us completely forget about the source map.
   */
  clearSourceMapCache: function(aSourceMapURL, opts = { hard: false }) {
    let oldSm = this._sourceMapCache[aSourceMapURL];

    if (opts.hard) {
      delete this._sourceMapCache[aSourceMapURL];
    }

    if (oldSm) {
      // Clear out the current cache so all sources will get the new one
      for (let [source, sm] of this._sourceMaps.entries()) {
        if (sm === oldSm) {
          this._sourceMaps.delete(source);
        }
      }
    }
  },

  /*
   * Forcefully change the source map of a source, changing the
   * sourceMapURL and installing the source map in the cache. This is
   * necessary to expose changes across Debugger instances
   * (pretty-printing is the use case). Generate a random url if one
   * isn't specified, allowing you to set "anonymous" source maps.
   *
   * @param aSource Debugger.Source
   *        The source to change the sourceMapURL property
   * @param aUrl string
   *        The source map URL (optional)
   * @param aMap SourceMapConsumer
   *        The source map instance
   */
  setSourceMapHard: function(aSource, aUrl, aMap) {
    let url = aUrl;
    if (!url) {
      // This is a littly hacky, but we want to forcefully set a
      // sourcemap regardless of sourcemap settings. We want to
      // literally change the sourceMapURL so that all debuggers will
      // get this and pretty-printing will Just Work (Debugger.Source
      // instances are per-debugger, so we can't key off that). To
      // avoid tons of work serializing the sourcemap into a data url,
      // just make a fake URL and stick the sourcemap there.
      url = "internal://sourcemap" + (this._anonSourceMapId++) + '/';
    }
    aSource.sourceMapURL = url;

    // Forcefully set the sourcemap cache. This will be used even if
    // sourcemaps are disabled.
    this._sourceMapCache[url] = resolve(aMap);
  },

  /**
   * Return the non-source-mapped location of the given Debugger.Frame. If the
   * frame does not have a script, the location's properties are all null.
   *
   * @param Debugger.Frame aFrame
   *        The frame whose location we are getting.
   * @returns Object
   *          Returns an object of the form { source, line, column }
   */
  getFrameLocation: function (aFrame) {
    if (!aFrame || !aFrame.script) {
      return { sourceActor: null, line: null, column: null };
    }
    return {
      sourceActor: this.createNonSourceMappedActor(aFrame.script.source),
      line: aFrame.script.getOffsetLine(aFrame.offset),
      column: getOffsetColumn(aFrame.offset, aFrame.script)
    }
  },

  /**
   * Returns a promise of the location in the original source if the source is
   * source mapped, otherwise a promise of the same location. This can
   * be called with a source from *any* Debugger instance and we make
   * sure to that it works properly, reusing source maps if already
   * fetched. Use this from any actor that needs sourcemapping.
   */
  getOriginalLocation: function ({ sourceActor, line, column }) {
    let source = sourceActor.source;
    let url = source ? source.url : sourceActor._originalUrl;

    // In certain scenarios the source map may have not been fetched
    // yet (or at least tied to this Debugger.Source instance), so use
    // `fetchSourceMap` instead of `getSourceMap`. This allows this
    // function to be called from anywere (across debuggers) and it
    // should just automatically work.
    return this.fetchSourceMap(source).then(sm => {
      if (sm) {
        let {
          source: sourceUrl,
          line: sourceLine,
          column: sourceCol,
          name: sourceName
        } = sm.originalPositionFor({
          line: line,
          column: column == null ? Infinity : column
        });

        return {
          // Since the `Debugger.Source` instance may come from a
          // different `Debugger` instance (any actor can call this
          // method), we can't rely on any of the source discovery
          // setup (`_discoverSources`, etc) to have been run yet. So
          // we have to assume that the actor may not already exist,
          // and we might need to create it, so use `source` and give
          // it the required parameters for a sourcemapped source.
          sourceActor: (!sourceUrl) ? null : this.source({
            originalUrl: sourceUrl,
            generatedSource: source
          }),
          url: sourceUrl,
          line: sourceLine,
          column: sourceCol,
          name: sourceName
        };
      }

      // No source map
      return resolve({
        sourceActor: sourceActor,
        url: url,
        line: line,
        column: column
      });
    });
  },

  /**
   * Returns a promise of the location in the generated source corresponding to
   * the original source and line given.
   *
   * When we pass a script S representing generated code to `sourceMap`,
   * above, that returns a promise P. The process of resolving P populates
   * the tables this function uses; thus, it won't know that S's original
   * source URLs map to S until P is resolved.
   */
  getGeneratedLocation: function ({ sourceActor, line, column }) {
    // Both original sources and normal sources could have sourcemaps,
    // because normal sources can be pretty-printed which generates a
    // sourcemap for itself. Check both of the source properties to make it work
    // for both kinds of sources.
    let source = sourceActor.generatedSource || sourceActor.source;

    // See comment about `fetchSourceMap` in `getOriginalLocation`.
    return this.fetchSourceMap(source).then(sm => {
      if (sm) {
        let { line: genLine, column: genColumn } = sm.generatedPositionFor({
          source: sourceActor.url,
          line: line,
          column: column == null ? Infinity : column
        });

        return {
          sourceActor: this.createNonSourceMappedActor(source),
          line: genLine,
          column: genColumn
        };
      }

      return resolve({
        sourceActor: sourceActor,
        line: line,
        column: column
      });
    });
  },

  /**
   * Returns true if URL for the given source is black boxed.
   *
   * @param aURL String
   *        The URL of the source which we are checking whether it is black
   *        boxed or not.
   */
  isBlackBoxed: function (aURL) {
    return this._thread.blackBoxedSources.has(aURL);
  },

  /**
   * Add the given source URL to the set of sources that are black boxed.
   *
   * @param aURL String
   *        The URL of the source which we are black boxing.
   */
  blackBox: function (aURL) {
    this._thread.blackBoxedSources.add(aURL);
  },

  /**
   * Remove the given source URL to the set of sources that are black boxed.
   *
   * @param aURL String
   *        The URL of the source which we are no longer black boxing.
   */
  unblackBox: function (aURL) {
    this._thread.blackBoxedSources.delete(aURL);
  },

  /**
   * Returns true if the given URL is pretty printed.
   *
   * @param aURL String
   *        The URL of the source that might be pretty printed.
   */
  isPrettyPrinted: function (aURL) {
    return this._thread.prettyPrintedSources.has(aURL);
  },

  /**
   * Add the given URL to the set of sources that are pretty printed.
   *
   * @param aURL String
   *        The URL of the source to be pretty printed.
   */
  prettyPrint: function (aURL, aIndent) {
    this._thread.prettyPrintedSources.set(aURL, aIndent);
  },

  /**
   * Return the indent the given URL was pretty printed by.
   */
  prettyPrintIndent: function (aURL) {
    return this._thread.prettyPrintedSources.get(aURL);
  },

  /**
   * Remove the given URL from the set of sources that are pretty printed.
   *
   * @param aURL String
   *        The URL of the source that is no longer pretty printed.
   */
  disablePrettyPrint: function (aURL) {
    this._thread.prettyPrintedSources.delete(aURL);
  },

  /**
   * Normalize multiple relative paths towards the base paths on the right.
   */
  _normalize: function (...aURLs) {
    dbg_assert(aURLs.length > 1, "Should have more than 1 URL");
    let base = Services.io.newURI(aURLs.pop(), null, null);
    let url;
    while ((url = aURLs.pop())) {
      base = Services.io.newURI(url, null, base);
    }
    return base.spec;
  },

  iter: function () {
    let actors = Object.keys(this._sourceMappedSourceActors).map(k => {
      return this._sourceMappedSourceActors[k];
    });
    for (let actor of this._sourceActors.values()) {
      if (!this._sourceMaps.has(actor.source)) {
        actors.push(actor);
      }
    }
    return actors;
  }
};

exports.ThreadSources = ThreadSources;

// Utility functions.

/**
 * Checks if a source should never be displayed to the user because
 * it's either internal or we don't support in the UI yet.
 */
function isHiddenSource(aSource) {
  // Ignore the internal Function.prototype script
  return aSource.text === '() {\n}';
}

/**
 * Returns true if its argument is not null.
 */
function isNotNull(aThing) {
  return aThing !== null;
}

/**
 * Report the given error in the error console and to stdout.
 *
 * @param Error aError
 *        The error object you wish to report.
 * @param String aPrefix
 *        An optional prefix for the reported error message.
 */
let oldReportError = reportError;
reportError = function(aError, aPrefix="") {
  dbg_assert(aError instanceof Error, "Must pass Error objects to reportError");
  let msg = aPrefix + aError.message + ":\n" + aError.stack;
  oldReportError(msg);
  dumpn(msg);
}

/**
 * Make a debuggee value for the given object, if needed. Primitive values
 * are left the same.
 *
 * Use case: you have a raw JS object (after unsafe dereference) and you want to
 * send it to the client. In that case you need to use an ObjectActor which
 * requires a debuggee value. The Debugger.Object.prototype.makeDebuggeeValue()
 * method works only for JS objects and functions.
 *
 * @param Debugger.Object obj
 * @param any value
 * @return object
 */
function makeDebuggeeValueIfNeeded(obj, value) {
  if (value && (typeof value == "object" || typeof value == "function")) {
    return obj.makeDebuggeeValue(value);
  }
  return value;
}

function getInnerId(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).
                getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
};

const symbolProtoToString = Symbol.prototype.toString;

function getSymbolName(symbol) {
  const name = symbolProtoToString.call(symbol).slice("Symbol(".length, -1);
  return name || undefined;
}

function isEvalSource(source) {
  let introType = source.introductionType;
  // These are all the sources that are essentially eval-ed (either
  // by calling eval or passing a string to one of these functions).
  return (introType === 'eval' ||
          introType === 'Function' ||
          introType === 'eventHandler' ||
          introType === 'setTimeout' ||
          introType === 'setInterval');
}

function getSourceURL(source) {
  if(isEvalSource(source)) {
    // Eval sources have no urls, but they might have a `displayURL`
    // created with the sourceURL pragma. If the introduction script
    // is a non-eval script, generate an full absolute URL relative to it.

    if(source.displayURL &&
       source.introductionScript &&
       !isEvalSource(source.introductionScript.source)) {
      return joinURI(dirname(source.introductionScript.source.url),
                     source.displayURL);
    }

    return source.displayURL;
  }
  return source.url;
}

/**
 * Find the scripts which contain offsets that are an entry point to the given
 * line.
 *
 * @param Array scripts
 *        The set of Debugger.Scripts to consider.
 * @param Number line
 *        The line we are searching for entry points into.
 * @returns Array of objects of the form { script, offsets } where:
 *          - script is a Debugger.Script
 *          - offsets is an array of offsets that are entry points into the
 *            given line.
 */
function findEntryPointsForLine(scripts, line) {
  const entryPoints = [];
  for (let script of scripts) {
    const offsets = script.getLineOffsets(line);
    if (offsets.length) {
      entryPoints.push({ script, offsets });
    }
  }
  return entryPoints;
}

/**
 * Set breakpoints on all the given entry points with the given
 * BreakpointActor as the handler.
 *
 * @param BreakpointActor actor
 *        The actor handling the breakpoint hits.
 * @param Array entryPoints
 *        An array of objects of the form `{ script, offsets }`.
 */
function setBreakpointOnEntryPoints(threadActor, actor, entryPoints) {
  for (let { script, offsets } of entryPoints) {
    for (let offset of offsets) {
      script.setBreakpoint(offset, actor);
    }
    actor.addScript(script);
  }
}

