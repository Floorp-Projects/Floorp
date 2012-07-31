/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; js-indent-level: 2; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/**
 * JSD2 actors.
 */
/**
 * Creates a ThreadActor.
 *
 * ThreadActors manage a JSInspector object and manage execution/inspection
 * of debuggees.
 *
 * @param aHooks object
 *        An object with preNest and postNest methods for calling when entering
 *        and exiting a nested event loop, as well as addToBreakpointPool and
 *        removeFromBreakpointPool methods for handling breakpoint lifetime.
 */
function ThreadActor(aHooks)
{
  this._state = "detached";
  this._frameActors = [];
  this._environmentActors = [];
  this._hooks = aHooks ? aHooks : {};
  this._breakpointStore = {};
  this._scripts = {};
}

ThreadActor.prototype = {
  actorPrefix: "context",

  get state() { return this._state; },

  get dbg() { return this._dbg; },

  get threadLifetimePool() {
    if (!this._threadLifetimePool) {
      this._threadLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._threadLifetimePool);
    }
    return this._threadLifetimePool;
  },

  /**
   * Add a debuggee global to the Debugger object.
   */
  addDebuggee: function TA_addDebuggee(aGlobal) {
    // Use the inspector xpcom component to turn on debugging
    // for aGlobal's compartment.  Ideally this won't be necessary
    // medium- to long-term, and will be managed by the engine
    // instead.

    if (!this._dbg) {
      this._dbg = new Debugger();
    }

    this.dbg.addDebuggee(aGlobal);
    this.dbg.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
    this.dbg.onDebuggerStatement = this.onDebuggerStatement.bind(this);
    this.dbg.onNewScript = this.onNewScript.bind(this);
    // Keep the debugger disabled until a client attaches.
    this.dbg.enabled = false;
  },

  /**
   * Remove a debuggee global from the JSInspector.
   */
  removeDebugee: function TA_removeDebuggee(aGlobal) {
    try {
      this.dbg.removeDebuggee(aGlobal);
    } catch(ex) {
      // XXX: This debuggee has code currently executing on the stack,
      // we need to save this for later.
    }
  },

  disconnect: function TA_disconnect() {
    if (this._state == "paused") {
      this.onResume();
    }

    this._state = "exited";
    if (this.dbg) {
      this.dbg.enabled = false;
      this._dbg = null;
    }
    this.conn.removeActorPool(this._threadLifetimePool || undefined);
    this._threadLifetimePool = null;
    // Unless we carefully take apart the scripts table this way, we end up
    // leaking documents. It would be nice to track this down carefully, once
    // we have the appropriate tools.
    for (let url in this._scripts) {
      delete this._scripts[url];
    }
    this._scripts = {};
  },

  /**
   * Disconnect the debugger and put the actor in the exited state.
   */
  exit: function TA_exit() {
    this.disconnect();
  },

  // Request handlers
  onAttach: function TA_onAttach(aRequest) {
    if (this.state === "exited") {
      return { type: "exited" };
    }

    if (this.state !== "detached") {
      return { error: "wrongState" };
    }

    this._state = "attached";

    this.dbg.enabled = true;
    try {
      // Put ourselves in the paused state.
      // XXX: We need to put the debuggee in a paused state too.
      let packet = this._paused();
      if (!packet) {
        return { error: "notAttached" };
      }
      packet.why = { type: "attached" };

      // Send the response to the attach request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send(packet);

      // Start a nested event loop.
      this._nest();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch(e) {
      Cu.reportError(e);
      return { error: "notAttached", message: e.toString() };
    }
  },

  onDetach: function TA_onDetach(aRequest) {
    this.disconnect();
    return { type: "detached" };
  },

  /**
   * Pause the debuggee, by entering a nested event loop, and return a 'paused'
   * packet to the client.
   *
   * @param Debugger.Frame aFrame
   *        The newest debuggee frame in the stack.
   * @param object aReason
   *        An object with a 'type' property containing the reason for the pause.
   */
  _pauseAndRespond: function TA__pauseAndRespond(aFrame, aReason) {
    try {
      let packet = this._paused(aFrame);
      if (!packet) {
        return undefined;
      }
      packet.why = aReason;
      this.conn.send(packet);
      return this._nest();
    } catch(e) {
      let msg = "Got an exception during TA__pauseAndRespond: " + e +
                ": " + e.stack;
      Cu.reportError(msg);
      dumpn(msg);
      return undefined;
    }
  },

  /**
   * Handle a protocol request to resume execution of the debuggee.
   */
  onResume: function TA_onResume(aRequest) {
    if (aRequest && aRequest.forceCompletion) {
      // TODO: remove this when Debugger.Frame.prototype.pop is implemented in
      // bug 736733.
      if (typeof this.frame.pop != "function") {
        return { error: "notImplemented",
                 message: "forced completion is not yet implemented." };
      }

      this.dbg.getNewestFrame().pop(aRequest.completionValue);
      let packet = this._resumed();
      DebuggerServer.xpcInspector.exitNestedEventLoop();
      return { type: "resumeLimit", frameFinished: aRequest.forceCompletion };
    }

    if (aRequest && aRequest.resumeLimit) {
      // Bind these methods because some of the hooks are called with 'this'
      // set to the current frame.
      let pauseAndRespond = this._pauseAndRespond.bind(this);
      let createValueGrip = this.createValueGrip.bind(this);

      let startFrame = this._youngestFrame;
      let startLine;
      if (this._youngestFrame.script) {
        let offset = this._youngestFrame.offset;
        startLine = this._youngestFrame.script.getOffsetLine(offset);
      }

      // Define the JS hook functions for stepping.

      let onEnterFrame = function TA_onEnterFrame(aFrame) {
        return pauseAndRespond(aFrame, { type: "resumeLimit" });
      };

      let onPop = function TA_onPop(aCompletion) {
        // onPop is called with 'this' set to the current frame.

        // Note that we're popping this frame; we need to watch for
        // subsequent step events on its caller.
        this.reportedPop = true;

        return pauseAndRespond(this, { type: "resumeLimit" });
      }

      let onStep = function TA_onStep() {
        // onStep is called with 'this' set to the current frame.

        // If we've changed frame or line, then report that.
        if (this !== startFrame ||
            (this.script &&
             this.script.getOffsetLine(this.offset) != startLine)) {
          return pauseAndRespond(this, { type: "resumeLimit" });
        }

        // Otherwise, let execution continue.
        return undefined;
      }

      switch (aRequest.resumeLimit.type) {
        case "step":
          this.dbg.onEnterFrame = onEnterFrame;
          // Fall through.
        case "next":
          let stepFrame = this._getNextStepFrame(startFrame);
          if (stepFrame) {
            stepFrame.onStep = onStep;
            stepFrame.onPop = onPop;
          }
          break;
        case "finish":
          stepFrame = this._getNextStepFrame(startFrame);
          if (stepFrame) {
            stepFrame.onPop = onPop;
          }
          break;
        default:
          return { error: "badParameterType",
                   message: "Unknown resumeLimit type" };
      }
    }

    if (aRequest && aRequest.pauseOnExceptions) {
      this.dbg.onExceptionUnwind = this.onExceptionUnwind.bind(this);
    }
    let packet = this._resumed();
    DebuggerServer.xpcInspector.exitNestedEventLoop();
    return packet;
  },

  /**
   * Helper method that returns the next frame when stepping.
   */
  _getNextStepFrame: function TA__getNextStepFrame(aFrame) {
    let stepFrame = aFrame.reportedPop ? aFrame.older : aFrame;
    if (!stepFrame || !stepFrame.script) {
      stepFrame = null;
    }
    return stepFrame;
  },

  onClientEvaluate: function TA_onClientEvaluate(aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Debuggee must be paused to evaluate code." };
    };

    let frame = this._requestFrame(aRequest.frame);
    if (!frame) {
      return { error: "unknownFrame",
               message: "Evaluation frame not found" };
    }

    if (!frame.environment) {
      return { error: "notDebuggee",
               message: "cannot access the environment of this frame." };
    };

    // We'll clobber the youngest frame if the eval causes a pause, so
    // save our frame now to be restored after eval returns.
    // XXX: or we could just start using dbg.getNewestFrame() now that it
    // works as expected.
    let youngest = this._youngestFrame;

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

  onFrames: function TA_onFrames(aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Stack frames are only available while the debuggee is paused."};
    }

    let start = aRequest.start ? aRequest.start : 0;
    let count = aRequest.count;

    // Find the starting frame...
    let frame = this._youngestFrame;
    let i = 0;
    while (frame && (i < start)) {
      frame = frame.older;
      i++;
    }

    // Return request.count frames, or all remaining
    // frames if count is not defined.
    let frames = [];
    for (; frame && (!count || i < (start + count)); i++) {
      let form = this._createFrameActor(frame).form();
      form.depth = i;
      frames.push(form);
      frame = frame.older;
    }

    return { frames: frames };
  },

  onReleaseMany: function TA_onReleaseMany(aRequest) {
    if (!aRequest.actors) {
      return { error: "missingParameter",
               message: "no actors were specified" };
    }

    for each (let actorID in aRequest.actors) {
      let actor = this.threadLifetimePool.get(actorID);
      this.threadLifetimePool.objectActors.delete(actor.obj);
      this.threadLifetimePool.removeActor(actorID);
    }
    return {};
  },

  /**
   * Handle a protocol request to set a breakpoint.
   */
  onSetBreakpoint: function TA_onSetBreakpoint(aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Breakpoints can only be set while the debuggee is paused."};
    }

    let location = aRequest.location;
    let line = location.line;
    if (!this._scripts[location.url] || line < 0) {
      return { error: "noScript" };
    }

    // Add the breakpoint to the store for later reuse, in case it belongs to a
    // script that hasn't appeared yet.
    if (!this._breakpointStore[location.url]) {
      this._breakpointStore[location.url] = [];
    }
    let scriptBreakpoints = this._breakpointStore[location.url];
    scriptBreakpoints[line] = {
      url: location.url,
      line: line,
      column: location.column
    };

    return this._setBreakpoint(location);
  },

  /**
   * Set a breakpoint using the jsdbg2 API. If the line on which the breakpoint
   * is being set contains no code, then the breakpoint will slide down to the
   * next line that has runnable code. In this case the server breakpoint cache
   * will be updated, so callers that iterate over the breakpoint cache should
   * take that into account.
   *
   * @param object aLocation
   *        The location of the breakpoint as specified in the protocol.
   */
  _setBreakpoint: function TA__setBreakpoint(aLocation) {
    // Fetch the list of scripts in that url.
    let scripts = this._scripts[aLocation.url];
    // Fetch the specified script in that list.
    let script = null;
    for (let i = aLocation.line; i >= 0; i--) {
      // Stop when the first script that contains this location is found.
      if (scripts[i]) {
        // If that first script does not contain the line specified, it's no
        // good.
        if (i + scripts[i].lineCount < aLocation.line) {
          continue;
        }
        script = scripts[i];
        break;
      }
    }

    let location = { url: aLocation.url, line: aLocation.line };
    // Get the list of cached breakpoints in this URL.
    let scriptBreakpoints = this._breakpointStore[location.url];
    let bpActor;
    if (scriptBreakpoints &&
        scriptBreakpoints[location.line] &&
        scriptBreakpoints[location.line].actor) {
      bpActor = scriptBreakpoints[location.line].actor;
    }
    if (!bpActor) {
      bpActor = new BreakpointActor(this, location);
      this._hooks.addToBreakpointPool(bpActor);
      if (scriptBreakpoints[location.line]) {
        scriptBreakpoints[location.line].actor = bpActor;
      }
    }

    if (!script) {
      return { error: "noScript", actor: bpActor.actorID };
    }

    script = this._getInnermostContainer(script, aLocation.line);
    bpActor.addScript(script, this);

    let offsets = script.getLineOffsets(aLocation.line);
    let codeFound = false;
    for (let i = 0; i < offsets.length; i++) {
      script.setBreakpoint(offsets[i], bpActor);
      codeFound = true;
    }

    let actualLocation;
    if (offsets.length == 0) {
      // No code at that line in any script, skipping forward.
      let lines = script.getAllOffsets();
      let oldLine = aLocation.line;
      for (let line = oldLine; line < lines.length; ++line) {
        if (lines[line]) {
          for (let i = 0; i < lines[line].length; i++) {
            script.setBreakpoint(lines[line][i], bpActor);
            codeFound = true;
          }
          actualLocation = {
            url: aLocation.url,
            line: line,
            column: aLocation.column
          };
          bpActor.location = actualLocation;
          // Update the cache as well.
          scriptBreakpoints[line] = scriptBreakpoints[oldLine];
          scriptBreakpoints[line].line = line;
          delete scriptBreakpoints[oldLine];
          break;
        }
      }
    }
    if (!codeFound) {
      return  { error: "noCodeAtLineColumn", actor: bpActor.actorID };
    }

    return { actor: bpActor.actorID, actualLocation: actualLocation };
  },

  /**
   * Get the innermost script that contains this line, by looking through child
   * scripts of the supplied script.
   *
   * @param aScript Debugger.Script
   *        The source script.
   * @param aLine number
   *        The line number.
   */
  _getInnermostContainer: function TA__getInnermostContainer(aScript, aLine) {
    let children = aScript.getChildScripts();
    if (children.length > 0) {
      for (let i = 0; i < children.length; i++) {
        let child = children[i];
        // Stop when the first script that contains this location is found.
        if (child.startLine <= aLine &&
            child.startLine + child.lineCount > aLine) {
          return this._getInnermostContainer(child, aLine);
        }
      }
    }
    // Location not found in children, this is the innermost containing script.
    return aScript;
  },

  /**
   * Handle a protocol request to return the list of loaded scripts.
   */
  onScripts: function TA_onScripts(aRequest) {
    // Get the script list from the debugger.
    for (let s of this.dbg.findScripts()) {
      this._addScript(s);
    }
    // Build the cache.
    let scripts = [];
    for (let url in this._scripts) {
      for (let i = 0; i < this._scripts[url].length; i++) {
        if (!this._scripts[url][i]) {
          continue;
        }
        let script = {
          url: url,
          startLine: i,
          lineCount: this._scripts[url][i].lineCount
        };
        scripts.push(script);
      }
    }

    let packet = { from: this.actorID,
                   scripts: scripts };
    return packet;
  },

  /**
   * Handle a protocol request to pause the debuggee.
   */
  onInterrupt: function TA_onInterrupt(aRequest) {
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
      this._nest();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch(e) {
      Cu.reportError(e);
      return { error: "notInterrupted", message: e.toString() };
    }
  },

  /**
   * Return the Debug.Frame for a frame mentioned by the protocol.
   */
  _requestFrame: function TA_requestFrame(aFrameID) {
    if (!aFrameID) {
      return this._youngestFrame;
    }

    if (this._framePool.has(aFrameID)) {
      return this._framePool.get(aFrameID).frame;
    }

    return undefined;
  },

  _paused: function TA_paused(aFrame) {
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

    this._state = "paused";

    // Save the pause frame (if any) as the youngest frame for
    // stack viewing.
    this._youngestFrame = aFrame;

    // Create the actor pool that will hold the pause actor and its
    // children.
    dbg_assert(!this._pausePool);
    this._pausePool = new ActorPool(this.conn);
    this.conn.addActorPool(this._pausePool);

    // Give children of the pause pool a quick link back to the
    // thread...
    this._pausePool.threadActor = this;

    // Create the pause actor itself...
    dbg_assert(!this._pauseActor);
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

  _nest: function TA_nest() {
    if (this._hooks.preNest) {
      var nestData = this._hooks.preNest();
    }

    DebuggerServer.xpcInspector.enterNestedEventLoop();

    dbg_assert(this.state === "running");

    if (this._hooks.postNest) {
      this._hooks.postNest(nestData)
    }

    // "continue" resumption value.
    return undefined;
  },

  _resumed: function TA_resumed() {
    this._state = "running";

    // Drop the actors in the pause actor pool.
    this.conn.removeActorPool(this._pausePool);

    this._pausePool = null;
    this._pauseActor = null;
    this._youngestFrame = null;

    return { from: this.actorID, type: "resumed" };
  },

  /**
   * Expire frame actors for frames that have been popped.
   *
   * @returns A list of actor IDs whose frames have been popped.
   */
  _updateFrames: function TA_updateFrames() {
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

  _createFrameActor: function TA_createFrameActor(aFrame) {
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
  createEnvironmentActor:
  function TA_createEnvironmentActor(aEnvironment, aPool) {
    if (!aEnvironment) {
      return undefined;
    }

    if (aEnvironment.actor) {
      return aEnvironment.actor;
    }

    let actor = new EnvironmentActor(aEnvironment, this);
    this._environmentActors.push(actor);
    aPool.addActor(actor);
    aEnvironment.actor = actor;

    return actor;
  },

  /**
   * Create a grip for the given debuggee value.  If the value is an
   * object, will create a pause-lifetime actor.
   */
  createValueGrip: function TA_createValueGrip(aValue) {
    let type = typeof(aValue);
    if (type === "boolean" || type === "string" || type === "number") {
      return aValue;
    }

    if (aValue === null) {
      return { type: "null" };
    }

    if (aValue === undefined) {
      return { type: "undefined" }
    }

    if (typeof(aValue) === "object") {
      return this.pauseObjectGrip(aValue);
    }

    dbg_assert(false, "Failed to provide a grip for: " + aValue);
    return null;
  },

  /**
   * Return a protocol completion value representing the given
   * Debugger-provided completion value.
   */
  createProtocolCompletionValue:
  function TA_createProtocolCompletionValue(aCompletion) {
    let protoValue = {};
    if ("return" in aCompletion) {
      protoValue.return = this.createValueGrip(aCompletion.return);
    } else if ("yield" in aCompletion) {
      protoValue.return = this.createValueGrip(aCompletion.yield);
    } else if ("throw" in aCompletion) {
      protoValue.throw = this.createValueGrip(aCompletion.throw);
    } else {
      protoValue.terminated = true;
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
  objectGrip: function TA_objectGrip(aValue, aPool) {
    if (!aPool.objectActors) {
      aPool.objectActors = new WeakMap();
    }

    if (aPool.objectActors.has(aValue)) {
      return aPool.objectActors.get(aValue).grip();
    }

    let actor = new ObjectActor(aValue, this);
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
  pauseObjectGrip: function TA_pauseObjectGrip(aValue) {
    if (!this._pausePool) {
      throw "Object grip requested while not paused.";
    }

    return this.objectGrip(aValue, this._pausePool);
  },

  /**
   * Create a grip for the given debuggee object with a thread lifetime.
   *
   * @param aValue Debugger.Object
   *        The debuggee object value.
   */
  threadObjectGrip: function TA_threadObjectGrip(aValue) {
    return this.objectGrip(aValue, this.threadLifetimePool);
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
  uncaughtExceptionHook: function TA_uncaughtExceptionHook(aException) {
    dumpn("Got an exception:" + aException);
  },

  /**
   * A function that the engine calls when a debugger statement has been
   * executed in the specified frame.
   *
   * @param aFrame Debugger.Frame
   *        The stack frame that contained the debugger statement.
   */
  onDebuggerStatement: function TA_onDebuggerStatement(aFrame) {
    return this._pauseAndRespond(aFrame, { type: "debuggerStatement" });
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
  onExceptionUnwind: function TA_onExceptionUnwind(aFrame, aValue) {
    try {
      let packet = this._paused(aFrame);
      if (!packet) {
        return undefined;
      }

      packet.why = { type: "exception",
                     exception: this.createValueGrip(aValue) };
      this.conn.send(packet);
      return this._nest();
    } catch(e) {
      Cu.reportError("Got an exception during TA_onExceptionUnwind: " + e +
                     ": " + e.stack);
      return undefined;
    }
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
  onNewScript: function TA_onNewScript(aScript, aGlobal) {
    this._addScript(aScript);
    // Notify the client.
    this.conn.send({
      from: this.actorID,
      type: "newScript",
      url: aScript.url,
      startLine: aScript.startLine,
      lineCount: aScript.lineCount
    });
  },

  /**
   * Add the provided script to the server cache.
   *
   * @param aScript Debugger.Script
   *        The source script that will be stored.
   */
  _addScript: function TA__addScript(aScript) {
    // Ignore XBL bindings for content debugging.
    if (aScript.url.indexOf("chrome://") == 0) {
      return;
    }
    // Ignore about:* pages for content debugging.
    if (aScript.url.indexOf("about:") == 0) {
      return;
    }
    // Use a sparse array for storing the scripts for each URL in order to
    // optimize retrieval.
    if (!this._scripts[aScript.url]) {
      this._scripts[aScript.url] = [];
    }
    this._scripts[aScript.url][aScript.startLine] = aScript;

    // Set any stored breakpoints.
    let existing = this._breakpointStore[aScript.url];
    if (existing) {
      let endLine = aScript.startLine + aScript.lineCount - 1;
      // Iterate over the lines backwards, so that sliding breakpoints don't
      // affect the loop.
      for (let line = existing.length - 1; line >= 0; line--) {
        let bp = existing[line];
        // Limit search to the line numbers contained in the new script.
        if (bp && line >= aScript.startLine && line <= endLine) {
          this._setBreakpoint(bp);
        }
      }
    }
  }

};

ThreadActor.prototype.requestTypes = {
  "attach": ThreadActor.prototype.onAttach,
  "detach": ThreadActor.prototype.onDetach,
  "resume": ThreadActor.prototype.onResume,
  "clientEvaluate": ThreadActor.prototype.onClientEvaluate,
  "frames": ThreadActor.prototype.onFrames,
  "interrupt": ThreadActor.prototype.onInterrupt,
  "releaseMany": ThreadActor.prototype.onReleaseMany,
  "setBreakpoint": ThreadActor.prototype.onSetBreakpoint,
  "scripts": ThreadActor.prototype.onScripts
};


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
 * Creates an actor for the specified object.
 *
 * @param aObj Debugger.Object
 *        The debuggee object.
 * @param aThreadActor ThreadActor
 *        The parent thread actor for this object.
 */
function ObjectActor(aObj, aThreadActor)
{
  this.obj = aObj;
  this.threadActor = aThreadActor;
}

ObjectActor.prototype = {
  actorPrefix: "obj",

  WRONG_STATE_RESPONSE: {
    error: "wrongState",
    message: "Object actors can only be accessed while the thread is paused."
  },

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function OA_grip() {
    return { "type": "object",
             "class": this.obj.class,
             "actor": this.actorID };
  },

  /**
   * Releases this actor from the pool.
   */
  release: function OA_release() {
    this.registeredPool.objectActors.delete(this.obj);
    this.registeredPool.removeActor(this.actorID);
  },

  /**
   * Handle a protocol request to provide the names of the properties defined on
   * the object and not its prototype.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onOwnPropertyNames: function OA_onOwnPropertyNames(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

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
  onPrototypeAndProperties: function OA_onPrototypeAndProperties(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

    let ownProperties = {};
    for each (let name in this.obj.getOwnPropertyNames()) {
      try {
        let desc = this.obj.getOwnPropertyDescriptor(name);
        ownProperties[name] = this._propertyDescriptor(desc);
      } catch (e if e.name == "NS_ERROR_XPC_BAD_OP_ON_WN_PROTO") {
        // Calling getOwnPropertyDescriptor on wrapped native prototypes is not
        // allowed.
        dumpn("Error while getting the property descriptor for " + name +
              ": " + e.name);
      }
    }
    return { from: this.actorID,
             prototype: this.threadActor.createValueGrip(this.obj.proto),
             ownProperties: ownProperties };
  },

  /**
   * Handle a protocol request to provide the prototype of the object.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPrototype: function OA_onPrototype(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

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
  onProperty: function OA_onProperty(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }
    if (!aRequest.name) {
      return { error: "missingParameter",
               message: "no property name was specified" };
    }

    let desc = this.obj.getOwnPropertyDescriptor(aRequest.name);
    return { from: this.actorID,
             descriptor: this._propertyDescriptor(desc) };
  },

  /**
   * A helper method that creates a property descriptor for the provided object,
   * properly formatted for sending in a protocol response.
   *
   * @param aObject object
   *        The object that the descriptor is generated for.
   */
  _propertyDescriptor: function OA_propertyDescriptor(aObject) {
    let descriptor = {};
    descriptor.configurable = aObject.configurable;
    descriptor.enumerable = aObject.enumerable;
    if (aObject.value !== undefined) {
      descriptor.writable = aObject.writable;
      descriptor.value = this.threadActor.createValueGrip(aObject.value);
    } else {
      descriptor.get = this.threadActor.createValueGrip(aObject.get);
      descriptor.set = this.threadActor.createValueGrip(aObject.set);
    }
    return descriptor;
  },

  /**
   * Handle a protocol request to provide the source code of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDecompile: function OA_onDecompile(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "decompile request is only valid for object grips " +
                        "with a 'Function' class." };
    }

    return { from: this.actorID,
             decompiledCode: this.obj.decompile(!!aRequest.pretty) };
  },

  /**
   * Handle a protocol request to provide the lexical scope of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onScope: function OA_onScope(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

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

    // XXX: the following call of env.form() won't work until bug 747514 lands.
    // We can't get to the frame that defined this function's environment,
    // neither here, nor during ObjectActor's construction. Luckily, we don't
    // use the 'scope' request in the debugger frontend.
    return { name: this.obj.name || null,
             scope: envActor.form(this.obj) };
  },

  /**
   * Handle a protocol request to provide the name and parameters of a function.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onNameAndParameters: function OA_onNameAndParameters(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

    if (this.obj.class !== "Function") {
      return { error: "objectNotFunction",
               message: "nameAndParameters request is only valid for object " +
                        "grips with a 'Function' class." };
    }

    return { name: this.obj.name || null,
             parameters: this.obj.parameterNames };
  },

  /**
   * Handle a protocol request to promote a pause-lifetime grip to a
   * thread-lifetime grip.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onThreadGrip: function OA_onThreadGrip(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

    return { threadGrip: this.threadActor.threadObjectGrip(this.obj) };
  },

  /**
   * Handle a protocol request to release a thread-lifetime grip.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onRelease: function OA_onRelease(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }
    if (this.registeredPool !== this.threadActor.threadLifetimePool) {
      return { error: "notReleasable",
               message: "only thread-lifetime actors can be released." };
    }

    this.release();
    return {};
  },
};

ObjectActor.prototype.requestTypes = {
  "nameAndParameters": ObjectActor.prototype.onNameAndParameters,
  "prototypeAndProperties": ObjectActor.prototype.onPrototypeAndProperties,
  "prototype": ObjectActor.prototype.onPrototype,
  "property": ObjectActor.prototype.onProperty,
  "ownPropertyNames": ObjectActor.prototype.onOwnPropertyNames,
  "scope": ObjectActor.prototype.onScope,
  "decompile": ObjectActor.prototype.onDecompile,
  "threadGrip": ObjectActor.prototype.onThreadGrip,
  "release": ObjectActor.prototype.onRelease,
};


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
  disconnect: function FA_disconnect() {
    this.conn.removeActorPool(this._frameLifetimePool);
    this._frameLifetimePool = null;
  },

  /**
   * Returns a frame form for use in a protocol message.
   */
  form: function FA_form() {
    let form = { actor: this.actorID,
                 type: this.frame.type };
    if (this.frame.type === "call") {
      form.callee = this.threadActor.createValueGrip(this.frame.callee);
      form.calleeName = getFunctionName(this.frame.callee);
    }

    let envActor = this.threadActor
                       .createEnvironmentActor(this.frame.environment,
                                               this.frameLifetimePool);
    form.environment = envActor ? envActor.form(this.frame) : envActor;
    form.this = this.threadActor.createValueGrip(this.frame.this);
    form.arguments = this._args();
    if (this.frame.script) {
      form.where = { url: this.frame.script.url,
                     line: this.frame.script.getOffsetLine(this.frame.offset) };
    }

    if (!this.frame.older) {
      form.oldest = true;
    }

    return form;
  },

  _args: function FA__args() {
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
  onPop: function FA_onPop(aRequest) {
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
function BreakpointActor(aThreadActor, aLocation)
{
  this.scripts = [];
  this.threadActor = aThreadActor;
  this.location = aLocation;
}

BreakpointActor.prototype = {
  actorPrefix: "breakpoint",

  /**
   * Called when this same breakpoint is added to another Debugger.Script
   * instance, in the case of a page reload.
   *
   * @param aScript Debugger.Script
   *        The new source script on which the breakpoint has been set.
   * @param ThreadActor aThreadActor
   *        The parent thread actor that contains this breakpoint.
   */
  addScript: function BA_addScript(aScript, aThreadActor) {
    this.threadActor = aThreadActor;
    this.scripts.push(aScript);
  },

  /**
   * A function that the engine calls when a breakpoint has been hit.
   *
   * @param aFrame Debugger.Frame
   *        The stack frame that contained the breakpoint.
   */
  hit: function BA_hit(aFrame) {
    // TODO: add the rest of the breakpoints on that line (bug 676602).
    let reason = { type: "breakpoint", actors: [ this.actorID ] };
    return this.threadActor._pauseAndRespond(aFrame, reason);
  },

  /**
   * Handle a protocol request to remove this breakpoint.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDelete: function BA_onDelete(aRequest) {
    // Remove from the breakpoint store.
    let scriptBreakpoints = this.threadActor._breakpointStore[this.location.url];
    delete scriptBreakpoints[this.location.line];
    // Remove the actual breakpoint.
    this.threadActor._hooks.removeFromBreakpointPool(this.actorID);
    for (let script of this.scripts) {
      script.clearBreakpoint(this);
    }
    this.scripts = null;

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
   * Returns an environment form for use in a protocol message. Note that the
   * requirement of passing the frame as a parameter is only temporary, since
   * when bug 747514 lands, the environment will have a callee property that
   * will contain it.
   *
   * @param Debugger.Frame aObject
   *        The stack frame object whose environment bindings are being
   *        generated.
   */
  form: function EA_form(aObject) {
    // Debugger.Frame might be dead by the time we get here, which will cause
    // accessing its properties to throw.
    if (!aObject.live) {
      return undefined;
    }

    let parent;
    if (this.obj.parent) {
      let thread = this.threadActor;
      parent = thread.createEnvironmentActor(this.obj.parent,
                                             this.registeredPool);
    }
    // Deduce the frame that created the parent scope in order to pass it to
    // parent.form(). TODO: this can be removed after bug 747514 is done.
    let parentFrame = aObject;
    if (this.obj.type == "declarative" && aObject.older) {
      parentFrame = aObject.older;
    }
    let form = { actor: this.actorID,
                 parent: parent ? parent.form(parentFrame) : parent };

    if (this.obj.type == "with") {
      form.type = "with";
      form.object = this.threadActor.createValueGrip(this.obj.object);
    } else if (this.obj.type == "object") {
      form.type = "object";
      form.object = this.threadActor.createValueGrip(this.obj.object);
    } else { // this.obj.type == "declarative"
      if (aObject.callee) {
        form.type = "function";
        form.function = this.threadActor.createValueGrip(aObject.callee);
        form.functionName = getFunctionName(aObject.callee);
      } else {
        form.type = "block";
      }
      form.bindings = this._bindings(aObject);
    }

    return form;
  },

  /**
   * Return the identifier bindings object as required by the remote protocol
   * specification. Note that the requirement of passing the frame as a
   * parameter is only temporary, since when bug 747514 lands, the environment
   * will have a callee property that will contain it.
   *
   * @param Debugger.Frame aObject [optional]
   *        The stack frame whose environment bindings are being generated. When
   *        left unspecified, the bindings do not contain an 'arguments'
   *        property.
   */
  _bindings: function EA_bindings(aObject) {
    let bindings = { arguments: [], variables: {} };

    // TODO: this part should be removed in favor of the commented-out part
    // below when getVariableDescriptor lands (bug 725815).
    if (typeof this.obj.getVariable != "function") {
    //if (typeof this.obj.getVariableDescriptor != "function") {
      return bindings;
    }

    let parameterNames;
    if (aObject && aObject.callee) {
      parameterNames = aObject.callee.parameterNames;
    }
    for each (let name in parameterNames) {
      let arg = {};
      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands (bug 725815).
      let desc = {
        value: this.obj.getVariable(name),
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

      // TODO: this part should be removed in favor of the commented-out part
      // below when getVariableDescriptor lands.
      let desc = {
        configurable: false,
        writable: true,
        enumerable: true
      };
      try {
        desc.value = this.obj.getVariable(name);
      } catch (e) {
        // Avoid "Debugger scope is not live" errors for |arguments|, introduced
        // in bug 746601.
        if (name != "arguments") {
          throw e;
        }
      }
      //let desc = this.obj.getVariableDescriptor(name);
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
  onAssign: function EA_onAssign(aRequest) {
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
    } catch (e) {
      if (e instanceof Debugger.DebuggeeWouldRun) {
        return { error: "threadWouldRun",
                 cause: e.cause ? e.cause : "setter",
                 message: "Assigning a value would cause the debuggee to run" };
      }
      // This should never happen, so let it complain loudly if it does.
      throw e;
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
  onBindings: function EA_onBindings(aRequest) {
    return { from: this.actorID,
             bindings: this._bindings() };
  }
};

EnvironmentActor.prototype.requestTypes = {
  "assign": EnvironmentActor.prototype.onAssign,
  "bindings": EnvironmentActor.prototype.onBindings
};

/**
 * Helper function to deduce the name of the provided function.
 *
 * @param Debugger.Object aFunction
 *        The function whose name will be returned.
 */
function getFunctionName(aFunction) {
  let name;
  if (aFunction.name) {
    name = aFunction.name;
  } else {
    let desc = aFunction.getOwnPropertyDescriptor("displayName");
    if (desc && desc.value && typeof desc.value == "string") {
      name = desc.value;
    }
  }
  return name;
}
