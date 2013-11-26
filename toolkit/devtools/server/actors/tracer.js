/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");

const { reportException } =
  Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm", {}).DevToolsUtils;

const { DebuggerServer } = Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

Cu.import("resource://gre/modules/jsdebugger.jsm");
addDebuggerToGlobal(this);

const { setTimeout } = require("sdk/timers");

/**
 * The number of milliseconds we should buffer frame enter/exit packets before
 * sending.
 */
const BUFFER_SEND_DELAY = 50;

/**
 * The maximum number of arguments we will send for any single function call.
 */
const MAX_ARGUMENTS = 5;

/**
 * The maximum number of an object's properties we will serialize.
 */
const MAX_PROPERTIES = 5;

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
  "arguments"
]);

/**
 * Creates a TraceActor. TraceActor provides a stream of function
 * call/return packets to a remote client gathering a full trace.
 */
function TraceActor(aConn, aParentActor)
{
  this._attached = false;
  this._activeTraces = new MapStack();
  this._totalTraces = 0;
  this._startTime = 0;

  // Keep track of how many different trace requests have requested what kind of
  // tracing info. This way we can minimize the amount of data we are collecting
  // at any given time.
  this._requestsForTraceType = Object.create(null);
  for (let type of TRACE_TYPES) {
    this._requestsForTraceType[type] = 0;
  }

  this._sequence = 0;
  this._bufferSendTimer = null;
  this._buffer = [];

  this.global = aParentActor.window.wrappedJSObject;
}

TraceActor.prototype = {
  actorPrefix: "trace",

  get attached() { return this._attached; },
  get idle()     { return this._attached && this._activeTraces.size === 0; },
  get tracing()  { return this._attached && this._activeTraces.size > 0; },

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
   * Initializes a Debugger instance and adds listeners to it.
   */
  _initDebugger: function() {
    this.dbg = new Debugger();
    this.dbg.onEnterFrame = this.onEnterFrame.bind(this);
    this.dbg.onNewGlobalObject = this.globalManager.onNewGlobal.bind(this);
    this.dbg.enabled = false;
  },

  /**
   * Add a debuggee global to the Debugger object.
   */
  _addDebuggee: function(aGlobal) {
    try {
      this.dbg.addDebuggee(aGlobal);
    } catch (e) {
      // Ignore attempts to add the debugger's compartment as a debuggee.
      reportException("TraceActor",
                      new Error("Ignoring request to add the debugger's "
                                + "compartment as a debuggee"));
    }
  },

  /**
   * Add the provided window and all windows in its frame tree as debuggees.
   */
  _addDebuggees: function(aWindow) {
    this._addDebuggee(aWindow);
    let frames = aWindow.frames;
    if (frames) {
      for (let i = 0; i < frames.length; i++) {
        this._addDebuggees(frames[i]);
      }
    }
  },

  /**
   * An object used by TraceActors to tailor their behavior depending
   * on the debugging context required (chrome or content).
   */
  globalManager: {
    /**
     * Adds all globals in the global object as debuggees.
     */
    findGlobals: function() {
      this._addDebuggees(this.global);
    },

    /**
     * A function that the engine calls when a new global object has been
     * created. Adds the global object as a debuggee if it is in the content
     * window.
     *
     * @param aGlobal Debugger.Object
     *        The new global object that was created.
     */
    onNewGlobal: function(aGlobal) {
      // Content debugging only cares about new globals in the content
      // window, like iframe children.
      if (aGlobal.hostAnnotations &&
          aGlobal.hostAnnotations.type == "document" &&
          aGlobal.hostAnnotations.element === this.global) {
        this._addDebuggee(aGlobal);
      }
    },
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

    if (!this.dbg) {
      this._initDebugger();
      this.globalManager.findGlobals.call(this);
    }

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

    this.dbg = null;

    this._attached = false;
    return { type: "detached" };
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

    return { type: "stoppedTrace", why: "requested", name: name };
  },

  // JS Debugger API hooks.

  /**
   * Called by the engine when a frame is entered. Sends an unsolicited packet
   * to the client carrying requested trace information.
   *
   * @param aFrame Debugger.frame
   *        The stack frame that was entered.
   */
  onEnterFrame: function(aFrame) {
    let callee = aFrame.callee;
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
        packet.arguments.push(createValueGrip(arg, true));
      }
    }

    aFrame.onPop = this.onExitFrame.bind(this);

    this._send(packet);
  },

  /**
   * Called by the engine when a frame is exited. Sends an unsolicited packet to
   * the client carrying requested trace information.
   *
   * @param aCompletion object
   *        The debugger completion value for the frame.
   */
  onExitFrame: function(aCompletion) {
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

    if (aCompletion) {
      if (this._requestsForTraceType.return) {
        packet.return = createValueGrip(aCompletion.return, true);
      }

      if (this._requestsForTraceType.throw) {
        packet.throw = createValueGrip(aCompletion.throw, true);
      }

      if (this._requestsForTraceType.yield) {
        packet.yield = createValueGrip(aCompletion.yield, true);
      }
    }

    this._send(packet);
  }
};

