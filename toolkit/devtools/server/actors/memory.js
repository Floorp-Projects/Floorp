/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
let protocol = require("devtools/server/protocol");
let { method, RetVal, Arg, types } = protocol;
const { reportException } = require("devtools/toolkit/DevToolsUtils");
loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "StackFrameCache",
                         "devtools/server/actors/utils/stack", true);

/**
 * A method decorator that ensures the actor is in the expected state before
 * proceeding. If the actor is not in the expected state, the decorated method
 * returns a rejected promise.
 *
 * @param String expectedState
 *        The expected state.
 *
 * @param Function method
 *        The actor method to proceed with when the actor is in the expected
 *        state.
 *
 * @returns Function
 *          The decorated method.
 */
function expectState(expectedState, method) {
  return function(...args) {
    if (this.state !== expectedState) {
      const msg = "Wrong State: Expected '" + expectedState + "', but current "
                + "state is '" + this.state + "'";
      return Promise.reject(new Error(msg));
    }

    return method.apply(this, args);
  };
}

types.addDictType("AllocationsRecordingOptions", {
  // The probability we sample any given allocation when recording
  // allocations. Must be between 0.0 and 1.0. Defaults to 1.0, or sampling
  // every allocation.
  probability: "number"
});

/**
 * An actor that returns memory usage data for its parent actor's window.
 * A tab-scoped instance of this actor will measure the memory footprint of its
 * parent tab. A global-scoped instance however, will measure the memory
 * footprint of the chrome window referenced by the root actor.
 */
