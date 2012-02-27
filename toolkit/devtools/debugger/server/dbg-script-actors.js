/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Panos Astithas <past@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *        and exiting a nested event loop.
 */
function ThreadActor(aHooks)
{
  this._state = "detached";
  this._frameActors = [];
  this._environmentActors = [];
  this._hooks = aHooks ? aHooks : {};
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

  _breakpointPool: null,
  get breakpointActorPool() {
    if (!this._breakpointPool) {
      this._breakpointPool = new ActorPool(this.conn);
      this.conn.addActorPool(this._breakpointPool);
    }
    return this._breakpointPool;
  },

  _scripts: {},

  /**
   * Add a debuggee global to the JSInspector.
   */
  addDebuggee: function TA_addDebuggee(aGlobal) {
    // Use the inspector xpcom component to turn on debugging
    // for aGlobal's compartment.  Ideally this won't be necessary
    // medium- to long-term, and will be managed by the engine
    // instead.

    if (!this._dbg) {
      this._dbg = new Debugger();
    }

    // TODO: Remove this horrible hack when bug 723563 is fixed.
    // Make sure that a chrome window is not added as a debuggee when opening
    // the debugger in an empty tab or during tests.
    if (aGlobal.location &&
        (aGlobal.location.protocol == "about:" ||
         aGlobal.location.protocol == "chrome:")) {
      return;
    }

    this.dbg.addDebuggee(aGlobal);
    this.dbg.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
    this.dbg.onDebuggerStatement = this.onDebuggerStatement.bind(this);
    this.dbg.onNewScript = this.onNewScript.bind(this);
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
    this.conn.removeActorPool(this._breakpointPool);
    this._breakpointPool = null;
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

  onResume: function TA_onResume(aRequest) {
    let packet = this._resumed();
    DebuggerServer.xpcInspector.exitNestedEventLoop();
    return packet;
  },

  onClientEvaluate: function TA_onClientEvaluate(aRequest) {
    if (this.state !== "paused") {
      return { type: "wrongState",
               message: "Debuggee must be paused to evaluate code." };
    };

    let frame = this._requestFrame(aRequest.frame);
    if (!frame) {
      // XXXspec
      return { type: "unknownFrame",
               message: "Evaluation frame not found" };
    }


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
    packet.why = { type: "clientEvaluated" };
    if ("return" in completion) {
      packet.why.value = this.createValueGrip(completion["return"]);
    } else if ("throw" in completion) {
      packet.why.exception = this.createValueGrip(completion["throw"]);
    } else {
      // XXXspec
      packet.why.terminated = true;
    }

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
      let grip = this._createFrameActor(frame).grip();
      grip.depth = i;
      frames.push(grip);
      frame = frame.older;
    }

    return { frames: frames };
  },

  onReleaseMany: function TA_onReleaseMany(aRequest) {
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
    if (!this._scripts[location.url] || location.line < 0) {
      return { error: "noScript" };
    }
    // Fetch the list of scripts in that url.
    let scripts = this._scripts[location.url];
    // Fetch the specified script in that list.
    let script = null;
    for (let i = location.line; i >= 0; i--) {
      // Stop when the first script that contains this location is found.
      if (scripts[i]) {
        // If that first script does not contain the line specified, it's no
        // good.
        if (i + scripts[i].lineCount < location.line) {
          break;
        }
        script = scripts[i];
        break;
      }
    }

    if (!script) {
      return { error: "noScript" };
    }

    script = this._getInnermostContainer(script, location.line);
    let bpActor = new BreakpointActor(script, this);
    this.breakpointActorPool.addActor(bpActor);

    let offsets = script.getLineOffsets(location.line);
    let codeFound = false;
    for (let i = 0; i < offsets.length; i++) {
      script.setBreakpoint(offsets[i], bpActor);
      codeFound = true;
    }

    let actualLocation;
    if (offsets.length == 0) {
      // No code at that line in any script, skipping forward.
      let lines = script.getAllOffsets();
      for (let line = location.line; line < lines.length; ++line) {
        if (lines[line]) {
          for (let i = 0; i < lines[line].length; i++) {
            script.setBreakpoint(lines[line][i], bpActor);
            codeFound = true;
          }
          actualLocation = location;
          actualLocation.line = line;
          break;
        }
      }
    }
    if (!codeFound) {
      bpActor.onDelete();
      return  { error: "noCodeAtLineColumn" };
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
  onInterrupt: function TA_onScripts(aRequest) {
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
    // XXXspec: doesn't actually specify how frames are named.  By
    // depth?  By actor?  Both?
    if (!aFrameID) {
      return this._youngestFrame;
    }

    if (this._framePool.has(aFrameID)) {
      return this._framePool.get(aFrameID).frame;
    }

    return undefined;
  },

  _paused: function TA_paused(aFrame) {
    // XXX: We don't handle nested pauses correctly.  Don't try - if we're
    // paused, just continue running whatever code triggered the pause.

    // We don't want to actually have nested pauses (although we will
    // have nested event loops).  If code runs in the debuggee during
    // a pause, it should cause the actor to resume (dropping
    // pause-lifetime actors etc) and then repause when complete.

    if (this.state === "paused") {
      return undefined;
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
      packet.frame = this._createFrameActor(aFrame).grip();
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
   * Create and return an environment actor that corresponds to the
   * Debugger.Environment for the provided object.
   * @param Debugger.Object aObject
   *        The object whose lexical environment we want to extract.
   * @param object aPool
   *        The pool where the newly-created actor will be placed.
   * @return The EnvironmentActor for aObject.
   */
  createEnvironmentActor: function TA_createEnvironmentActor(aObject, aPool) {
    let environment = aObject.environment;
    // XXX: need to spec this: when the object is a function proxy or not a
    // function implemented in JavaScript, we don't return a scope property at
    // all.
    if (!environment) {
      return undefined;
    }

    if (environment.actor) {
      return environment.actor;
    }

    let actor = new EnvironmentActor(aObject, this);
    this._environmentActors.push(actor);
    aPool.addActor(actor);
    environment.actor = actor;

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
    try {
      let packet = this._paused(aFrame);
      if (!packet) {
        return undefined;
      }
      packet.why = { type: "debuggerStatement" };
      this.conn.send(packet);
      return this._nest();
    } catch(e) {
      Cu.reportError("Got an exception during onDebuggerStatement: " + e +
                     ": " + e.stack);
      return undefined;
    }
  },

  /**
   * A function that the engine calls when a new script has been loaded into a
   * debuggee compartment. If the new code is part of a function, aFunction is
   * a Debugger.Object reference to the function object. (Not all code is part
   * of a function; for example, the code appearing in a <script> tag that is
   * outside of any functions defined in that tag would be passed to
   * onNewScript without an accompanying function argument.)
   *
   * @param aScript Debugger.Script
   *        The source script that has been loaded into a debuggee compartment.
   * @param aFunction Debugger.Object
   *        The function object that the ew code is part of.
   */
  onNewScript: function TA_onNewScript(aScript, aFunction) {
    // Use a sparse array for storing the scripts for each URL in order to
    // optimize retrieval. XXX: in case this is not fast enough for very large
    // files with too many scripts, we could sort the hash of script locations
    // or use a trie.
    if (!this._scripts[aScript.url]) {
      this._scripts[aScript.url] = [];
    }
    this._scripts[aScript.url][aScript.startLine] = aScript;
    // Notify the client.
    this.conn.send({ from: this.actorID, type: "newScript",
                     url: aScript.url, startLine: aScript.startLine });
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
             "class": this.obj["class"],
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
    // XXX: spec this.
    if (!aRequest.name) {
      return { error: "noPropertyName",
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
    if (aObject.value) {
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

    if (this.obj["class"] !== "Function") {
      // XXXspec: Error type for this.
      return { error: "unrecognizedPacketType",
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

    if (this.obj["class"] !== "Function") {
      // XXXspec: Error type for this.
      return { error: "unrecognizedPacketType",
               message: "scope request is only valid for object grips with a" +
                        " 'Function' class." };
    }

    let packet = { name: this.obj.name || null };
    let envActor = this.threadActor.createEnvironmentActor(this.obj, this.registeredPool);
    packet.scope = envActor ? envActor.grip() : envActor;

    return packet;
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

    if (this.obj["class"] !== "Function") {
      // XXXspec: Error type for this.
      return { error: "unrecognizedPacketType",
               message: "nameAndParameters request is only valid for object grips with a 'Function' class." };
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
      // XXXspec: error type?
      return { error: "unrecognizedPacketType",
               message: "release is only recognized on thread-lifetime actors." };
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
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function FA_grip() {
    let grip = { actor: this.actorID,
                 type: this.frame.type };
    if (this.frame.type === "call") {
      grip.callee = this.threadActor.createValueGrip(this.frame.callee);
      grip.calleeName = this.frame.callee.name;
    }

    let envActor = this.threadActor
                       .createEnvironmentActor(this.frame,
                                               this.frameLifetimePool);
    grip.environment = envActor ? envActor.grip() : envActor;
    grip["this"] = this.threadActor.createValueGrip(this.frame["this"]);
    grip.arguments = this._args();
    if (this.frame.script) {
      grip.where = { url: this.frame.script.url,
                     line: this.frame.script.getOffsetLine(this.frame.offset) };
    }

    if (!this.frame.older) {
      grip.oldest = true;
    }

    return grip;
  },

  _args: function FA__args() {
    if (!this.frame["arguments"]) {
      return [];
    }

    return [this.threadActor.createValueGrip(arg)
            for each (arg in this.frame["arguments"])];
  },

  /**
   * Handle a protocol request to pop this frame from the stack.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onPop: function FA_onPop(aRequest) {
    return { error: "notImplemented",
             message: "Popping frames is not yet implemented." };
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
 * @param Debugger.Script aScript
 *        The script this breakpoint is set on.
 * @param ThreadActor aThreadActor
 *        The parent thread actor that contains this breakpoint.
 */
function BreakpointActor(aScript, aThreadActor)
{
  this.script = aScript;
  this.threadActor = aThreadActor;
}

BreakpointActor.prototype = {
  actorPrefix: "breakpoint",

  /**
   * A function that the engine calls when a breakpoint has been hit.
   *
   * @param aFrame Debugger.Frame
   *        The stack frame that contained the breakpoint.
   */
  hit: function BA_hit(aFrame) {
    try {
      let packet = this.threadActor._paused(aFrame);
      if (!packet) {
        return undefined;
      }
      // TODO: add the rest of the breakpoints on that line.
      packet.why = { type: "breakpoint", actors: [ this.actorID ] };
      this.conn.send(packet);
      return this.threadActor._nest();
    } catch(e) {
      Cu.reportError("Got an exception during hit: " + e + ': ' + e.stack);
      return undefined;
    }
  },

  /**
   * Handle a protocol request to remove this breakpoint.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDelete: function BA_onDelete(aRequest) {
    this.threadActor.breakpointActorPool.removeActor(this.actorID);
    this.script.clearBreakpoint(this);
    this.script = null;

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
 * @param Debugger.Object aObject
 *        The object whose lexical environment will be used to create the actor.
 * @param ThreadActor aThreadActor
 *        The parent thread actor that contains this environment.
 */
function EnvironmentActor(aObject, aThreadActor)
{
  this.obj = aObject;
  this.threadActor = aThreadActor;
}

EnvironmentActor.prototype = {
  actorPrefix: "environment",

  /**
   * Returns a grip for this actor for returning in a protocol message.
   */
  grip: function EA_grip() {
    // Debugger.Frame might be dead by the time we get here, which will cause
    // accessing its properties to throw.
    if (!this.obj.live) {
      return undefined;
    }

    let parent;
    if (this.obj.environment.parent) {
      parent = this.threadActor
                   .createEnvironmentActor(this.obj.environment.parent,
                                           this.registeredPool);
    }
    let grip = { actor: this.actorID,
                 parent: parent ? parent.grip() : parent };

    if (this.obj.environment.type == "object") {
      grip.type = "object"; // XXX: how can we tell if it's "with"?
      grip.object = this.threadActor.createValueGrip(this.obj.environment.object);
    } else {
      if (this.obj["class"] == "Function") {
        grip.type = "function";
        grip["function"] = this.threadActor.createValueGrip(this.obj);
        grip.functionName = this.obj.name;
      } else {
        grip.type = "block";
      }

      grip.bindings = this._bindings();
    }

    return grip;
  },

  /**
   * Return the identifier bindings object as required by the remote protocol
   * specification.
   */
  _bindings: function EA_bindings() {
    let bindings = { mutable: {}, immutable: {} };

    // TODO: this will be redundant after bug 692984 is fixed.
    if (typeof this.obj.environment.getVariableDescriptor != "function") {
      return bindings;
    }

    for (let name in this.obj.environment.names()) {
      let desc = this.obj.environment.getVariableDescriptor(name);
      // XXX: the spec doesn't say what to do with accessor properties.
      if (desc.writable) {
        grip.bindings.mutable[name] = desc.value;
      } else {
        grip.bindings.immutable[name] = desc.value;
      }
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
    let desc = this.obj.environment.getVariableDescriptor(aRequest.name);

    if (!desc.writable) {
      return { error: "immutableBinding",
               message: "Changing the value of an immutable binding is not " +
                        "allowed" };
    }

    try {
      this.obj.environment.setVariable(aRequest.name, aRequest.value);
    } catch (e) {
      if (e instanceof Debugger.DebuggeeWouldRun) {
        // XXX: we need to spec this. Is this a real problem?
        return { error: "debuggeeWouldRun",
                 message: "Assigning this value would cause the debuggee to run." };
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
