/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const { DebuggerServer } = require("devtools/server/main");
const { DevToolsUtils } = Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {});
const Debugger = require("Debugger");

// TODO bug 943125: remove this polyfill and use Debugger.Frame.prototype.depth
// once it is implemented.
if (!Object.getOwnPropertyDescriptor(Debugger.Frame.prototype, "depth")) {
  Debugger.Frame.prototype._depth = null;
  Object.defineProperty(Debugger.Frame.prototype, "depth", {
    get: function () {
      if (this._depth === null) {
        if (!this.older) {
          this._depth = 0;
        } else {
          // Hide depth from self-hosted frames.
          const increment = this.script && this.script.url == "self-hosted"
            ? 0
            : 1;
          this._depth = increment + this.older.depth;
        }
      }

      return this._depth;
    }
  });
}

const { setTimeout } = require("sdk/timers");

/**
 * The number of milliseconds we should buffer frame enter/exit packets before
 * sending.
 */
const BUFFER_SEND_DELAY = 50;

/**
 * The maximum number of arguments we will send for any single function call.
 */
const MAX_ARGUMENTS = 3;

/**
 * The maximum number of an object's properties we will serialize.
 */
const MAX_PROPERTIES = 3;

/**
 * The complete set of trace types supported.
 */
const TRACE_TYPES = new Set([
  "time",
  "return",
  "throw",
  "yield",
  "name",
  "location",
  "callsite",
  "parameterNames",
  "arguments",
  "depth"
]);

/**
 * Creates a TracerActor. TracerActor provides a stream of function
 * call/return packets to a remote client gathering a full trace.
 */
function TracerActor(aConn, aParent)
{
  this._dbg = null;
  this._parent = aParent;
  this._attached = false;
  this._activeTraces = new MapStack();
  this._totalTraces = 0;
  this._startTime = 0;
  this._sequence = 0;
  this._bufferSendTimer = null;
  this._buffer = [];

  // Keep track of how many different trace requests have requested what kind of
  // tracing info. This way we can minimize the amount of data we are collecting
  // at any given time.
  this._requestsForTraceType = Object.create(null);
  for (let type of TRACE_TYPES) {
    this._requestsForTraceType[type] = 0;
  }

  this.onEnterFrame = this.onEnterFrame.bind(this);
  this.onExitFrame = this.onExitFrame.bind(this);
}

