/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const events = require("sdk/event/core");
const promise = require("sdk/core/promise");
const protocol = require("devtools/server/protocol");
const {ContentObserver} = require("devtools/content-observer");

const {on, once, off, emit} = events;
const {method, Arg, Option, RetVal} = protocol;

exports.register = function(handle) {
  handle.addTabActor(CallWatcherActor, "callWatcherActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(CallWatcherActor);
};

/**
 * Type describing a single function call in a stack trace.
 */
protocol.types.addDictType("call-stack-item", {
  name: "string",
  file: "string",
  line: "number"
});

/**
 * Type describing an overview of a function call.
 */
protocol.types.addDictType("call-details", {
  type: "number",
  name: "string",
  stack: "array:call-stack-item"
});

/**
 * This actor contains information about a function call, like the function
 * type, name, stack, arguments, returned value etc.
 */
let FunctionCallActor = protocol.ActorClass({
  typeName: "function-call",

  /**
   * Creates the function call actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param DOMWindow window
   *        The content window.
   * @param string global
   *        The name of the global object owning this function, like
   *        "CanvasRenderingContext2D" or "WebGLRenderingContext".
   * @param object caller
   *        The object owning the function when it was called.
   *        For example, in `foo.bar()`, the caller is `foo`.
   * @param number type
   *        Either METHOD_FUNCTION, METHOD_GETTER or METHOD_SETTER.
   * @param string name
   *        The called function's name.
   * @param array stack
   *        The called function's stack, as a list of { name, file, line } objects.
   * @param array args
   *        The called function's arguments.
   * @param any result
   *        The value returned by the function call.
   */
  initialize: function(conn, [window, global, caller, type, name, stack, args, result]) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.details = {
      window: window,
      caller: caller,
      type: type,
      name: name,
      stack: stack,
      args: args,
      result: result
    };

    this.meta = {
      global: -1,
      previews: { caller: "", args: "" }
    };

    if (global == "WebGLRenderingContext") {
      this.meta.global = CallWatcherFront.CANVAS_WEBGL_CONTEXT;
    } else if (global == "CanvasRenderingContext2D") {
      this.meta.global = CallWatcherFront.CANVAS_2D_CONTEXT;
    } else if (global == "window") {
      this.meta.global = CallWatcherFront.UNKNOWN_SCOPE;
    } else {
      this.meta.global = CallWatcherFront.GLOBAL_SCOPE;
    }

    this.meta.previews.caller = this._generateCallerPreview();
    this.meta.previews.args = this._generateArgsPreview();
  },

  /**
   * Customize the marshalling of this actor to provide some generic information
   * directly on the Front instance.
   */
  form: function() {
    return {
      actor: this.actorID,
      type: this.details.type,
      name: this.details.name,
      file: this.details.stack[0].file,
      line: this.details.stack[0].line,
      callerPreview: this.meta.previews.caller,
      argsPreview: this.meta.previews.args
    };
  },

  /**
   * Gets more information about this function call, which is not necessarily
   * available on the Front instance.
   */
  getDetails: method(function() {
    let { type, name, stack } = this.details;

    // Since not all calls on the stack have corresponding owner files (e.g.
    // callbacks of a requestAnimationFrame etc.), there's no benefit in
    // returning them, as the user can't jump to the Debugger from them.
    for (let i = stack.length - 1;;) {
      if (stack[i].file) {
        break;
      }
      stack.pop();
      i--;
    }

    // XXX: Use grips for objects and serialize them properly, in order
    // to add the function's caller, arguments and return value. Bug 978957.
    return {
      type: type,
      name: name,
      stack: stack
    };
  }, {
    response: { info: RetVal("call-details") }
  }),

  /**
   * Serializes the caller's name so that it can be easily be transferred
   * as a string, but still be useful when displayed in a potential UI.
   *
   * @return string
   *         The caller's name as a string.
   */
  _generateCallerPreview: function() {
    let global = this.meta.global;
    if (global == CallWatcherFront.CANVAS_WEBGL_CONTEXT) {
      return "gl";
    }
    if (global == CallWatcherFront.CANVAS_2D_CONTEXT) {
      return "ctx";
    }
    return "";
  },

  /**
   * Serializes the arguments so that they can be easily be transferred
   * as a string, but still be useful when displayed in a potential UI.
   *
   * @return string
   *         The arguments as a string.
   */
  _generateArgsPreview: function() {
    let { caller, args } = this.details;
    let { global } = this.meta;

    // XXX: All of this sucks. Make this smarter, so that the frontend
    // can inspect each argument, be it object or primitive. Bug 978960.
    let serializeArgs = () => args.map(arg => {
      if (typeof arg == "undefined") {
        return "undefined";
      }
      if (typeof arg == "function") {
        return "Function";
      }
      if (typeof arg == "object") {
        return "Object";
      }
      if (global == CallWatcherFront.CANVAS_WEBGL_CONTEXT) {
        // XXX: This doesn't handle combined bitmasks. Bug 978964.
        return getEnumsLookupTable("webgl", caller)[arg] || arg;
      }
      if (global == CallWatcherFront.CANVAS_2D_CONTEXT) {
        return getEnumsLookupTable("2d", caller)[arg] || arg;
      }
      return arg;
    });

    return serializeArgs().join(", ");
  }
});

