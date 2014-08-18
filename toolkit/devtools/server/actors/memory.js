/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
let protocol = require("devtools/server/protocol");
let { method, RetVal, Arg } = protocol;
const { reportException } = require("devtools/toolkit/DevToolsUtils");

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

  initialize: function(conn, parent) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.parent = parent;
    this._mgr = Cc["@mozilla.org/memory-reporter-manager;1"]
                  .getService(Ci.nsIMemoryReporterManager);
    this.state = "detached";
    this._dbg = null;
  },

  destroy: function() {
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
    this.dbg.removeAllDebuggees();
    this.dbg.enabled = false;
    this._dbg = null;
    this.state = "detached";
  }), {
    request: {},
    response: {
      type: "detached"
    }
  }),

  /**
   * Enable or disable the recording of allocation sites.
   *
   * @param Boolean shouldRecord
   *        True to enable recording, false to disable it.
   */
  recordAllocations: method(expectState("attached", function(shouldRecord) {
    this.dbg.memory.trackingAllocationSites = shouldRecord;
  }), {
    request: {
      shouldRecord: Arg(0, "boolean")
    },
    response: {}
  }),

  /**
   * Get a list of the most recent allocations since the last time we got
   * allocations
   *
   * @returns Object
   *          An object of the form:
   *
   *            {
   *              allocations: [<index into "frames" below> ...],
   *              frames: [
   *                {
   *                  line: <line number for this frame>,
   *                  column: <column number for this frame>,
   *                  source: <filename string for this frame>,
   *                  functionDisplayName: <this frame's inferred function name function or null>,
   *                  parent: <index into "frames">
   *                }
   *                ...
   *              ]
   *            }
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
    const packet = {
      frames: [],
      allocations: []
    };

    const framesToIndices = new Map();

    for (let stack of this.dbg.memory.flushAllocationsLog()) {
      packet.allocations.push(this._insertStackInAllocationsPacket(stack,
                                                                   packet,
                                                                   framesToIndices));
    }

    return packet;
  }), {
    request: {},
    response: RetVal("json")
  }),

  /**
   * Inserts each frame in the stack into the given RDP allocations packet.
   *
   * @param SavedFrame frame
   *        A frame to insert into the packet.
   *
   * @param Object packet
   *        The allocations packet being created. See the @returns for
   *        |getAllocations|
   *
   * @param Map framesToIndices
   *        A map from a SavedFrame instance to an index into the packet's
   *        "frames" list.
   *
   * @returns Number
   *          The index into the "frames" list where the given frame was
   *          inserted.
   */
  _insertStackInAllocationsPacket: function(frame, packet, framesToIndices) {
    if (framesToIndices.has(frame)) {
      return framesToIndices.get(frame);
    }

    let frameForm = {
      line: frame.line,
      column: frame.column,
      source: frame.source,
      functionDisplayName: frame.functionDisplayName
    };

    if (frame.parent) {
      frameForm.parent = this._insertStackInAllocationsPacket(frame.parent,
                                                              packet,
                                                              framesToIndices);
    }

    let idx = packet.frames.length;
    packet.frames.push(frameForm);
    return idx;
  },

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

exports.register = function(handle) {
  handle.addGlobalActor(MemoryActor, "memoryActor");
  handle.addTabActor(MemoryActor, "memoryActor");
};

exports.unregister = function(handle) {
  handle.removeGlobalActor(MemoryActor, "memoryActor");
  handle.removeTabActor(MemoryActor, "memoryActor");
};
