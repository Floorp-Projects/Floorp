/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/server/protocol");
const { method, RetVal, Arg, types } = protocol;
const { MemoryBridge } = require("./utils/memory-bridge");
loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "StackFrameCache",
                         "devtools/server/actors/utils/stack", true);

/**
 * Proxies a call to the MemoryActor to the underlying MemoryBridge,
 * allowing access to MemoryBridge features by defining the RDP
 * request/response signature.
 */
function linkBridge (methodName, definition) {
  return method(function () {
    return this.bridge[methodName].apply(this.bridge, arguments);
  }, definition);
}

types.addDictType("AllocationsRecordingOptions", {
  // The probability we sample any given allocation when recording
  // allocations. Must be between 0.0 and 1.0. Defaults to 1.0, or sampling
  // every allocation.
  probability: "number",

  // The maximum number of of allocation events to keep in the allocations
  // log. If new allocations arrive, when we are already at capacity, the oldest
  // allocation event is lost. This number must fit in a 32 bit signed integer.
  maxLogLength: "number"
});

/**
 * An actor that returns memory usage data for its parent actor's window.
 * A tab-scoped instance of this actor will measure the memory footprint of its
 * parent tab. A global-scoped instance however, will measure the memory
 * footprint of the chrome window referenced by the root actor.
 */
let MemoryActor = protocol.ActorClass({
  typeName: "memory",

  /**
   * The set of unsolicited events the MemoryActor emits that will be sent over
   * the RDP (by protocol.js).
   */

  events: {
    // Same format as the data passed to the
    // `Debugger.Memory.prototype.onGarbageCollection` hook. See
    // `js/src/doc/Debugger/Debugger.Memory.md` for documentation.
    "garbage-collection": {
      type: "garbage-collection",
      data: Arg(0, "json"),
    },
  },

  initialize: function(conn, parent, frameCache = new StackFrameCache()) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this._onGarbageCollection = this._onGarbageCollection.bind(this);
    this.bridge = new MemoryBridge(parent, frameCache);
    this.bridge.on("garbage-collection", this._onGarbageCollection);
  },

  destroy: function() {
    this.bridge.off("garbage-collection", this._onGarbageCollection);
    this.bridge.destroy();
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Attach to this MemoryActor.
   *
   * This attaches the MemoryActor's Debugger instance so that you can start
   * recording allocations or take a census of the heap. In addition, the
   * MemoryActor will start emitting GC events.
   */
  attach: linkBridge("attach", {
    request: {},
    response: {
      type: "attached"
    }
  }),

  /**
   * Detach from this MemoryActor.
   */
  detach: linkBridge("detach", {
    request: {},
    response: {
      type: "detached"
    }
  }),

  /**
   * Gets the current MemoryActor attach/detach state.
   */
  getState: linkBridge("getState", {
    response: {
      state: RetVal(0, "string")
    }
  }),

  /**
   * Take a census of the heap. See js/src/doc/Debugger/Debugger.Memory.md for
   * more information.
   */
  takeCensus: linkBridge("takeCensus", {
    request: {},
    response: RetVal("json")
  }),

  /**
   * Start recording allocation sites.
   *
   * @param AllocationsRecordingOptions options
   *        See the protocol.js definition of AllocationsRecordingOptions above.
   */
  startRecordingAllocations: linkBridge("startRecordingAllocations", {
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
  stopRecordingAllocations: linkBridge("stopRecordingAllocations", {
    request: {},
    response: {
      // Accept `nullable` in the case of server Gecko <= 37, handled on the front
      value: RetVal(0, "nullable:number")
    }
  }),

  /**
   * Return settings used in `startRecordingAllocations` for `probability`
   * and `maxLogLength`. Currently only uses in tests.
   */
  getAllocationsSettings: linkBridge("getAllocationsSettings", {
    request: {},
    response: {
      options: RetVal(0, "json")
    }
  }),

  getAllocations: linkBridge("getAllocations", {
    request: {},
    response: RetVal("json")
  }),

  /*
   * Force a browser-wide GC.
   */
  forceGarbageCollection: linkBridge("forceGarbageCollection", {
    request: {},
    response: {}
  }),

  /**
   * Force an XPCOM cycle collection. For more information on XPCOM cycle
   * collection, see
   * https://developer.mozilla.org/en-US/docs/Interfacing_with_the_XPCOM_cycle_collector#What_the_cycle_collector_does
   */
  forceCycleCollection: linkBridge("forceCycleCollection", {
    request: {},
    response: {}
  }),

  /**
   * A method that returns a detailed breakdown of the memory consumption of the
   * associated window.
   *
   * @returns object
   */
  measure: linkBridge("measure", {
    request: {},
    response: RetVal("json"),
  }),

  residentUnique: linkBridge("residentUnique", {
    request: {},
    response: { value: RetVal("number") }
  }),

  /**
   * Called when the underlying MemoryBridge fires a "garbage-collection" events.
   * Propagates over RDP.
   */
  _onGarbageCollection: function (data) {
    events.emit(this, "garbage-collection", data);
  },
});

exports.MemoryActor = MemoryActor;

exports.MemoryFront = protocol.FrontClass(MemoryActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.memoryActor;
    this.manage(this);
  }
});