/**
 * The corresponding Front object for the FunctionCallActor.
 */
let FunctionCallFront = protocol.FrontClass(FunctionCallActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  },

  /**
   * Adds some generic information directly to this instance,
   * to avoid extra roundtrips.
   */
  form: function(form) {
    this.actorID = form.actor;
    this.type = form.type;
    this.name = form.name;
    this.file = form.file;
    this.line = form.line;
    this.callerPreview = form.callerPreview;
    this.argsPreview = form.argsPreview;
  }
});

/**
 * This actor observes function calls on certain objects or globals.
 */
let CallWatcherActor = exports.CallWatcherActor = protocol.ActorClass({
  typeName: "call-watcher",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
    this._onContentFunctionCall = this._onContentFunctionCall.bind(this);
  },
  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Starts waiting for the current tab actor's document global to be
   * created, in order to instrument the specified objects and become
   * aware of everything the content does with them.
   */
  setup: method(function({ tracedGlobals, tracedFunctions, startRecording, performReload }) {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    this._functionCalls = [];
    this._tracedGlobals = tracedGlobals || [];
    this._tracedFunctions = tracedFunctions || [];
    this._contentObserver = new ContentObserver(this.tabActor);

    on(this._contentObserver, "global-created", this._onGlobalCreated);
    on(this._contentObserver, "global-destroyed", this._onGlobalDestroyed);

    if (startRecording) {
      this.resumeRecording();
    }
    if (performReload) {
      this.tabActor.window.location.reload();
    }
  }, {
    request: {
      tracedGlobals: Option(0, "nullable:array:string"),
      tracedFunctions: Option(0, "nullable:array:string"),
      startRecording: Option(0, "boolean"),
      performReload: Option(0, "boolean")
    },
    oneway: true
  }),

  /**
   * Stops listening for document global changes and puts this actor
   * to hibernation. This method is called automatically just before the
   * actor is destroyed.
   */
  finalize: method(function() {
    if (!this._initialized) {
      return;
    }
    this._initialized = false;

    this._contentObserver.stopListening();
    off(this._contentObserver, "global-created", this._onGlobalCreated);
    off(this._contentObserver, "global-destroyed", this._onGlobalDestroyed);

    this._tracedGlobals = null;
    this._tracedFunctions = null;
    this._contentObserver = null;
  }, {
    oneway: true
  }),

  /**
   * Returns whether the instrumented function calls are currently recorded.
   */
  isRecording: method(function() {
    return this._recording;
  }, {
    response: RetVal("boolean")
  }),

  /**
   * Starts recording function calls.
   */
  resumeRecording: method(function() {
    this._recording = true;
  }),

  /**
   * Stops recording function calls.
   */
  pauseRecording: method(function() {
    this._recording = false;
    return this._functionCalls;
  }, {
    response: { calls: RetVal("array:function-call") }
  }),

  /**
   * Erases all the recorded function calls.
   * Calling `resumeRecording` or `pauseRecording` does not erase history.
   */
  eraseRecording: method(function() {
    this._functionCalls = [];
  }),

  /**
   * Lightweight listener invoked whenever an instrumented function is called
   * while recording. We're doing this to avoid the event emitter overhead,
   * since this is expected to be a very hot function.
   */
  onCall: function() {},

  /**
   * Invoked whenever the current tab actor's document global is created.
   */
  _onGlobalCreated: function(window) {
    let self = this;

    this._tracedWindowId = ContentObserver.GetInnerWindowID(window);
    let unwrappedWindow = XPCNativeWrapper.unwrap(window);
    let callback = this._onContentFunctionCall;

    for (let global of this._tracedGlobals) {
      let prototype = unwrappedWindow[global].prototype;
      let properties = Object.keys(prototype);
      properties.forEach(name => overrideSymbol(global, prototype, name, callback));
    }

    for (let name of this._tracedFunctions) {
      overrideSymbol("window", unwrappedWindow, name, callback);
    }

    /**
     * Instruments a method, getter or setter on the specified target object to
     * invoke a callback whenever it is called.
     */
    function overrideSymbol(global, target, name, callback) {
      let propertyDescriptor = Object.getOwnPropertyDescriptor(target, name);

      if (propertyDescriptor.get || propertyDescriptor.set) {
        overrideAccessor(global, target, name, propertyDescriptor, callback);
        return;
      }
      if (propertyDescriptor.writable && typeof propertyDescriptor.value == "function") {
        overrideFunction(global, target, name, propertyDescriptor, callback);
        return;
      }
    }

    /**
     * Instruments a function on the specified target object.
     */
    function overrideFunction(global, target, name, descriptor, callback) {
      let originalFunc = target[name];

      Object.defineProperty(target, name, {
        value: function(...args) {
          let result = originalFunc.apply(this, args);

          if (self._recording) {
            let stack = getStack(name);
            let type = CallWatcherFront.METHOD_FUNCTION;
            callback(unwrappedWindow, global, this, type, name, stack, args, result);
          }
          return result;
        },
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable,
        writable: true
      });
    }

    /**
     * Instruments a getter or setter on the specified target object.
     */
    function overrideAccessor(global, target, name, descriptor, callback) {
      let originalGetter = target.__lookupGetter__(name);
      let originalSetter = target.__lookupSetter__(name);

      Object.defineProperty(target, name, {
        get: function(...args) {
          if (!originalGetter) return undefined;
          let result = originalGetter.apply(this, args);

          if (self._recording) {
            let stack = getStack(name);
            let type = CallWatcherFront.GETTER_FUNCTION;
            callback(unwrappedWindow, global, this, type, name, stack, args, result);
          }
          return result;
        },
        set: function(...args) {
          if (!originalSetter) return;
          originalSetter.apply(this, args);

          if (self._recording) {
            let stack = getStack(name);
            let type = CallWatcherFront.SETTER_FUNCTION;
            callback(unwrappedWindow, global, this, type, name, stack, args, undefined);
          }
        },
        configurable: descriptor.configurable,
        enumerable: descriptor.enumerable
      });
    }

    /**
     * Stores the relevant information about calls on the stack when
     * a function is called.
     */
    function getStack(caller) {
      try {
        // Using Components.stack wouldn't be a better idea, since it's
        // much slower because it attempts to retrieve the C++ stack as well.
        throw new Error();
      } catch (e) {
        var stack = e.stack;
      }

      // Of course, using a simple regex like /(.*?)@(.*):(\d*):\d*/ would be
      // much prettier, but this is a very hot function, so let's sqeeze
      // every drop of performance out of it.
      let calls = [];
      let callIndex = 0;
      let currNewLinePivot = stack.indexOf("\n") + 1;
      let nextNewLinePivot = stack.indexOf("\n", currNewLinePivot);

      while (nextNewLinePivot > 0) {
        let nameDelimiterIndex = stack.indexOf("@", currNewLinePivot);
        let columnDelimiterIndex = stack.lastIndexOf(":", nextNewLinePivot - 1);
        let lineDelimiterIndex = stack.lastIndexOf(":", columnDelimiterIndex - 1);

        if (!calls[callIndex]) {
          calls[callIndex] = { name: "", file: "", line: 0 };
        }
        if (!calls[callIndex + 1]) {
          calls[callIndex + 1] = { name: "", file: "", line: 0 };
        }

        if (callIndex > 0) {
          let file = stack.substring(nameDelimiterIndex + 1, lineDelimiterIndex);
          let line = stack.substring(lineDelimiterIndex + 1, columnDelimiterIndex);
          let name = stack.substring(currNewLinePivot, nameDelimiterIndex);
          calls[callIndex].name = name;
          calls[callIndex - 1].file = file;
          calls[callIndex - 1].line = line;
        } else {
          // Since the topmost stack frame is actually our overwritten function,
          // it will not have the expected name.
          calls[0].name = caller;
        }

        currNewLinePivot = nextNewLinePivot + 1;
        nextNewLinePivot = stack.indexOf("\n", currNewLinePivot);
        callIndex++;
      }

      return calls;
    }
  },

  /**
   * Invoked whenever the current tab actor's inner window is destroyed.
   */
  _onGlobalDestroyed: function(id) {
    if (this._tracedWindowId == id) {
      this.pauseRecording();
      this.eraseRecording();
    }
  },

  /**
   * Invoked whenever an instrumented function is called.
   */
  _onContentFunctionCall: function(...details) {
    let functionCall = new FunctionCallActor(this.conn, details);
    this._functionCalls.push(functionCall);
    this.onCall(functionCall);
  }
});