let MemoryActor = protocol.ActorClass({
  typeName: "memory",

  get dbg() {
    if (!this._dbg) {
      this._dbg = this.parent.makeDebugger();
    }
    return this._dbg;
  },

  initialize: function(conn, parent, frameCache = new StackFrameCache()) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.parent = parent;
    this._mgr = Cc["@mozilla.org/memory-reporter-manager;1"]
                  .getService(Ci.nsIMemoryReporterManager);
    this.state = "detached";
    this._dbg = null;
    this._frameCache = frameCache;
    this._onWindowReady = this._onWindowReady.bind(this);

    events.on(this.parent, "window-ready", this._onWindowReady);
  },

  destroy: function() {
    events.off(this.parent, "window-ready", this._onWindowReady);

    this._mgr = null;
    if (this.state === "attached") {
      this.detach();
    }
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Attach to this MemoryActor.
   */
  attach: method(expectState("detached", function() {
    this.dbg.addDebuggees();
    this.state = "attached";
  }), {
    request: {},
    response: {
      type: "attached"
    }
  }),

  /**
   * Detach from this MemoryActor.
   */
  detach: method(expectState("attached", function() {
    this._clearDebuggees();
    this.dbg.enabled = false;
    this._dbg = null;
    this.state = "detached";
  }), {
    request: {},
    response: {
      type: "detached"
    }
  }),

  _clearDebuggees: function() {
    if (this._dbg) {
      if (this.dbg.memory.trackingAllocationSites) {
        this.dbg.memory.drainAllocationsLog();
      }
      this._clearFrames();
      this.dbg.removeAllDebuggees();
    }
  },

  _clearFrames: function() {
    if (this.dbg.memory.trackingAllocationSites) {
      this._frameCache.clearFrames();
    }
  },

  /**
   * Handler for the parent actor's "window-ready" event.
   */
  _onWindowReady: function({ isTopLevel }) {
    if (this.state == "attached") {
      if (isTopLevel && this.dbg.memory.trackingAllocationSites) {
        this._clearDebuggees();
        this._frameCache.initFrames();
      }
      this.dbg.addDebuggees();
    }
  },

  /**
   * Take a census of the heap. See js/src/doc/Debugger/Debugger.Memory.md for
   * more information.
   */
  takeCensus: method(expectState("attached", function() {
    return this.dbg.memory.takeCensus();
  }), {
    request: {},
    response: RetVal("json")
  }),

  /**
   * Start recording allocation sites.
   *
   * @param AllocationsRecordingOptions options
   *        See the protocol.js definition of AllocationsRecordingOptions above.
   */
  startRecordingAllocations: method(expectState("attached", function(options = {}) {
    this._frameCache.initFrames();
    this.dbg.memory.allocationSamplingProbability = options.probability != null
      ? options.probability
      : 1.0;
    this.dbg.memory.trackingAllocationSites = true;

    return Date.now();
  }), {
    request: {
      options: Arg(0, "nullable:AllocationsRecordingOptions")
    },
    response: {
      // Accept `nullable` in the case of server Gecko <= 37, handled on the front
      value: RetVal(0, "nullable:number")
    }
  }),

  /**
   * Stop recording allocation sites.
   */
  stopRecordingAllocations: method(expectState("attached", function() {
    this.dbg.memory.trackingAllocationSites = false;
    this._clearFrames();

    return Date.now();
  }), {
    request: {},
    response: {
      // Accept `nullable` in the case of server Gecko <= 37, handled on the front
      value: RetVal(0, "nullable:number")
    }
  }),

  /**
   * Get a list of the most recent allocations since the last time we got
   * allocations, as well as a summary of all allocations since we've been
   * recording.
   *
   * @returns Object
   *          An object of the form:
   *
   *            {
   *              allocations: [<index into "frames" below>, ...],
   *              allocationsTimestamps: [
   *                <timestamp for allocations[0]>,
   *                <timestamp for allocations[1]>,
   *                ...
   *              ],
   *              frames: [
   *                {
   *                  line: <line number for this frame>,
   *                  column: <column number for this frame>,
   *                  source: <filename string for this frame>,
   *                  functionDisplayName: <this frame's inferred function name function or null>,
   *                  parent: <index into "frames">
   *                },
   *                ...
   *              ],
   *              counts: [
   *                <number of allocations in frames[0]>,
   *                <number of allocations in frames[1]>,
   *                <number of allocations in frames[2]>,
   *                ...
   *              ]
   *            }
   *
   *          The timestamps' unit is microseconds since the epoch.
   *
   *          Subsequent `getAllocations` request within the same recording and
   *          tab navigation will always place the same stack frames at the same
   *          indices as previous `getAllocations` requests in the same
   *          recording. In other words, it is safe to use the index as a
   *          unique, persistent id for its frame.
   *
   *          Additionally, the root node (null) is always at index 0.
   *
   *          Note that the allocation counts include "self" allocations only,
   *          and don't account for allocations in child frames.
   *
   *          We use the indices into the "frames" array to avoid repeating the
   *          description of duplicate stack frames both when listing
   *          allocations, and when many stacks share the same tail of older
   *          frames. There shouldn't be any duplicates in the "frames" array,
   *          as that would defeat the purpose of this compression trick.
   *
   *          In the future, we might want to split out a frame's "source" and
   *          "functionDisplayName" properties out the same way we have split
   *          frames out with the "frames" array. While this would further
   *          compress the size of the response packet, it would increase CPU
   *          usage to build the packet, and it should, of course, be guided by
   *          profiling and done only when necessary.
   */
  getAllocations: method(expectState("attached", function() {
    const allocations = this.dbg.memory.drainAllocationsLog()
    const packet = {
      allocations: [],
      allocationsTimestamps: []
    };

    for (let { frame: stack, timestamp } of allocations) {
      if (stack && Cu.isDeadWrapper(stack)) {
        continue;
      }

      // Safe because SavedFrames are frozen/immutable.
      let waived = Cu.waiveXrays(stack);

      // Ensure that we have a form, count, and index for new allocations
      // because we potentially haven't seen some or all of them yet. After this
      // loop, we can rely on the fact that every frame we deal with already has
      // its metadata stored.
      let index = this._frameCache.addFrame(waived);

      packet.allocations.push(index);
      packet.allocationsTimestamps.push(timestamp);
    }

    return this._frameCache.updateFramePacket(packet);
  }), {
    request: {},
    response: RetVal("json")
  }),

  /*
   * Force a browser-wide GC.
   */
  forceGarbageCollection: method(function() {
    for (let i = 0; i < 3; i++) {
      Cu.forceGC();
    }
  }, {
    request: {},
    response: {}
  }),

  /**
   * Force an XPCOM cycle collection. For more information on XPCOM cycle
   * collection, see
   * https://developer.mozilla.org/en-US/docs/Interfacing_with_the_XPCOM_cycle_collector#What_the_cycle_collector_does
   */
  forceCycleCollection: method(function() {
    Cu.forceCC();
  }, {
    request: {},
    response: {}
  }),

  /**
   * A method that returns a detailed breakdown of the memory consumption of the
   * associated window.
   *
   * @returns object
   */
  measure: method(function() {
    let result = {};

    let jsObjectsSize = {};
    let jsStringsSize = {};
    let jsOtherSize = {};
    let domSize = {};
    let styleSize = {};
    let otherSize = {};
    let totalSize = {};
    let jsMilliseconds = {};
    let nonJSMilliseconds = {};

    try {
      this._mgr.sizeOfTab(this.parent.window, jsObjectsSize, jsStringsSize, jsOtherSize,
                          domSize, styleSize, otherSize, totalSize, jsMilliseconds, nonJSMilliseconds);
      result.total = totalSize.value;
      result.domSize = domSize.value;
      result.styleSize = styleSize.value;
      result.jsObjectsSize = jsObjectsSize.value;
      result.jsStringsSize = jsStringsSize.value;
      result.jsOtherSize = jsOtherSize.value;
      result.otherSize = otherSize.value;
      result.jsMilliseconds = jsMilliseconds.value.toFixed(1);
      result.nonJSMilliseconds = nonJSMilliseconds.value.toFixed(1);
    } catch (e) {
      reportException("MemoryActor.prototype.measure", e);
    }

    return result;
  }, {
    request: {},
    response: RetVal("json"),
  }),

  residentUnique: method(function() {
    return this._mgr.residentUnique;
  }, {
    request: {},
    response: { value: RetVal("number") }
  })
});

exports.MemoryActor = MemoryActor;

exports.MemoryFront = protocol.FrontClass(MemoryActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.memoryActor;
    this.manage(this);
  }
});
