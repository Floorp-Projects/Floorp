/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Cc, Ci, Cu, components, ChromeWorker } = require("chrome");
const { ActorPool } = require("devtools/server/actors/common");
const { DebuggerServer } = require("devtools/server/main");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils");
const { dbg_assert, dumpn, update } = DevToolsUtils;
const { SourceMapConsumer, SourceMapGenerator } = require("source-map");
const promise = require("promise");
const Debugger = require("Debugger");
const xpcInspector = require("xpcInspector");
const mapURIToAddonID = require("./utils/map-uri-to-addon-id");

const { defer, resolve, reject, all } = require("devtools/toolkit/deprecated-sync-thenables");
const { CssLogic } = require("devtools/styleinspector/css-logic");

DevToolsUtils.defineLazyGetter(this, "NetUtil", () => {
  return Cu.import("resource://gre/modules/NetUtil.jsm", {}).NetUtil;
});

let TYPED_ARRAY_CLASSES = ["Uint8Array", "Uint8ClampedArray", "Uint16Array",
      "Uint32Array", "Int8Array", "Int16Array", "Int32Array", "Float32Array",
      "Float64Array"];

// Number of items to preview in objects, arrays, maps, sets, lists,
// collections, etc.
let OBJECT_PREVIEW_MAX_ITEMS = 10;

/**
 * BreakpointStore objects keep track of all breakpoints that get set so that we
 * can reset them when the same script is introduced to the thread again (such
 * as after a refresh).
 */
function BreakpointStore() {
  this._size = 0;

  // If we have a whole-line breakpoint set at LINE in URL, then
  //
  //   this._wholeLineBreakpoints[URL][LINE]
  //
  // is an object
  //
  //   { url, line[, actor] }
  //
  // where the `actor` property is optional.
  this._wholeLineBreakpoints = Object.create(null);

  // If we have a breakpoint set at LINE, COLUMN in URL, then
  //
  //   this._breakpoints[URL][LINE][COLUMN]
  //
  // is an object
  //
  //   { url, line, column[, actor] }
  //
  // where the `actor` property is optional.
  this._breakpoints = Object.create(null);
}

BreakpointStore.prototype = {
  _size: null,
  get size() { return this._size; },

  /**
   * Add a breakpoint to the breakpoint store if it doesn't already exist.
   *
   * @param Object aBreakpoint
   *        The breakpoint to be added (not copied). It is an object with the
   *        following properties:
   *          - url
   *          - line
   *          - column (optional; omission implies that the breakpoint is for
   *            the whole line)
   *          - condition (optional)
   *          - actor (optional)
   * @returns Object aBreakpoint
   *          The new or existing breakpoint.
   */
  addBreakpoint: function (aBreakpoint) {
    let { url, line, column } = aBreakpoint;

    if (column != null) {
      if (!this._breakpoints[url]) {
        this._breakpoints[url] = [];
      }
      if (!this._breakpoints[url][line]) {
        this._breakpoints[url][line] = [];
      }
      if (!this._breakpoints[url][line][column]) {
        this._breakpoints[url][line][column] = aBreakpoint;
        this._size++;
      }
      return this._breakpoints[url][line][column];
    } else {
      // Add a breakpoint that breaks on the whole line.
      if (!this._wholeLineBreakpoints[url]) {
        this._wholeLineBreakpoints[url] = [];
      }
      if (!this._wholeLineBreakpoints[url][line]) {
        this._wholeLineBreakpoints[url][line] = aBreakpoint;
        this._size++;
      }
      return this._wholeLineBreakpoints[url][line];
    }
  },

  /**
   * Remove a breakpoint from the breakpoint store.
   *
   * @param Object aBreakpoint
   *        The breakpoint to be removed. It is an object with the following
   *        properties:
   *          - url
   *          - line
   *          - column (optional)
   */
  removeBreakpoint: function ({ url, line, column }) {
    if (column != null) {
      if (this._breakpoints[url]) {
        if (this._breakpoints[url][line]) {
          if (this._breakpoints[url][line][column]) {
            delete this._breakpoints[url][line][column];
            this._size--;

            // If this was the last breakpoint on this line, delete the line from
            // `this._breakpoints[url]` as well. Otherwise `_iterLines` will yield
            // this line even though we no longer have breakpoints on
            // it. Furthermore, we use Object.keys() instead of just checking
            // `this._breakpoints[url].length` directly, because deleting
            // properties from sparse arrays doesn't update the `length` property
            // like adding them does.
            if (Object.keys(this._breakpoints[url][line]).length === 0) {
              delete this._breakpoints[url][line];
            }
          }
        }
      }
    } else {
      if (this._wholeLineBreakpoints[url]) {
        if (this._wholeLineBreakpoints[url][line]) {
          delete this._wholeLineBreakpoints[url][line];
          this._size--;
        }
      }
    }
  },

  /**
   * Get a breakpoint from the breakpoint store. Will throw an error if the
   * breakpoint is not found.
   *
   * @param Object aLocation
   *        The location of the breakpoint you are retrieving. It is an object
   *        with the following properties:
   *          - url
   *          - line
   *          - column (optional)
   */
  getBreakpoint: function (aLocation) {
    let { url, line, column } = aLocation;
    dbg_assert(url != null);
    dbg_assert(line != null);

    var foundBreakpoint = this.hasBreakpoint(aLocation);
    if (foundBreakpoint == null) {
      throw new Error("No breakpoint at url = " + url
          + ", line = " + line
          + ", column = " + column);
    }

    return foundBreakpoint;
  },

  /**
   * Checks if the breakpoint store has a requested breakpoint.
   *
   * @param Object aLocation
   *        The location of the breakpoint you are retrieving. It is an object
   *        with the following properties:
   *          - url
   *          - line
   *          - column (optional)
   * @returns The stored breakpoint if it exists, null otherwise.
   */
  hasBreakpoint: function (aLocation) {
    let { url, line, column } = aLocation;
    dbg_assert(url != null);
    dbg_assert(line != null);
    for (let bp of this.findBreakpoints(aLocation)) {
      // We will get whole line breakpoints before individual columns, so just
      // return the first one and if they didn't specify a column then they will
      // get the whole line breakpoint, and otherwise we will find the correct
      // one.
      return bp;
    }

    return null;
  },

  /**
   * Iterate over the breakpoints in this breakpoint store. You can optionally
   * provide search parameters to filter the set of breakpoints down to those
   * that match your parameters.
   *
   * @param Object aSearchParams
   *        Optional. An object with the following properties:
   *          - url
   *          - line (optional; requires the url property)
   *          - column (optional; requires the line property)
   */
  findBreakpoints: function* (aSearchParams={}) {
    if (aSearchParams.column != null) {
      dbg_assert(aSearchParams.line != null);
    }
    if (aSearchParams.line != null) {
      dbg_assert(aSearchParams.url != null);
    }

    for (let url of this._iterUrls(aSearchParams.url)) {
      for (let line of this._iterLines(url, aSearchParams.line)) {
        // Always yield whole line breakpoints first. See comment in
        // |BreakpointStore.prototype.hasBreakpoint|.
        if (aSearchParams.column == null
            && this._wholeLineBreakpoints[url]
            && this._wholeLineBreakpoints[url][line]) {
          yield this._wholeLineBreakpoints[url][line];
        }
        for (let column of this._iterColumns(url, line, aSearchParams.column)) {
          yield this._breakpoints[url][line][column];
        }
      }
    }
  },

  _iterUrls: function* (aUrl) {
    if (aUrl) {
      if (this._breakpoints[aUrl] || this._wholeLineBreakpoints[aUrl]) {
        yield aUrl;
      }
    } else {
      for (let url of Object.keys(this._wholeLineBreakpoints)) {
        yield url;
      }
      for (let url of Object.keys(this._breakpoints)) {
        if (url in this._wholeLineBreakpoints) {
          continue;
        }
        yield url;
      }
    }
  },

  _iterLines: function* (aUrl, aLine) {
    if (aLine != null) {
      if ((this._wholeLineBreakpoints[aUrl]
           && this._wholeLineBreakpoints[aUrl][aLine])
          || (this._breakpoints[aUrl] && this._breakpoints[aUrl][aLine])) {
        yield aLine;
      }
    } else {
      const wholeLines = this._wholeLineBreakpoints[aUrl]
        ? Object.keys(this._wholeLineBreakpoints[aUrl])
        : [];
      const columnLines = this._breakpoints[aUrl]
        ? Object.keys(this._breakpoints[aUrl])
        : [];

      const lines = wholeLines.concat(columnLines).sort();

      let lastLine;
      for (let line of lines) {
        if (line === lastLine) {
          continue;
        }
        yield line;
        lastLine = line;
      }
    }
  },

  _iterColumns: function* (aUrl, aLine, aColumn) {
    if (!this._breakpoints[aUrl] || !this._breakpoints[aUrl][aLine]) {
      return;
    }

    if (aColumn != null) {
      if (this._breakpoints[aUrl][aLine][aColumn]) {
        yield aColumn;
      }
    } else {
      for (let column in this._breakpoints[aUrl][aLine]) {
        yield column;
      }
    }
  },
};