/**
 * The corresponding Front object for the CallWatcherActor.
 */
let CallWatcherFront = exports.CallWatcherFront = protocol.FrontClass(CallWatcherActor, {
  initialize: function(client, { callWatcherActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: callWatcherActor });
    client.addActorPool(this);
    this.manage(this);
  }
});

/**
 * Constants.
 */
CallWatcherFront.METHOD_FUNCTION = 0;
CallWatcherFront.GETTER_FUNCTION = 1;
CallWatcherFront.SETTER_FUNCTION = 2;

CallWatcherFront.GLOBAL_SCOPE = 0;
CallWatcherFront.UNKNOWN_SCOPE = 1;
CallWatcherFront.CANVAS_WEBGL_CONTEXT = 2;
CallWatcherFront.CANVAS_2D_CONTEXT = 3;

/**
 * A lookup table for cross-referencing flags or properties with their name
 * assuming they look LIKE_THIS most of the time.
 *
 * For example, when gl.clear(gl.COLOR_BUFFER_BIT) is called, the actual passed
 * argument's value is 16384, which we want identified as "COLOR_BUFFER_BIT".
 */
var gEnumRegex = /^[A-Z_]+$/;
var gEnumsLookupTable = {};

function getEnumsLookupTable(type, object) {
  let cachedEnum = gEnumsLookupTable[type];
  if (cachedEnum) {
    return cachedEnum;
  }

  let table = gEnumsLookupTable[type] = {};

  for (let key in object) {
    if (key.match(gEnumRegex)) {
      table[object[key]] = key;
    }
  }

  return table;
}