TracerActor.prototype = {
  actorPrefix: "trace",

  get attached() { return this._attached; },
  get idle()     { return this._attached && this._activeTraces.size === 0; },
  get tracing()  { return this._attached && this._activeTraces.size > 0; },

  get dbg() {
    if (!this._dbg) {
      this._dbg = this._parent.makeDebugger();
      this._dbg.onEnterFrame = this.onEnterFrame;
    }
    return this._dbg;
  },

  /**
   * Buffer traces and only send them every BUFFER_SEND_DELAY milliseconds.
   */
  _send: function(aPacket) {
    this._buffer.push(aPacket);
    if (this._bufferSendTimer === null) {
      this._bufferSendTimer = setTimeout(() => {
        this.conn.send({
          from: this.actorID,
          type: "traces",
          traces: this._buffer.splice(0, this._buffer.length)
        });
        this._bufferSendTimer = null;
      }, BUFFER_SEND_DELAY);
    }
  },

  /**
   * Handle a protocol request to attach to the trace actor.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onAttach: function(aRequest) {
    if (this.attached) {
      return {
        error: "wrongState",
        message: "Already attached to a client"
      };
    }

    this.dbg.addDebuggees();
    this._attached = true;

    return {
      type: "attached",
      traceTypes: Object.keys(this._requestsForTraceType)
        .filter(k => !!this._requestsForTraceType[k])
    };
  },

  /**
   * Handle a protocol request to detach from the trace actor.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onDetach: function() {
    while (this.tracing) {
      this.onStopTrace();
    }

    this._dbg = null;
    this._attached = false;

    return {
      type: "detached"
    };
  },

  /**
   * Handle a protocol request to start a new trace.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onStartTrace: function(aRequest) {
    for (let traceType of aRequest.trace) {
      if (!TRACE_TYPES.has(traceType)) {
        return {
          error: "badParameterType",
          message: "No such trace type: " + traceType
        };
      }
    }

    if (this.idle) {
      this.dbg.enabled = true;
      this._sequence = 0;
      this._startTime = Date.now();
    }

    // Start recording all requested trace types.
    for (let traceType of aRequest.trace) {
      this._requestsForTraceType[traceType]++;
    }

    this._totalTraces++;
    let name = aRequest.name || "Trace " + this._totalTraces;
    this._activeTraces.push(name, aRequest.trace);

    return { type: "startedTrace", why: "requested", name: name };
  },

  /**
   * Handle a protocol request to end a trace.
   *
   * @param aRequest object
   *        The protocol request object.
   */
  onStopTrace: function(aRequest) {
    if (!this.tracing) {
      return {
        error: "wrongState",
        message: "No active traces"
      };
    }

    let stoppedTraceTypes, name;
    if (aRequest && aRequest.name) {
      name = aRequest.name;
      if (!this._activeTraces.has(name)) {
        return {
          error: "noSuchTrace",
          message: "No active trace with name: " + name
        };
      }
      stoppedTraceTypes = this._activeTraces.delete(name);
    } else {
      name = this._activeTraces.peekKey();
      stoppedTraceTypes = this._activeTraces.pop();
    }

    for (let traceType of stoppedTraceTypes) {
      this._requestsForTraceType[traceType]--;
    }

    if (this.idle) {
      this.dbg.enabled = false;
    }

    return {
      type: "stoppedTrace",
      why: "requested",
      name
    };
  },

  // JS Debugger API hooks.

  /**
   * Called by the engine when a frame is entered. Sends an unsolicited packet
   * to the client carrying requested trace information.
   *
   * @param aFrame Debugger.Frame
   *        The stack frame that was entered.
   */
  onEnterFrame: function(aFrame) {
    if (aFrame.script && aFrame.script.url == "self-hosted") {
      return;
    }

    let packet = {
      type: "enteredFrame",
      sequence: this._sequence++
    };

    if (this._requestsForTraceType.name) {
      packet.name = aFrame.callee
        ? aFrame.callee.displayName || "(anonymous function)"
        : "(" + aFrame.type + ")";
    }

    if (this._requestsForTraceType.location && aFrame.script) {
      // We should return the location of the start of the script, but
      // Debugger.Script does not provide complete start locations (bug
      // 901138). Instead, return the current offset (the location of the first
      // statement in the function).
      packet.location = {
        url: aFrame.script.url,
        line: aFrame.script.getOffsetLine(aFrame.offset),
        column: getOffsetColumn(aFrame.offset, aFrame.script)
      };
    }

    if (this._requestsForTraceType.callsite
        && aFrame.older
        && aFrame.older.script) {
      let older = aFrame.older;
      packet.callsite = {
        url: older.script.url,
        line: older.script.getOffsetLine(older.offset),
        column: getOffsetColumn(older.offset, older.script)
      };
    }

    if (this._requestsForTraceType.time) {
      packet.time = Date.now() - this._startTime;
    }

    if (this._requestsForTraceType.parameterNames && aFrame.callee) {
      packet.parameterNames = aFrame.callee.parameterNames;
    }

    if (this._requestsForTraceType.arguments && aFrame.arguments) {
      packet.arguments = [];
      let i = 0;
      for (let arg of aFrame.arguments) {
        if (i++ > MAX_ARGUMENTS) {
          break;
        }
        packet.arguments.push(createValueSnapshot(arg, true));
      }
    }

    if (this._requestsForTraceType.depth) {
      packet.depth = aFrame.depth;
    }

    const onExitFrame = this.onExitFrame;
    aFrame.onPop = function (aCompletion) {
      onExitFrame(this, aCompletion);
    };

    this._send(packet);
  },

  /**
   * Called by the engine when a frame is exited. Sends an unsolicited packet to
   * the client carrying requested trace information.
   *
   * @param Debugger.Frame aFrame
   *        The Debugger.Frame that was just exited.
   * @param aCompletion object
   *        The debugger completion value for the frame.
   */
  onExitFrame: function(aFrame, aCompletion) {
    let packet = {
      type: "exitedFrame",
      sequence: this._sequence++,
    };

    if (!aCompletion) {
      packet.why = "terminated";
    } else if (aCompletion.hasOwnProperty("return")) {
      packet.why = "return";
    } else if (aCompletion.hasOwnProperty("yield")) {
      packet.why = "yield";
    } else {
      packet.why = "throw";
    }

    if (this._requestsForTraceType.time) {
      packet.time = Date.now() - this._startTime;
    }

    if (this._requestsForTraceType.depth) {
      packet.depth = aFrame.depth;
    }

    if (aCompletion) {
      if (this._requestsForTraceType.return && "return" in aCompletion) {
        packet.return = createValueSnapshot(aCompletion.return, true);
      }

      else if (this._requestsForTraceType.throw && "throw" in aCompletion) {
        packet.throw = createValueSnapshot(aCompletion.throw, true);
      }

      else if (this._requestsForTraceType.yield && "yield" in aCompletion) {
        packet.yield = createValueSnapshot(aCompletion.yield, true);
      }
    }

    this._send(packet);
  }
};

/**
 * The request types this actor can handle.
 */
TracerActor.prototype.requestTypes = {
  "attach": TracerActor.prototype.onAttach,
  "detach": TracerActor.prototype.onDetach,
  "startTrace": TracerActor.prototype.onStartTrace,
  "stopTrace": TracerActor.prototype.onStopTrace
};