/**
 * The request types this actor can handle.
 */
TraceActor.prototype.requestTypes = {
  "attach": TraceActor.prototype.onAttach,
  "detach": TraceActor.prototype.onDetach,
  "startTrace": TraceActor.prototype.onStartTrace,
  "stopTrace": TraceActor.prototype.onStopTrace
};

exports.register = function(handle) {
  handle.addTabActor(TraceActor, "traceActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(TraceActor, "traceActor");
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
    return this._map[aKey];
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
  let bestOffsetMapping = null;
  for (let offsetMapping of aScript.getAllColumnOffsets()) {
    if (!bestOffsetMapping ||
        (offsetMapping.offset <= aOffset &&
         offsetMapping.offset > bestOffsetMapping.offset)) {
      bestOffsetMapping = offsetMapping;
    }
  }

  if (!bestOffsetMapping) {
    // XXX: Try not to completely break the experience of using the
    // tracer for the user by assuming column 0. Simultaneously,
    // report the error so that there is a paper trail if the
    // assumption is bad and the tracing experience becomes wonky.
    reportException("TraceActor",
                    new Error("Could not find a column for offset " + aOffset +
                              " in the script " + aScript));
    return 0;
  }

  return bestOffsetMapping.columnNumber;
}


// Serialization helper functions. Largely copied from script.js and modified
// for use in serialization rather than object actor requests.

/**
 * Create a grip for the given debuggee value.
 *
 * @param aValue Debugger.Object|primitive
 *        The value to describe with the created grip.
 *
 * @param aUseDescriptor boolean
 *        If true, creates descriptors for objects rather than grips.
 *
 * @return ValueGrip
 *        A primitive value or a grip object.
 */
function createValueGrip(aValue, aUseDescriptor) {
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
      return aUseDescriptor ? objectDescriptor(aValue) : objectGrip(aValue);
    default:
      reportException("TraceActor",
                      new Error("Failed to provide a grip for: " + aValue));
      return null;
  }
}

/**
 * Create a grip for the given debuggee object.
 *
 * @param aObject Debugger.Object
 *        The object to describe with the created grip.
 */
function objectGrip(aObject) {
  let g = {
    "type": "object",
    "class": aObject.class,
    "extensible": aObject.isExtensible(),
    "frozen": aObject.isFrozen(),
    "sealed": aObject.isSealed()
  };

  // Add additional properties for functions.
  if (aObject.class === "Function") {
    if (aObject.name) {
      g.name = aObject.name;
    }
    if (aObject.displayName) {
      g.displayName = aObject.displayName;
    }

    // Check if the developer has added a de-facto standard displayName
    // property for us to use.
    let name = aObject.getOwnPropertyDescriptor("displayName");
    if (name && name.value && typeof name.value == "string") {
      g.userDisplayName = createValueGrip(name.value, aObject);
    }

    // Add source location information.
    if (aObject.script) {
      g.url = aObject.script.url;
      g.line = aObject.script.startLine;
    }
  }

  return g;
}

/**
 * Create a descriptor for the given debuggee object. Descriptors are
 * identical to grips, with the addition of the prototype,
 * ownProperties, and safeGetterValues properties.
 *
 * @param aObject Debugger.Object
 *        The object to describe with the created descriptor.
 */
function objectDescriptor(aObject) {
  let desc = objectGrip(aObject);
  let ownProperties = Object.create(null);

  if (Cu.isDeadWrapper(aObject)) {
    desc.prototype = createValueGrip(null);
    desc.ownProperties = ownProperties;
    desc.safeGetterValues = Object.create(null);
    return desc;
  }

  const names = aObject.getOwnPropertyNames();
  let i = 0;
  for (let name of names) {
    if (i++ > MAX_PROPERTIES) {
      break;
    }
    let desc = propertyDescriptor(name, aObject);
    if (desc) {
      ownProperties[name] = desc;
    }
  }

  desc.ownProperties = ownProperties;

  return desc;
}

/**
 * A helper method that creates a property descriptor for the provided object,
 * properly formatted for sending in a protocol response.
 *
 * @param aName string
 *        The property that the descriptor is generated for.
 *
 * @param aObject Debugger.Object
 *        The object whose property the descriptor is generated for.
 *
 * @return object
 *         The property descriptor for the property |aName| in |aObject|.
 */
function propertyDescriptor(aName, aObject) {
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

  // Skip objects since we only support shallow objects anyways.
  if (!desc || typeof desc.value == "object" && desc.value !== null) {
    return undefined;
  }

  let retval = {
    configurable: desc.configurable,
    enumerable: desc.enumerable
  };

  if ("value" in desc) {
    retval.writable = desc.writable;
    retval.value = createValueGrip(desc.value);
  } else {
    if ("get" in desc) {
      retval.get = createValueGrip(desc.get);
    }
    if ("set" in desc) {
      retval.set = createValueGrip(desc.set);
    }
  }
  return retval;
}