exports.BreakpointStore = BreakpointStore;

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

  this._options = {
    useSourceMaps: false,
    autoBlackBox: false
  };

  // A map of actorID -> actor for breakpoints created and managed by the
  // server.
  this._hiddenBreakpoints = new Map();

  this.global = aGlobal;

  this._allEventsListener = this._allEventsListener.bind(this);
  this.onNewGlobal = this.onNewGlobal.bind(this);
  this.onNewSource = this.onNewSource.bind(this);
  this.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
  this.onDebuggerStatement = this.onDebuggerStatement.bind(this);
  this.onNewScript = this.onNewScript.bind(this);
}

/**
 * The breakpoint store must be shared across instances of ThreadActor so that
 * page reloads don't blow away all of our breakpoints.
 */
ThreadActor.breakpointStore = new BreakpointStore();

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

  get breakpointStore() { return ThreadActor.breakpointStore; },

  get threadLifetimePool() {
    if (!this._threadLifetimePool) {
      this._threadLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._threadLifetimePool);
      this._threadLifetimePool.objectActors = new WeakMap();
    }
    return this._threadLifetimePool;
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

      this.sources.getOriginalLocation(packet.frame.where).then(aOrigPosition => {
        packet.frame.where = aOrigPosition;
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
      const generatedLocation = getFrameLocation(aFrame);
      let { url } = this.synchronize(this.sources.getOriginalLocation(
        generatedLocation));

      return this.sources.isBlackBoxed(url)
        ? undefined
        : pauseAndRespond(aFrame);
    };
  },

  _makeOnPop: function ({ thread, pauseAndRespond, createValueGrip }) {
    return function (aCompletion) {
      // onPop is called with 'this' set to the current frame.

      const generatedLocation = getFrameLocation(this);
      const { url } = thread.synchronize(thread.sources.getOriginalLocation(
        generatedLocation));

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

      const generatedLocation = getFrameLocation(this);
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

    const generatedLocation = getFrameLocation(this.youngestFrame);
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
   * Set a breakpoint on the first bytecode offset in the provided script.
   */
  _breakOnEnter: function(script) {
    let offsets = script.getAllOffsets();
    for (let line = 0, n = offsets.length; line < n; line++) {
      if (offsets[line]) {
        let location = { url: script.url, line: line };
        let resp = this._createAndStoreBreakpoint(location);
        dbg_assert(!resp.actualLocation, "No actualLocation should be returned");
        if (resp.error) {
          reportError(new Error("Unable to set breakpoint on event listener"));
          return;
        }
        let bp = this.breakpointStore.getBreakpoint(location);
        let bpActor = bp.actor;
        dbg_assert(bp, "Breakpoint must exist");
        dbg_assert(bpActor, "Breakpoint actor must be created");
        this._hiddenBreakpoints.set(bpActor.actorID, bpActor);
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

      let promise = this.sources.getOriginalLocation(form.where)
        .then((aOrigLocation) => {
          form.where = aOrigLocation;
          let source = this.sources.source({ url: form.where.url });
          if (source) {
            form.source = source.form();
          }
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
   * Handle a protocol request to set a breakpoint.
   */
  onSetBreakpoint: function (aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Breakpoints can only be set while the debuggee is paused."};
    }

    let { url: originalSource,
          line: originalLine,
          column: originalColumn } = aRequest.location;

    let locationPromise = this.sources.getGeneratedLocation(aRequest.location);
    return locationPromise.then(({url, line, column}) => {
      if (line == null || line < 0) {
        return {
          error: "noScript",
          message: "Requested setting a breakpoint on "
            + url + ":" + line
            + (column != null ? ":" + column : "")
            + " but there is no Debugger.Script at that location"
        };
      }

      let response = this._createAndStoreBreakpoint({
        url: url,
        line: line,
        column: column,
        condition: aRequest.condition
      });
      // If the original location of our generated location is different from
      // the original location we attempted to set the breakpoint on, we will
      // need to know so that we can set actualLocation on the response.
      let originalLocation = this.sources.getOriginalLocation({
        url: url,
        line: line,
        column: column
      });

      return all([response, originalLocation])
        .then(([aResponse, {url, line}]) => {
          if (aResponse.actualLocation) {
            let actualOrigLocation = this.sources.getOriginalLocation(aResponse.actualLocation);
            return actualOrigLocation.then(({ url, line, column }) => {
              if (url !== originalSource
                  || line !== originalLine
                  || column !== originalColumn) {
                aResponse.actualLocation = {
                  url: url,
                  line: line,
                  column: column
                };
              }
              return aResponse;
            });
          }

          if (url !== originalSource || line !== originalLine) {
            aResponse.actualLocation = { url: url, line: line };
          }

          return aResponse;
        });
    });
  },

  /**
   * Create a breakpoint at the specified location and store it in the
   * cache. Takes ownership of `aLocation`.
   *
   * @param Object aLocation
   *        An object of the form { url, line[, column] }
   */
  _createAndStoreBreakpoint: function (aLocation) {
    // Add the breakpoint to the store for later reuse, in case it belongs to a
    // script that hasn't appeared yet.
    this.breakpointStore.addBreakpoint(aLocation);
    return this._setBreakpoint(aLocation);
  },

  /**
   * Set a breakpoint using the jsdbg2 API. If the line on which the breakpoint
   * is being set contains no code, then the breakpoint will slide down to the
   * next line that has runnable code. In this case the server breakpoint cache
   * will be updated, so callers that iterate over the breakpoint cache should
   * take that into account.
   *
   * @param object aLocation
   *        The location of the breakpoint (in the generated source, if source
   *        mapping).
   */
  _setBreakpoint: function (aLocation) {
    let actor;
    let storedBp = this.breakpointStore.getBreakpoint(aLocation);
    if (storedBp.actor) {
      actor = storedBp.actor;
      actor.condition = aLocation.condition;
    } else {
      storedBp.actor = actor = new BreakpointActor(this, {
        url: aLocation.url,
        line: aLocation.line,
        column: aLocation.column,
        condition: aLocation.condition
      });
      this.threadLifetimePool.addActor(actor);
    }

    // Find all scripts matching the given location
    let scripts = this.dbg.findScripts(aLocation);
    if (scripts.length == 0) {
      // Since we did not find any scripts to set the breakpoint on now, return
      // early. When a new script that matches this breakpoint location is
      // introduced, the breakpoint actor will already be in the breakpoint store
      // and will be set at that time.
      return {
        actor: actor.actorID
      };
    }

   /**
    * For each script, if the given line has at least one entry point, set a
    * breakpoint on the bytecode offets for each of them.
    */

    // Debugger.Script -> array of offset mappings
    let scriptsAndOffsetMappings = new Map();

    for (let script of scripts) {
      this._findClosestOffsetMappings(aLocation,
                                      script,
                                      scriptsAndOffsetMappings);
    }

    if (scriptsAndOffsetMappings.size > 0) {
      for (let [script, mappings] of scriptsAndOffsetMappings) {
        for (let offsetMapping of mappings) {
          script.setBreakpoint(offsetMapping.offset, actor);
        }
        actor.addScript(script, this);
      }

      return {
        actor: actor.actorID
      };
    }

   /**
    * If we get here, no breakpoint was set. This is because the given line
    * has no entry points, for example because it is empty. As a fallback
    * strategy, we try to set the breakpoint on the smallest line greater
    * than or equal to the given line that as at least one entry point.
    */

    // Find all innermost scripts matching the given location
    let scripts = this.dbg.findScripts({
      url: aLocation.url,
      line: aLocation.line,
      innermost: true
    });

    /**
     * For each innermost script, look for the smallest line greater than or
     * equal to the given line that has one or more entry points. If found, set
     * a breakpoint on the bytecode offset for each of its entry points.
     */
    let actualLocation;
    let found = false;
    for (let script of scripts) {
      let offsets = script.getAllOffsets();
      for (let line = aLocation.line; line < offsets.length; ++line) {
        if (offsets[line]) {
          for (let offset of offsets[line]) {
            script.setBreakpoint(offset, actor);
          }
          actor.addScript(script, this);
          if (!actualLocation) {
            actualLocation = {
              url: aLocation.url,
              line: line
            };
          }
          found = true;
          break;
        }
      }
    }
    if (found) {
      let existingBp = this.breakpointStore.hasBreakpoint(actualLocation);

      if (existingBp && existingBp.actor) {
        /**
         * We already have a breakpoint actor for the actual location, so
         * actor we created earlier is now redundant. Delete it, update the
         * breakpoint store, and return the actor for the actual location.
         */
        actor.onDelete();
        this.breakpointStore.removeBreakpoint(aLocation);
        return {
          actor: existingBp.actor.actorID,
          actualLocation: actualLocation
        };
      } else {
        /**
         * We don't have a breakpoint actor for the actual location yet.
         * Instead or creating a new actor, reuse the actor we created earlier,
         * and update the breakpoint store.
         */
        actor.location = actualLocation;
        this.breakpointStore.addBreakpoint({
          actor: actor,
          url: actualLocation.url,
          line: actualLocation.line,
          column: actualLocation.column
        });
        this.breakpointStore.removeBreakpoint(aLocation);
        return {
          actor: actor.actorID,
          actualLocation: actualLocation
        };
      }
    }

    /**
     * If we get here, no line matching the given line was found, so just
     * fail epically.
     */
    return {
      error: "noCodeAtLineColumn",
      actor: actor.actorID
    };
  },

  /**
   * Find all of the offset mappings associated with `aScript` that are closest
   * to `aTargetLocation`. If new offset mappings are found that are closer to
   * `aTargetOffset` than the existing offset mappings inside
   * `aScriptsAndOffsetMappings`, we empty that map and only consider the
   * closest offset mappings. If there is no column in `aTargetLocation`, we add
   * all offset mappings that are on the given line.
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
    // If we are given a column, we will try and break only at that location,
    // otherwise we will break anytime we get on that line.

    if (aTargetLocation.column == null) {
      let offsetMappings = aScript.getLineOffsets(aTargetLocation.line)
        .map(o => ({
          line: aTargetLocation.line,
          offset: o
        }));
      if (offsetMappings.length) {
        aScriptsAndOffsetMappings.set(aScript, offsetMappings);
      }
      return;
    }

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
  },

  /**
   * Get the script and source lists from the debugger.
   */
  _discoverSources: function () {
    // Only get one script per url.
    const sourcesToScripts = new Map();
    for (let s of this.dbg.findScripts()) {
      if (s.source) {
        sourcesToScripts.set(s.source, s);
      }
    }

    return all([this.sources.sourcesForScript(script)
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
    for (let bp of this.breakpointStore.findBreakpoints()) {
      if (bp.actor) {
        bp.actor.removeScripts();
      }
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
        return this.objectGrip(aValue, aPool);
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
    const generatedLocation = getFrameLocation(aFrame);
    const { url } = this.synchronize(this.sources.getOriginalLocation(
      generatedLocation));

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

    const generatedLocation = getFrameLocation(aFrame);
    const { url } = this.synchronize(this.sources.getOriginalLocation(
      generatedLocation));

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
    this._addScript(aScript);

    // |onNewScript| is only fired for top level scripts (AKA staticLevel == 0),
    // so we have to make sure to call |_addScript| on every child script as
    // well to restore breakpoints in those scripts.
    for (let s of aScript.getChildScripts()) {
      this._addScript(s);
    }

    this.sources.sourcesForScript(aScript);
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
  _allowSource: function (aSourceUrl) {
    // Ignore anything we don't have a URL for (eval scripts, for example).
    if (!aSourceUrl)
      return false;
    // Ignore XBL bindings for content debugging.
    if (aSourceUrl.indexOf("chrome://") == 0) {
      return false;
    }
    // Ignore about:* pages for content debugging.
    if (aSourceUrl.indexOf("about:") == 0) {
      return false;
    }
    return true;
  },

  /**
   * Restore any pre-existing breakpoints to the scripts that we have access to.
   */
  _restoreBreakpoints: function () {
    if (this.breakpointStore.size === 0) {
      return;
    }

    for (let s of this.dbg.findScripts()) {
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
    if (!this._allowSource(aScript.url)) {
      return false;
    }

    // Set any stored breakpoints.

    let endLine = aScript.startLine + aScript.lineCount - 1;
    for (let bp of this.breakpointStore.findBreakpoints({ url: aScript.url })) {
      // Only consider breakpoints that are not already associated with
      // scripts, and limit search to the line numbers contained in the new
      // script.
      if (!bp.actor.scripts.length
          && bp.line >= aScript.startLine
          && bp.line <= endLine) {
        this._setBreakpoint(bp);
      }
    }

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
  "setBreakpoint": ThreadActor.prototype.onSetBreakpoint,
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
  switch (aURI.scheme) {
    case "jar":
    case "file":
      return aURI;

    case "chrome":
      let resolved = Cc["@mozilla.org/chrome/chrome-registry;1"].
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
 * A SourceActor provides information about the source of a script.
 *
 * @param String url
 *        The url of the source we are representing.
 * @param ThreadActor thread
 *        The current thread actor.
 * @param SourceMapConsumer sourceMap
 *        Optional. The source map that introduced this source, if available.
 * @param String generatedSource
 *        Optional, passed in when aSourceMap is also passed in. The generated
 *        source url that introduced this source.
 * @param String text
 *        Optional. The content text of this source, if immediately available.
 * @param String contentType
 *        Optional. The content type of this source, if immediately available.
 */
function SourceActor({ url, thread, sourceMap, generatedSource, text,
                       contentType }) {
  this._threadActor = thread;
  this._url = url;
  this._sourceMap = sourceMap;
  this._generatedSource = generatedSource;
  this._text = text;
  this._contentType = contentType;

  this.onSource = this.onSource.bind(this);
  this._invertSourceMap = this._invertSourceMap.bind(this);
  this._saveMap = this._saveMap.bind(this);
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

  get threadActor() this._threadActor,
  get url() this._url,
  get addonID() this._addonID,
  get addonPath() this._addonPath,

  get prettyPrintWorker() {
    return this.threadActor.prettyPrintWorker;
  },

  form: function () {
    return {
      actor: this.actorID,
      url: this._url,
      addonID: this._addonID,
      addonPath: this._addonPath,
      isBlackBoxed: this.threadActor.sources.isBlackBoxed(this.url),
      isPrettyPrinted: this.threadActor.sources.isPrettyPrinted(this.url)
      // TODO bug 637572: introductionScript
    };
  },

  disconnect: function () {
    if (this.registeredPool && this.registeredPool.sourceActors) {
      delete this.registeredPool.sourceActors[this.actorID];
    }
  },

  _mapSourceToAddon: function() {
    try {
      var nsuri = Services.io.newURI(this._url.split(" -> ").pop(), null, null);
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
          const error = new Error("Could not find the root of the add-on for " + this._url);
          DevToolsUtils.reportException("SourceActor.prototype._mapSourceToAddon", error)
          return;
        }

        this._addonPath = path;
      }
    }
  },

  _getSourceText: function () {
    const toResolvedContent = t => resolve({
      content: t,
      contentType: this._contentType
    });

    let sc;
    if (this._sourceMap && (sc = this._sourceMap.sourceContentFor(this._url))) {
      return toResolvedContent(sc);
    }

    if (this._text) {
      return toResolvedContent(this._text);
    }

    // XXX bug 865252: Don't load from the cache if this is a source mapped
    // source because we can't guarantee that the cache has the most up to date
    // content for this source like we can if it isn't source mapped.
    let sourceFetched = fetch(this._url, { loadFromCache: !this._sourceMap });

    // Record the contentType we just learned during fetching
    sourceFetched.then(({ contentType }) => {
      this._contentType = contentType;
    });

    return sourceFetched;
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
          "message": "Could not load the source for " + this._url + ".\n"
            + DevToolsUtils.safeErrorString(aError)
        };
      });
  },

  /**
   * Handler for the "prettyPrint" packet.
   */
  onPrettyPrint: function ({ indent }) {
    this.threadActor.sources.prettyPrint(this._url, indent);
    return this._getSourceText()
      .then(this._sendToPrettyPrintWorker(indent))
      .then(this._invertSourceMap)
      .then(this._saveMap)
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
        url: this._url,
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
    const generator = new SourceMapGenerator({ file: this._url });
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
      generator.setSourceContent(this._url, code);
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
          source: this._url
        });
        location.source = this._url;
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
  _saveMap: function ({ map }) {
    if (this._sourceMap) {
      // Compose the source maps
      this._oldSourceMap = this._sourceMap;
      this._sourceMap = SourceMapGenerator.fromSourceMap(this._sourceMap);
      this._sourceMap.applySourceMap(map, this._url);
      this._sourceMap = SourceMapConsumer.fromSourceMap(this._sourceMap);
      this._threadActor.sources.saveSourceMap(this._sourceMap,
                                              this._generatedSource);
    } else {
      this._sourceMap = map;
      this._threadActor.sources.saveSourceMap(this._sourceMap, this._url);
    }
  },

  /**
   * Handler for the "disablePrettyPrint" packet.
   */
  onDisablePrettyPrint: function () {
    this._sourceMap = this._oldSourceMap;
    this.threadActor.sources.saveSourceMap(this._sourceMap,
                                           this._generatedSource || this._url);
    this.threadActor.sources.disablePrettyPrint(this._url);
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
  }
};

SourceActor.prototype.requestTypes = {
  "source": SourceActor.prototype.onSource,
  "blackbox": SourceActor.prototype.onBlackBox,
  "unblackbox": SourceActor.prototype.onUnblackBox,
  "prettyPrint": SourceActor.prototype.onPrettyPrint,
  "disablePrettyPrint": SourceActor.prototype.onDisablePrettyPrint
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
  return stringifier(aObj);
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
  }
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
      url: this.obj.script.url,
      line: this.obj.script.startLine,
      // TODO bug 901138: use Debugger.Script.prototype.startColumn.
      column: 0
    };

    return this.threadActor.sources.getOriginalLocation(generatedLocation)
      .then(({ url, line, column }) => {
        return {
          from: this.actorID,
          url: url,
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
    } else if (aRawObj instanceof Ci.nsIDOMTransitionEvent ||
               aRawObj instanceof Ci.nsIDOMAnimationEvent) {
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
    let form = { actor: this.actorID,
                 type: this.frame.type };
    if (this.frame.type === "call") {
      form.callee = this.threadActor.createValueGrip(this.frame.callee);
    }

    if (this.frame.environment) {
      let envActor = this.threadActor
        .createEnvironmentActor(this.frame.environment,
                                this.frameLifetimePool);
      form.environment = envActor.form();
    }
    form.this = this.threadActor.createValueGrip(this.frame.this);
    form.arguments = this._args();
    if (this.frame.script) {
      form.where = getFrameLocation(this.frame);
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
 * @param object aLocation
 *        The location of the breakpoint as specified in the protocol.
 */
function BreakpointActor(aThreadActor, { url, line, column, condition })
{
  this.scripts = [];
  this.threadActor = aThreadActor;
  this.location = { url: url, line: line, column: column };
  this.condition = condition;
}

BreakpointActor.prototype = {
  actorPrefix: "breakpoint",
  condition: null,

  /**
   * Called when this same breakpoint is added to another Debugger.Script
   * instance, in the case of a page reload.
   *
   * @param aScript Debugger.Script
   *        The new source script on which the breakpoint has been set.
   * @param ThreadActor aThreadActor
   *        The parent thread actor that contains this breakpoint.
   */
  addScript: function (aScript, aThreadActor) {
    this.threadActor = aThreadActor;
    this.scripts.push(aScript);
  },

  /**
   * Remove the breakpoints from associated scripts and clear the script cache.
   */
  removeScripts: function () {
    for (let script of this.scripts) {
      script.clearBreakpoint(this);
    }
    this.scripts = [];
  },

  /**
   * Check if this breakpoint has a condition that doesn't error and
   * evaluates to true in aFrame
   *
   * @param aFrame Debugger.Frame
   *        The frame to evaluate the condition in
   */
  isValidCondition: function(aFrame) {
    if(!this.condition) {
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
    let { url } = this.threadActor.synchronize(
      this.threadActor.sources.getOriginalLocation({
        url: this.location.url,
        line: this.location.line,
        column: this.location.column
      }));

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
    this.threadActor.breakpointStore.removeBreakpoint(this.location);
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
      // The slot is optimized out.
      // FIXME: Need actual UI, bug 941287.
      if (value && value.optimizedOut) {
        continue;
      }

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands (bug 725815).
      let desc = {
        value: value,
        configurable: false,
        writable: true,
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
      // The slot is optimized out or arguments on a dead scope.
      // FIXME: Need actual UI, bug 941287.
      if (value && (value.optimizedOut || value.missingArguments)) {
        continue;
      }

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands.
      let desc = {
        value: value,
        configurable: false,
        writable: true,
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

/**
 * Override the toString method in order to get more meaningful script output
 * for debugging the debugger.
 */
Debugger.Script.prototype.toString = function() {
  let output = "";
  if (this.url) {
    output += this.url;
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
  actorPrefix: "chromeDebugger",

  /**
   * Override the eligibility check for scripts and sources to make sure every
   * script and source with a URL is stored when debugging chrome.
   */
  _allowSource: aSourceURL => !!aSourceURL
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
  _allowSource: function(aSourceURL) {
    // Hide eval scripts
    if (!aSourceURL) {
      return false;
    }

    // XPIProvider.jsm evals some code in every add-on's bootstrap.js. Hide it.
    if (aSourceURL == "resource://gre/modules/addons/XPIProvider.jsm") {
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
  this._onNewSource = aOnNewSource;

  // generated source url --> promise of SourceMapConsumer
  this._sourceMapsByGeneratedSource = Object.create(null);
  // original source url --> promise of SourceMapConsumer
  this._sourceMapsByOriginalSource = Object.create(null);
  // source url --> SourceActor
  this._sourceActors = Object.create(null);
  // original url --> generated url
  this._generatedUrlsByOriginalUrl = Object.create(null);
}

/**
 * Must be a class property because it needs to persist across reloads, same as
 * the breakpoint store.
 */
ThreadSources._blackBoxedSources = new Set(["self-hosted"]);
ThreadSources._prettyPrintedSources = new Map();

/**
 * Matches strings of the form "foo.min.js" or "foo-min.js", etc. If the regular
 * expression matches, we can be fairly sure that the source is minified, and
 * treat it as such.
 */
const MINIFIED_SOURCE_REGEXP = /\bmin\.js$/;

ThreadSources.prototype = {
  /**
   * Return the source actor representing |url|, creating one if none
   * exists already. Returns null if |url| is not allowed by the 'allow'
   * predicate.
   *
   * Right now this takes a URL, but in the future it should
   * take a Debugger.Source. See bug 637572.
   *
   * @param String url
   *        The source URL.
   * @param optional SourceMapConsumer sourceMap
   *        The source map that introduced this source, if any.
   * @param optional String generatedSource
   *        The generated source url that introduced this source via source map,
   *        if any.
   * @param optional String text
   *        The text content of the source, if immediately available.
   * @param optional String contentType
   *        The content type of the source, if immediately available.
   * @returns a SourceActor representing the source at aURL or null.
   */
  source: function ({ url, sourceMap, generatedSource, text, contentType }) {
    if (!this._allow(url)) {
      return null;
    }

    if (url in this._sourceActors) {
      return this._sourceActors[url];
    }

    if (this._autoBlackBox && this._isMinifiedURL(url)) {
      this.blackBox(url);
    }

    let actor = new SourceActor({
      url: url,
      thread: this._thread,
      sourceMap: sourceMap,
      generatedSource: generatedSource,
      text: text,
      contentType: contentType
    });
    this._thread.threadLifetimePool.addActor(actor);
    this._sourceActors[url] = actor;
    try {
      this._onNewSource(actor);
    } catch (e) {
      reportError(e);
    }
    return actor;
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
   * Only to be used when we aren't source mapping.
   */
  _sourceForScript: function (aScript) {
    const spec = {
      url: aScript.url
    };

    // XXX bug 915433: We can't rely on Debugger.Source.prototype.text if the
    // source is an HTML-embedded <script> tag. Since we don't have an API
    // implemented to detect whether this is the case, we need to be
    // conservative and only use Debugger.Source.prototype.text if we get a
    // normal .js file.
    if (aScript.url) {
      try {
        const url = Services.io.newURI(aScript.url, null, null)
          .QueryInterface(Ci.nsIURL);
        if (url.fileExtension === "js") {
          spec.contentType = "text/javascript";
          // If the Debugger API wasn't able to load the source,
          // because sources were discarded
          // (javascript.options.discardSystemSource == true),
          // give source() a chance to fetch them.
          if (aScript.source.text != "[no source]") {
            spec.text = aScript.source.text;
          }
        }
      } catch(ex) {
        // Not a valid URI.
      }
    }

    return this.source(spec);
  },

  /**
   * Return a promise of an array of source actors representing all the
   * sources of |aScript|.
   *
   * If source map handling is enabled and |aScript| has a source map, then
   * use it to find all of |aScript|'s *original* sources; return a promise
   * of an array of source actors for those.
   */
  sourcesForScript: function (aScript) {
    if (!this._useSourceMaps || !aScript.sourceMapURL) {
      return resolve([this._sourceForScript(aScript)].filter(isNotNull));
    }

    return this.sourceMap(aScript)
      .then((aSourceMap) => {
        return [
          this.source({ url: s,
                        sourceMap: aSourceMap,
                        generatedSource: aScript.url })
          for (s of aSourceMap.sources)
        ];
      })
      .then(null, (e) => {
        reportError(e);
        delete this._sourceMapsByGeneratedSource[aScript.url];
        return [this._sourceForScript(aScript)];
      })
      .then(ss => ss.filter(isNotNull));
  },

  /**
   * Return a promise of a SourceMapConsumer for the source map for
   * |aScript|; if we already have such a promise extant, return that.
   * |aScript| must have a non-null sourceMapURL.
   */
  sourceMap: function (aScript) {
    dbg_assert(aScript.sourceMapURL, "Script should have a sourceMapURL");
    let sourceMapURL = this._normalize(aScript.sourceMapURL, aScript.url);
    let map = this._fetchSourceMap(sourceMapURL, aScript.url)
      .then(aSourceMap => this.saveSourceMap(aSourceMap, aScript.url));
    this._sourceMapsByGeneratedSource[aScript.url] = map;
    return map;
  },

  /**
   * Save the given source map so that we can use it to query source locations
   * down the line.
   */
  saveSourceMap: function (aSourceMap, aGeneratedSource) {
    if (!aSourceMap) {
      delete this._sourceMapsByGeneratedSource[aGeneratedSource];
      return null;
    }
    this._sourceMapsByGeneratedSource[aGeneratedSource] = resolve(aSourceMap);
    for (let s of aSourceMap.sources) {
      this._generatedUrlsByOriginalUrl[s] = aGeneratedSource;
      this._sourceMapsByOriginalSource[s] = resolve(aSourceMap);
    }
    return aSourceMap;
  },

  /**
   * Return a promise of a SourceMapConsumer for the source map located at
   * |aAbsSourceMapURL|, which must be absolute. If there is already such a
   * promise extant, return it.
   *
   * @param string aAbsSourceMapURL
   *        The source map URL, in absolute form, not relative.
   * @param string aScriptURL
   *        When the source map URL is a data URI, there is no sourceRoot on the
   *        source map, and the source map's sources are relative, we resolve
   *        them from aScriptURL.
   */
  _fetchSourceMap: function (aAbsSourceMapURL, aScriptURL) {
    return fetch(aAbsSourceMapURL, { loadFromCache: false })
      .then(({ content }) => {
        let map = new SourceMapConsumer(content);
        this._setSourceMapRoot(map, aAbsSourceMapURL, aScriptURL);
        return map;
      });
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
   * Returns a promise of the location in the original source if the source is
   * source mapped, otherwise a promise of the same location.
   */
  getOriginalLocation: function ({ url, line, column }) {
    if (url in this._sourceMapsByGeneratedSource) {
      column = column || 0;

      return this._sourceMapsByGeneratedSource[url]
        .then((aSourceMap) => {
          let { source: aSourceURL, line: aLine, column: aColumn } = aSourceMap.originalPositionFor({
            line: line,
            column: column
          });
          return {
            url: aSourceURL,
            line: aLine,
            column: aColumn
          };
        })
        .then(null, error => {
          if (!DevToolsUtils.reportingDisabled) {
            DevToolsUtils.reportException("ThreadSources.prototype.getOriginalLocation", error);
          }
          return { url: null, line: null, column: null };
        });
    }

    // No source map
    return resolve({
      url: url,
      line: line,
      column: column
    });
  },

  /**
   * Returns a promise of the location in the generated source corresponding to
   * the original source and line given.
   *
   * When we pass a script S representing generated code to |sourceMap|,
   * above, that returns a promise P. The process of resolving P populates
   * the tables this function uses; thus, it won't know that S's original
   * source URLs map to S until P is resolved.
   */
  getGeneratedLocation: function ({ url, line, column }) {
    if (url in this._sourceMapsByOriginalSource) {
      return this._sourceMapsByOriginalSource[url]
        .then((aSourceMap) => {
          let { line: aLine, column: aColumn } = aSourceMap.generatedPositionFor({
            source: url,
            line: line,
            column: column == null ? Infinity : column
          });
          return {
            url: this._generatedUrlsByOriginalUrl[url],
            line: aLine,
            column: aColumn
          };
        });
    }

    // No source map
    return resolve({
      url: url,
      line: line,
      column: column
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
    return ThreadSources._blackBoxedSources.has(aURL);
  },

  /**
   * Add the given source URL to the set of sources that are black boxed.
   *
   * @param aURL String
   *        The URL of the source which we are black boxing.
   */
  blackBox: function (aURL) {
    ThreadSources._blackBoxedSources.add(aURL);
  },

  /**
   * Remove the given source URL to the set of sources that are black boxed.
   *
   * @param aURL String
   *        The URL of the source which we are no longer black boxing.
   */
  unblackBox: function (aURL) {
    ThreadSources._blackBoxedSources.delete(aURL);
  },

  /**
   * Returns true if the given URL is pretty printed.
   *
   * @param aURL String
   *        The URL of the source that might be pretty printed.
   */
  isPrettyPrinted: function (aURL) {
    return ThreadSources._prettyPrintedSources.has(aURL);
  },

  /**
   * Add the given URL to the set of sources that are pretty printed.
   *
   * @param aURL String
   *        The URL of the source to be pretty printed.
   */
  prettyPrint: function (aURL, aIndent) {
    ThreadSources._prettyPrintedSources.set(aURL, aIndent);
  },

  /**
   * Return the indent the given URL was pretty printed by.
   */
  prettyPrintIndent: function (aURL) {
    return ThreadSources._prettyPrintedSources.get(aURL);
  },

  /**
   * Remove the given URL from the set of sources that are pretty printed.
   *
   * @param aURL String
   *        The URL of the source that is no longer pretty printed.
   */
  disablePrettyPrint: function (aURL) {
    ThreadSources._prettyPrintedSources.delete(aURL);
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

  iter: function* () {
    for (let url in this._sourceActors) {
      yield this._sourceActors[url];
    }
  }
};

exports.ThreadSources = ThreadSources;

// Utility functions.

// TODO bug 863089: use Debugger.Script.prototype.getOffsetColumn when it is
// implemented.
function getOffsetColumn(aOffset, aScript) {
  let bestOffsetMapping = null;
  for (let offsetMapping of aScript.getAllColumnOffsets()) {
    if (!bestOffsetMapping ||
        (offsetMapping.offset <= aOffset &&
         offsetMapping.offset > bestOffsetMapping.offset)) {
      bestOffsetMapping = offsetMapping;
    }
  }

  if (!bestOffsetMapping) {
    // XXX: Try not to completely break the experience of using the debugger for
    // the user by assuming column 0. Simultaneously, report the error so that
    // there is a paper trail if the assumption is bad and the debugging
    // experience becomes wonky.
    reportError(new Error("Could not find a column for offset " + aOffset
                          + " in the script " + aScript));
    return 0;
  }

  return bestOffsetMapping.columnNumber;
}

/**
 * Return the non-source-mapped location of the given Debugger.Frame. If the
 * frame does not have a script, the location's properties are all null.
 *
 * @param Debugger.Frame aFrame
 *        The frame whose location we are getting.
 * @returns Object
 *          Returns an object of the form { url, line, column }
 */
function getFrameLocation(aFrame) {
  if (!aFrame || !aFrame.script) {
    return { url: null, line: null, column: null };
  }
  return {
    url: aFrame.script.url,
    line: aFrame.script.getOffsetLine(aFrame.offset),
    column: getOffsetColumn(aFrame.offset, aFrame.script)
  }
}

/**
 * Returns true if its argument is not null.
 */
function isNotNull(aThing) {
  return aThing !== null;
}

/**
 * Performs a request to load the desired URL and returns a promise.
 *
 * @param aURL String
 *        The URL we will request.
 * @returns Promise
 *        A promise of the document at that URL, as a string.
 *
 * XXX: It may be better to use nsITraceableChannel to get to the sources
 * without relying on caching when we can (not for eval, etc.):
 * http://www.softwareishard.com/blog/firebug/nsitraceablechannel-intercept-http-traffic/
 */
function fetch(aURL, aOptions={ loadFromCache: true }) {
  let deferred = defer();
  let scheme;
  let url = aURL.split(" -> ").pop();
  let charset;
  let contentType;

  try {
    scheme = Services.io.extractScheme(url);
  } catch (e) {
    // In the xpcshell tests, the script url is the absolute path of the test
    // file, which will make a malformed URI error be thrown. Add the file
    // scheme prefix ourselves.
    url = "file://" + url;
    scheme = Services.io.extractScheme(url);
  }

  switch (scheme) {
    case "file":
    case "chrome":
    case "resource":
      try {
        NetUtil.asyncFetch(url, function onFetch(aStream, aStatus, aRequest) {
          if (!components.isSuccessCode(aStatus)) {
            deferred.reject(new Error("Request failed with status code = "
                                      + aStatus
                                      + " after NetUtil.asyncFetch for url = "
                                      + url));
            return;
          }

          let source = NetUtil.readInputStreamToString(aStream, aStream.available());
          contentType = aRequest.contentType;
          deferred.resolve(source);
          aStream.close();
        });
      } catch (ex) {
        deferred.reject(ex);
      }
      break;

    default:
      let channel;
      try {
        channel = Services.io.newChannel(url, null, null);
      } catch (e if e.name == "NS_ERROR_UNKNOWN_PROTOCOL") {
        // On Windows xpcshell tests, c:/foo/bar can pass as a valid URL, but
        // newChannel won't be able to handle it.
        url = "file:///" + url;
        channel = Services.io.newChannel(url, null, null);
      }
      let chunks = [];
      let streamListener = {
        onStartRequest: function(aRequest, aContext, aStatusCode) {
          if (!components.isSuccessCode(aStatusCode)) {
            deferred.reject(new Error("Request failed with status code = "
                                      + aStatusCode
                                      + " in onStartRequest handler for url = "
                                      + url));
          }
        },
        onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
          chunks.push(NetUtil.readInputStreamToString(aStream, aCount));
        },
        onStopRequest: function(aRequest, aContext, aStatusCode) {
          if (!components.isSuccessCode(aStatusCode)) {
            deferred.reject(new Error("Request failed with status code = "
                                      + aStatusCode
                                      + " in onStopRequest handler for url = "
                                      + url));
            return;
          }

          charset = channel.contentCharset;
          contentType = channel.contentType;
          deferred.resolve(chunks.join(""));
        }
      };

      channel.loadFlags = aOptions.loadFromCache
        ? channel.LOAD_FROM_CACHE
        : channel.LOAD_BYPASS_CACHE;
      channel.asyncOpen(streamListener, null);
      break;
  }

  return deferred.promise.then(source => {
    return {
      content: convertToUnicode(source, charset),
      contentType: contentType
    };
  });
}

/**
 * Convert a given string, encoded in a given character set, to unicode.
 *
 * @param string aString
 *        A string.
 * @param string aCharset
 *        A character set.
 */
function convertToUnicode(aString, aCharset=null) {
  // Decoding primitives.
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
    .createInstance(Ci.nsIScriptableUnicodeConverter);
  try {
    converter.charset = aCharset || "UTF-8";
    return converter.ConvertToUnicode(aString);
  } catch(e) {
    return aString;
  }
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

exports.register = function(handle) {
  ThreadActor.breakpointStore = new BreakpointStore();
  ThreadSources._blackBoxedSources = new Set(["self-hosted"]);
  ThreadSources._prettyPrintedSources = new Map();
};

exports.unregister = function(handle) {
  ThreadActor.breakpointStore = null;
  ThreadSources._blackBoxedSources.clear();
  ThreadSources._prettyPrintedSources.clear();
};