exports.register = function(handle) {
  handle.addTabActor(TracerActor, "traceActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(TracerActor, "traceActor");
};


/**
 * MapStack is a collection of key/value pairs with stack ordering,
 * where keys are strings and values are any JS value. In addition to
 * the push and pop stack operations, supports a "delete" operation,
 * which removes the value associated with a given key from any
 * location in the stack.
 */
function MapStack()
{
  // Essentially a MapStack is just sugar-coating around a standard JS
  // object, plus the _stack array to track ordering.
  this._stack = [];
  this._map = Object.create(null);
}

MapStack.prototype = {
  get size() { return this._stack.length; },

  /**
   * Return the key for the value on the top of the stack, or
   * undefined if the stack is empty.
   */
  peekKey: function() {
    return this._stack[this.size - 1];
  },

  /**
   * Return true iff a value has been associated with the given key.
   *
   * @param aKey string
   *        The key whose presence is to be tested.
   */
  has: function(aKey) {
    return Object.prototype.hasOwnProperty.call(this._map, aKey);
  },

  /**
   * Return the value associated with the given key, or undefined if
   * no value is associated with the key.
   *
   * @param aKey string
   *        The key whose associated value is to be returned.
   */
  get: function(aKey) {
    return this._map[aKey] || undefined;
  },

  /**
   * Push a new value onto the stack. If another value with the same
   * key is already on the stack, it will be removed before the new
   * value is pushed onto the top of the stack.
   *
   * @param aKey string
   *        The key of the object to push onto the stack.
   *
   * @param aValue
   *        The value to push onto the stack.
   */
  push: function(aKey, aValue) {
    this.delete(aKey);
    this._stack.push(aKey);
    this._map[aKey] = aValue;
  },

  /**
   * Remove the value from the top of the stack and return it.
   * Returns undefined if the stack is empty.
   */
  pop: function() {
    let key = this.peekKey();
    let value = this.get(key);
    this._stack.pop();
    delete this._map[key];
    return value;
  },

  /**
   * Remove the value associated with the given key from the stack and
   * return it. Returns undefined if no value is associated with the
   * given key.
   *
   * @param aKey string
   *        The key for the value to remove from the stack.
   */
  delete: function(aKey) {
    let value = this.get(aKey);
    if (this.has(aKey)) {
      let keyIndex = this._stack.lastIndexOf(aKey);
      this._stack.splice(keyIndex, 1);
      delete this._map[aKey];
    }
    return value;
  }
};

// TODO bug 863089: use Debugger.Script.prototype.getOffsetColumn when
// it is implemented.
function getOffsetColumn(aOffset, aScript) {
  return 0;
}

// Serialization helper functions. Largely copied from script.js and modified
// for use in serialization rather than object actor requests.

/**
 * Create a grip for the given debuggee value.
 *
 * @param aValue Debugger.Object|primitive
 *        The value to describe with the created grip.
 *
 * @param aDetailed boolean
 *        If true, capture slightly more detailed information, like some
 *        properties on an object.
 *
 * @return Object
 *         A primitive value or a snapshot of an object.
 */
function createValueSnapshot(aValue, aDetailed=false) {
  switch (typeof aValue) {
    case "boolean":
      return aValue;
    case "string":
      if (aValue.length >= DebuggerServer.LONG_STRING_LENGTH) {
        return {
          type: "longString",
          initial: aValue.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH),
          length: aValue.length
        };
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
      return aDetailed
        ? detailedObjectSnapshot(aValue)
        : objectSnapshot(aValue);
    default:
      DevToolsUtils.reportException("TracerActor",
                      new Error("Failed to provide a grip for: " + aValue));
      return null;
  }
}

/**
 * Create a very minimal snapshot of the given debuggee object.
 *
 * @param aObject Debugger.Object
 *        The object to describe with the created grip.
 */
function objectSnapshot(aObject) {
  return {
    "type": "object",
    "class": aObject.class,
  };
}

/**
 * Create a (slightly more) detailed snapshot of the given debuggee object.
 *
 * @param aObject Debugger.Object
 *        The object to describe with the created descriptor.
 */
function detailedObjectSnapshot(aObject) {
  let desc = objectSnapshot(aObject);
  let ownProperties = desc.ownProperties = Object.create(null);

  if (aObject.class == "DeadObject") {
    return desc;
  }

  let i = 0;
  for (let name of aObject.getOwnPropertyNames()) {
    if (i++ > MAX_PROPERTIES) {
      break;
    }
    let desc = propertySnapshot(name, aObject);
    if (desc) {
      ownProperties[name] = desc;
    }
  }

  return desc;
}

/**
 * A helper method that creates a snapshot of the object's |aName| property.
 *
 * @param aName string
 *        The property of which the snapshot is taken.
 *
 * @param aObject Debugger.Object
 *        The object whose property the snapshot is taken of.
 *
 * @return Object
 *         The snapshot of the property.
 */
function propertySnapshot(aName, aObject) {
  let desc;
  try {
    desc = aObject.getOwnPropertyDescriptor(aName);
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

  // Only create descriptors for simple values. We skip objects and properties
  // that have getters and setters; ain't nobody got time for that!
  if (!desc
      || typeof desc.value == "object" && desc.value !== null
      || !("value" in desc)) {
    return undefined;
  }

  return {
    configurable: desc.configurable,
    enumerable: desc.enumerable,
    writable: desc.writable,
    value: createValueSnapshot(desc.value)
  };
}
