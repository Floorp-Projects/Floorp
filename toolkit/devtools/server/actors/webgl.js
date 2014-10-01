/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const events = require("sdk/event/core");
const promise = require("promise");
const protocol = require("devtools/server/protocol");
const { ContentObserver } = require("devtools/content-observer");
const { on, once, off, emit } = events;
const { method, Arg, Option, RetVal } = protocol;

const WEBGL_CONTEXT_NAMES = ["webgl", "experimental-webgl", "moz-webgl"];

// These traits are bit masks. Make sure they're powers of 2.
const PROGRAM_DEFAULT_TRAITS = 0;
const PROGRAM_BLACKBOX_TRAIT = 1;
const PROGRAM_HIGHLIGHT_TRAIT = 2;

/**
 * A WebGL Shader contributing to building a WebGL Program.
 * You can either retrieve, or compile the source of a shader, which will
 * automatically inflict the necessary changes to the WebGL state.
 */
let ShaderActor = protocol.ActorClass({
  typeName: "gl-shader",

  /**
   * Create the shader actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param WebGLProgram program
   *        The WebGL program being linked.
   * @param WebGLShader shader
   *        The cooresponding vertex or fragment shader.
   * @param WebGLProxy proxy
   *        The proxy methods for the WebGL context owning this shader.
   */
  initialize: function(conn, program, shader, proxy) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.program = program;
    this.shader = shader;
    this.text = proxy.getShaderSource(shader);
    this.linkedProxy = proxy;
  },

  /**
   * Gets the source code for this shader.
   */
  getText: method(function() {
    return this.text;
  }, {
    response: { text: RetVal("string") }
  }),

  /**
   * Sets and compiles new source code for this shader.
   */
  compile: method(function(text) {
    // Get the shader and corresponding program to change via the WebGL proxy.
    let { linkedProxy: proxy, shader, program } = this;

    // Get the new shader source to inject.
    let oldText = this.text;
    let newText = text;

    // Overwrite the shader's source.
    let error = proxy.compileShader(program, shader, this.text = newText);

    // If something went wrong, revert to the previous shader.
    if (error.compile || error.link) {
      proxy.compileShader(program, shader, this.text = oldText);
      return error;
    }
    return undefined;
  }, {
    request: { text: Arg(0, "string") },
    response: { error: RetVal("nullable:json") }
  })
});

/**
 * The corresponding Front object for the ShaderActor.
 */
let ShaderFront = protocol.FrontClass(ShaderActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

/**
 * A WebGL program is composed (at the moment, analogue to OpenGL ES 2.0)
 * of two shaders: a vertex shader and a fragment shader.
 */
let ProgramActor = protocol.ActorClass({
  typeName: "gl-program",

  /**
   * Create the program actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param WebGLProgram program
   *        The WebGL program being linked.
   * @param WebGLShader[] shaders
   *        The WebGL program's cooresponding vertex and fragment shaders.
   * @param WebGLCache cache
   *        The state storage for the WebGL context owning this program.
   * @param WebGLProxy proxy
   *        The proxy methods for the WebGL context owning this program.
   */
  initialize: function(conn, [program, shaders, cache, proxy]) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._shaderActorsCache = { vertex: null, fragment: null };
    this.program = program;
    this.shaders = shaders;
    this.linkedCache = cache;
    this.linkedProxy = proxy;
  },

  get ownerWindow() this.linkedCache.ownerWindow,
  get ownerContext() this.linkedCache.ownerContext,

  /**
   * Gets the vertex shader linked to this program. This method guarantees
   * a single actor instance per shader.
   */
  getVertexShader: method(function() {
    return this._getShaderActor("vertex");
  }, {
    response: { shader: RetVal("gl-shader") }
  }),

  /**
   * Gets the fragment shader linked to this program. This method guarantees
   * a single actor instance per shader.
   */
  getFragmentShader: method(function() {
    return this._getShaderActor("fragment");
  }, {
    response: { shader: RetVal("gl-shader") }
  }),

  /**
   * Highlights any geometry rendered using this program.
   */
  highlight: method(function(tint) {
    this.linkedProxy.highlightTint = tint;
    this.linkedCache.setProgramTrait(this.program, PROGRAM_HIGHLIGHT_TRAIT);
  }, {
    request: { tint: Arg(0, "array:number") },
    oneway: true
  }),

  /**
   * Allows geometry to be rendered normally using this program.
   */
  unhighlight: method(function() {
    this.linkedCache.unsetProgramTrait(this.program, PROGRAM_HIGHLIGHT_TRAIT);
  }, {
    oneway: true
  }),

  /**
   * Prevents any geometry from being rendered using this program.
   */
  blackbox: method(function() {
    this.linkedCache.setProgramTrait(this.program, PROGRAM_BLACKBOX_TRAIT);
  }, {
    oneway: true
  }),

  /**
   * Allows geometry to be rendered using this program.
   */
  unblackbox: method(function() {
    this.linkedCache.unsetProgramTrait(this.program, PROGRAM_BLACKBOX_TRAIT);
  }, {
    oneway: true
  }),

  /**
   * Returns a cached ShaderActor instance based on the required shader type.
   *
   * @param string type
   *        Either "vertex" or "fragment".
   * @return ShaderActor
   *         The respective shader actor instance.
   */
  _getShaderActor: function(type) {
    if (this._shaderActorsCache[type]) {
      return this._shaderActorsCache[type];
    }
    let proxy = this.linkedProxy;
    let shader = proxy.getShaderOfType(this.shaders, type);
    let shaderActor = new ShaderActor(this.conn, this.program, shader, proxy);
    return this._shaderActorsCache[type] = shaderActor;
  }
});

/**
 * The corresponding Front object for the ProgramActor.
 */
let ProgramFront = protocol.FrontClass(ProgramActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  }
});

/**
 * The WebGL Actor handles simple interaction with a WebGL context via a few
 * high-level methods. After instantiating this actor, you'll need to set it
 * up by calling setup().
 */
let WebGLActor = exports.WebGLActor = protocol.ActorClass({
  typeName: "webgl",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
    this._onProgramLinked = this._onProgramLinked.bind(this);
  },
  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Starts waiting for the current tab actor's document global to be
   * created, in order to instrument the Canvas context and become
   * aware of everything the content does WebGL-wise.
   *
   * See ContentObserver and WebGLInstrumenter for more details.
   */
  setup: method(function({ reload }) {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    this._programActorsCache = [];
    this._webglObserver = new WebGLObserver();

    on(this.tabActor, "window-ready", this._onGlobalCreated);
    on(this.tabActor, "window-destroyed", this._onGlobalDestroyed);
    on(this._webglObserver, "program-linked", this._onProgramLinked);

    if (reload) {
      this.tabActor.window.location.reload();
    }
  }, {
    request: { reload: Option(0, "boolean") },
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

    off(this.tabActor, "window-ready", this._onGlobalCreated);
    off(this.tabActor, "window-destroyed", this._onGlobalDestroyed);
    off(this._webglObserver, "program-linked", this._onProgramLinked);

    this._programActorsCache = null;
    this._contentObserver = null;
    this._webglObserver = null;
  }, {
   oneway: true
  }),

  /**
   * Gets an array of cached program actors for the current tab actor's window.
   * This is useful for dealing with bfcache, when no new programs are linked.
   */
  getPrograms: method(function() {
    let id = ContentObserver.GetInnerWindowID(this.tabActor.window);
    return this._programActorsCache.filter(e => e.ownerWindow == id);
  }, {
    response: { programs: RetVal("array:gl-program") }
  }),

  /**
   * Waits for one frame via `requestAnimationFrame` on the tab actor's window.
   * Used in tests.
   */
  waitForFrame: method(function () {
    let deferred = promise.defer();
    this.tabActor.window.requestAnimationFrame(deferred.resolve);
    return deferred.promise;
  }, {
    response: { success: RetVal("nullable:json") }
  }),

  /**
   * Gets a pixel's RGBA value from a context specified by selector
   * and the coordinates of the pixel in question.
   * Currently only used in tests.
   *
   * @param string selector
   *        A string selector to select the canvas in question from the DOM.
   * @param Object position
   *        An object with an `x` and `y` property indicating coordinates of the pixel being inspected.
   * @return Object
   *        An object containing `r`, `g`, `b`, and `a` properties of the pixel.
   */
  getPixel: method(function ({ selector, position }) {
    let { x, y } = position;
    let canvas = this.tabActor.window.document.querySelector(selector);
    let context = XPCNativeWrapper.unwrap(canvas.getContext("webgl"));
    let { proxy } = this._webglObserver.for(context);
    let height = canvas.height;

    let buffer = new this.tabActor.window.Uint8Array(4);
    buffer = XPCNativeWrapper.unwrap(buffer);

    proxy.readPixels(x, height - y - 1, 1, 1, context.RGBA, context.UNSIGNED_BYTE, buffer);

    return { r: buffer[0], g: buffer[1], b: buffer[2], a: buffer[3] };
  }, {
    request: {
      selector: Option(0, "string"),
      position: Option(0, "json")
    },
    response: { pixels: RetVal("json") }
  }),

  /**
   * Events emitted by this actor. The "program-linked" event is fired
   * every time a WebGL program was linked with its respective two shaders.
   */
  events: {
    "program-linked": {
      type: "programLinked",
      program: Arg(0, "gl-program")
    },
    "global-destroyed": {
      type: "globalDestroyed",
      program: Arg(0, "number")
    },
    "global-created": {
      type: "globalCreated",
      program: Arg(0, "number")
    }
  },

  /**
   * Gets an array of all cached program actors belonging to all windows.
   * This should only be used for tests.
   */
  _getAllPrograms: method(function() {
    return this._programActorsCache;
  }, {
    response: { programs: RetVal("array:gl-program") }
  }),


  /**
   * Invoked whenever the current tab actor's document global is created.
   */
  _onGlobalCreated: function({id, window, isTopLevel}) {
    if (isTopLevel) {
      WebGLInstrumenter.handle(window, this._webglObserver);
      events.emit(this, "global-created", id);
    }
  },

  /**
   * Invoked whenever the current tab actor's inner window is destroyed.
   */
  _onGlobalDestroyed: function({id, isTopLevel, isFrozen}) {
    if (isTopLevel && !isFrozen) {
      removeFromArray(this._programActorsCache, e => e.ownerWindow == id);
      this._webglObserver.unregisterContextsForWindow(id);
      events.emit(this, "global-destroyed", id);
    }
  },

  /**
   * Invoked whenever an observed WebGL context links a program.
   */
  _onProgramLinked: function(...args) {
    let programActor = new ProgramActor(this.conn, args);
    this._programActorsCache.push(programActor);
    events.emit(this, "program-linked", programActor);
  }
});

/**
 * The corresponding Front object for the WebGLActor.
 */
let WebGLFront = exports.WebGLFront = protocol.FrontClass(WebGLActor, {
  initialize: function(client, { webglActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: webglActor });
    this.manage(this);
  }
});

/**
 * Instruments a HTMLCanvasElement with the appropriate inspection methods.
 */
let WebGLInstrumenter = {
  /**
   * Overrides the getContext method in the HTMLCanvasElement prototype.
   *
   * @param nsIDOMWindow window
   *        The window to perform the instrumentation in.
   * @param WebGLObserver observer
   *        The observer watching function calls in the context.
   */
  handle: function(window, observer) {
    let self = this;

    let id = ContentObserver.GetInnerWindowID(window);
    let canvasElem = XPCNativeWrapper.unwrap(window.HTMLCanvasElement);
    let canvasPrototype = canvasElem.prototype;
    let originalGetContext = canvasPrototype.getContext;

    /**
     * Returns a drawing context on the canvas, or null if the context ID is
     * not supported. This override creates an observer for the targeted context
     * type and instruments specific functions in the targeted context instance.
     */
    canvasPrototype.getContext = function(name, options) {
      // Make sure a context was able to be created.
      let context = originalGetContext.call(this, name, options);
      if (!context) {
        return context;
      }
      // Make sure a WebGL (not a 2D) context will be instrumented.
      if (WEBGL_CONTEXT_NAMES.indexOf(name) == -1) {
        return context;
      }
      // Repeated calls to 'getContext' return the same instance, no need to
      // instrument everything again.
      if (observer.for(context)) {
        return context;
      }

      // Create a separate state storage for this context.
      observer.registerContextForWindow(id, context);

      // Link our observer to the new WebGL context methods.
      for (let { timing, callback, functions } of self._methods) {
        for (let func of functions) {
          self._instrument(observer, context, func, callback, timing);
        }
      }

      // Return the decorated context back to the content consumer, which
      // will continue using it normally.
      return context;
    };
  },

  /**
   * Overrides a specific method in a HTMLCanvasElement context.
   *
   * @param WebGLObserver observer
   *        The observer watching function calls in the context.
   * @param WebGLRenderingContext context
   *        The targeted WebGL context instance.
   * @param string funcName
   *        The function to override.
   * @param array callbackName [optional]
   *        The two callback function names in the observer, corresponding to
   *        the "before" and "after" invocation times. If unspecified, they will
   *        default to the name of the function to override.
   * @param number timing [optional]
   *        When to issue the callback in relation to the actual context
   *        function call. Availalble values are -1 for "before" (default)
   *        1 for "after" and 0 for "before and after".
   */
  _instrument: function(observer, context, funcName, callbackName = [], timing = -1) {
    let { cache, proxy } = observer.for(context);
    let originalFunc = context[funcName];
    let beforeFuncName = callbackName[0] || funcName;
    let afterFuncName = callbackName[1] || callbackName[0] || funcName;

    context[funcName] = function(...glArgs) {
      if (timing <= 0 && !observer.suppressHandlers) {
        let glBreak = observer[beforeFuncName](glArgs, cache, proxy);
        if (glBreak) return undefined;
      }

      // Invoking .apply on an unxrayed content function doesn't work, because
      // the arguments array is inaccessible to it. Get Xrays back.
      let glResult = Cu.waiveXrays(Cu.unwaiveXrays(originalFunc).apply(this, glArgs));

      if (timing >= 0 && !observer.suppressHandlers) {
        let glBreak = observer[afterFuncName](glArgs, glResult, cache, proxy);
        if (glBreak) return undefined;
      }

      return glResult;
    };
  },

  /**
   * Override mappings for WebGL methods.
   */
  _methods: [{
    timing: 1, // after
    functions: [
      "linkProgram", "getAttribLocation", "getUniformLocation"
    ]
  }, {
    timing: -1, // before
    callback: [
      "toggleVertexAttribArray"
    ],
    functions: [
      "enableVertexAttribArray", "disableVertexAttribArray"
    ]
  }, {
    timing: -1, // before
    callback: [
      "attribute_"
    ],
    functions: [
      "vertexAttrib1f", "vertexAttrib2f", "vertexAttrib3f", "vertexAttrib4f",
      "vertexAttrib1fv", "vertexAttrib2fv", "vertexAttrib3fv", "vertexAttrib4fv",
      "vertexAttribPointer"
    ]
  }, {
    timing: -1, // before
    callback: [
      "uniform_"
    ],
    functions: [
      "uniform1i", "uniform2i", "uniform3i", "uniform4i",
      "uniform1f", "uniform2f", "uniform3f", "uniform4f",
      "uniform1iv", "uniform2iv", "uniform3iv", "uniform4iv",
      "uniform1fv", "uniform2fv", "uniform3fv", "uniform4fv",
      "uniformMatrix2fv", "uniformMatrix3fv", "uniformMatrix4fv"
    ]
  }, {
    timing: -1, // before
    functions: [
      "useProgram", "enable", "disable", "blendColor",
      "blendEquation", "blendEquationSeparate",
      "blendFunc", "blendFuncSeparate"
    ]
  }, {
    timing: 0, // before and after
    callback: [
      "beforeDraw_", "afterDraw_"
    ],
    functions: [
      "drawArrays", "drawElements"
    ]
  }]
  // TODO: It'd be a good idea to handle other functions as well:
  //   - getActiveUniform
  //   - getUniform
  //   - getActiveAttrib
  //   - getVertexAttrib
};

/**
 * An observer that captures a WebGL context's method calls.
 */
function WebGLObserver() {
  this._contexts = new Map();
}

WebGLObserver.prototype = {
  _contexts: null,

  /**
   * Creates a WebGLCache and a WebGLProxy for the specified window and context.
   *
   * @param number id
   *        The id of the window containing the WebGL context.
   * @param WebGLRenderingContext context
   *        The WebGL context used in the cache and proxy instances.
   */
  registerContextForWindow: function(id, context) {
    let cache = new WebGLCache(id, context);
    let proxy = new WebGLProxy(id, context, cache, this);
    cache.refreshState(proxy);

    this._contexts.set(context, {
      ownerWindow: id,
      cache: cache,
      proxy: proxy
    });
  },

  /**
   * Removes all WebGLCache and WebGLProxy instances for a particular window.
   *
   * @param number id
   *        The id of the window containing the WebGL context.
   */
  unregisterContextsForWindow: function(id) {
    removeFromMap(this._contexts, e => e.ownerWindow == id);
  },

  /**
   * Gets the WebGLCache and WebGLProxy instances for a particular context.
   *
   * @param WebGLRenderingContext context
   *        The WebGL context used in the cache and proxy instances.
   * @return object
   *         An object containing the corresponding { cache, proxy } instances.
   */
  for: function(context) {
    return this._contexts.get(context);
  },

  /**
   * Set this flag to true to stop observing any context function calls.
   */
  suppressHandlers: false,

  /**
   * Called immediately *after* 'linkProgram' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param void glResult
   *        The returned value of the original function call.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   * @param WebGLProxy proxy
   *        The proxy methods for the WebGL context initiating this call.
   */
  linkProgram: function(glArgs, glResult, cache, proxy) {
    let program = glArgs[0];
    let shaders = proxy.getAttachedShaders(program);
    cache.addProgram(program, PROGRAM_DEFAULT_TRAITS);
    emit(this, "program-linked", program, shaders, cache, proxy);
  },

  /**
   * Called immediately *after* 'getAttribLocation' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param GLint glResult
   *        The returned value of the original function call.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  getAttribLocation: function(glArgs, glResult, cache) {
    // Make sure the attribute's value is legal before caching.
    if (glResult < 0) {
      return;
    }
    let [program, name] = glArgs;
    cache.addAttribute(program, name, glResult);
  },

  /**
   * Called immediately *after* 'getUniformLocation' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLUniformLocation glResult
   *        The returned value of the original function call.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  getUniformLocation: function(glArgs, glResult, cache) {
    // Make sure the uniform's value is legal before caching.
    if (!glResult) {
      return;
    }
    let [program, name] = glArgs;
    cache.addUniform(program, name, glResult);
  },

  /**
   * Called immediately *before* 'enableVertexAttribArray' or
   * 'disableVertexAttribArray'is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  toggleVertexAttribArray: function(glArgs, cache) {
    glArgs[0] = cache.getCurrentAttributeLocation(glArgs[0]);
    return glArgs[0] < 0; // Return true to break original function call.
  },

  /**
   * Called immediately *before* 'attribute_' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  attribute_: function(glArgs, cache) {
    glArgs[0] = cache.getCurrentAttributeLocation(glArgs[0]);
    return glArgs[0] < 0; // Return true to break original function call.
  },

  /**
   * Called immediately *before* 'uniform_' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  uniform_: function(glArgs, cache) {
    glArgs[0] = cache.getCurrentUniformLocation(glArgs[0]);
    return !glArgs[0]; // Return true to break original function call.
  },

  /**
   * Called immediately *before* 'useProgram' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  useProgram: function(glArgs, cache) {
    // Manually keeping a cache and not using gl.getParameter(CURRENT_PROGRAM)
    // because gl.get* functions are slow as potatoes.
    cache.currentProgram = glArgs[0];
  },

  /**
   * Called immediately *before* 'enable' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  enable: function(glArgs, cache) {
    cache.currentState[glArgs[0]] = true;
  },

  /**
   * Called immediately *before* 'disable' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  disable: function(glArgs, cache) {
    cache.currentState[glArgs[0]] = false;
  },

  /**
   * Called immediately *before* 'blendColor' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  blendColor: function(glArgs, cache) {
    let blendColor = cache.currentState.blendColor;
    blendColor[0] = glArgs[0];
    blendColor[1] = glArgs[1];
    blendColor[2] = glArgs[2];
    blendColor[3] = glArgs[3];
  },

  /**
   * Called immediately *before* 'blendEquation' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  blendEquation: function(glArgs, cache) {
    let state = cache.currentState;
    state.blendEquationRgb = state.blendEquationAlpha = glArgs[0];
  },

  /**
   * Called immediately *before* 'blendEquationSeparate' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  blendEquationSeparate: function(glArgs, cache) {
    let state = cache.currentState;
    state.blendEquationRgb = glArgs[0];
    state.blendEquationAlpha = glArgs[1];
  },

  /**
   * Called immediately *before* 'blendFunc' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  blendFunc: function(glArgs, cache) {
    let state = cache.currentState;
    state.blendSrcRgb = state.blendSrcAlpha = glArgs[0];
    state.blendDstRgb = state.blendDstAlpha = glArgs[1];
  },

  /**
   * Called immediately *before* 'blendFuncSeparate' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  blendFuncSeparate: function(glArgs, cache) {
    let state = cache.currentState;
    state.blendSrcRgb = glArgs[0];
    state.blendDstRgb = glArgs[1];
    state.blendSrcAlpha = glArgs[2];
    state.blendDstAlpha = glArgs[3];
  },

  /**
   * Called immediately *before* 'drawArrays' or 'drawElements' is requested
   * in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   * @param WebGLProxy proxy
   *        The proxy methods for the WebGL context initiating this call.
   */
  beforeDraw_: function(glArgs, cache, proxy) {
    let traits = cache.currentProgramTraits;

    // Handle program blackboxing.
    if (traits & PROGRAM_BLACKBOX_TRAIT) {
      return true; // Return true to break original function call.
    }
    // Handle program highlighting.
    if (traits & PROGRAM_HIGHLIGHT_TRAIT) {
      proxy.enableHighlighting();
    }

    return false;
  },

  /**
   * Called immediately *after* 'drawArrays' or 'drawElements' is requested
   * in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param void glResult
   *        The returned value of the original function call.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   * @param WebGLProxy proxy
   *        The proxy methods for the WebGL context initiating this call.
   */
  afterDraw_: function(glArgs, glResult, cache, proxy) {
    let traits = cache.currentProgramTraits;

    // Handle program highlighting.
    if (traits & PROGRAM_HIGHLIGHT_TRAIT) {
      proxy.disableHighlighting();
    }
  }
};

/**
 * A mechanism for storing a single WebGL context's state, programs, shaders,
 * attributes or uniforms.
 *
 * @param number id
 *        The id of the window containing the WebGL context.
 * @param WebGLRenderingContext context
 *        The WebGL context for which the state is stored.
 */
function WebGLCache(id, context) {
  this._id = id;
  this._gl = context;
  this._programs = new Map();
  this.currentState = {};
}

WebGLCache.prototype = {
  _id: 0,
  _gl: null,
  _programs: null,
  _currentProgramInfo: null,
  _currentAttributesMap: null,
  _currentUniformsMap: null,

  get ownerWindow() this._id,
  get ownerContext() this._gl,

  /**
   * A collection of flags or properties representing the context's state.
   * Implemented as an object hash and not a Map instance because keys are
   * always either strings or numbers.
   */
  currentState: null,

  /**
   * Populates the current state with values retrieved from the context.
   *
   * @param WebGLProxy proxy
   *        The proxy methods for the WebGL context owning the state.
   */
  refreshState: function(proxy) {
    let gl = this._gl;
    let s = this.currentState;

    // Populate only with the necessary parameters. Not all default WebGL
    // state values are required.
    s[gl.BLEND] = proxy.isEnabled("BLEND");
    s.blendColor = proxy.getParameter("BLEND_COLOR");
    s.blendEquationRgb = proxy.getParameter("BLEND_EQUATION_RGB");
    s.blendEquationAlpha = proxy.getParameter("BLEND_EQUATION_ALPHA");
    s.blendSrcRgb = proxy.getParameter("BLEND_SRC_RGB");
    s.blendSrcAlpha = proxy.getParameter("BLEND_SRC_ALPHA");
    s.blendDstRgb = proxy.getParameter("BLEND_DST_RGB");
    s.blendDstAlpha = proxy.getParameter("BLEND_DST_ALPHA");
  },

  /**
   * Adds a program to the cache.
   *
   * @param WebGLProgram program
   *        The shader for which the traits are to be cached.
   * @param number traits
   *        A default properties mask set for the program.
   */
  addProgram: function(program, traits) {
    this._programs.set(program, {
      traits: traits,
      attributes: [], // keys are GLints (numbers)
      uniforms: new Map() // keys are WebGLUniformLocations (objects)
    });
  },

  /**
   * Adds a specific trait to a program. The effect of such properties is
   * determined by the consumer of this cache.
   *
   * @param WebGLProgram program
   *        The program to add the trait to.
   * @param number trait
   *        The property added to the program.
   */
  setProgramTrait: function(program, trait) {
    this._programs.get(program).traits |= trait;
  },

  /**
   * Removes a specific trait from a program.
   *
   * @param WebGLProgram program
   *        The program to remove the trait from.
   * @param number trait
   *        The property removed from the program.
   */
  unsetProgramTrait: function(program, trait) {
    this._programs.get(program).traits &= ~trait;
  },

  /**
   * Sets the currently used program in the context.
   * @param WebGLProgram program
   */
  set currentProgram(program) {
    let programInfo = this._programs.get(program);
    if (programInfo == null) {
      return;
    }
    this._currentProgramInfo = programInfo;
    this._currentAttributesMap = programInfo.attributes;
    this._currentUniformsMap = programInfo.uniforms;
  },

  /**
   * Gets the traits for the currently used program.
   * @return number
   */
  get currentProgramTraits() {
    return this._currentProgramInfo.traits;
  },

  /**
   * Adds an attribute to the cache.
   *
   * @param WebGLProgram program
   *        The program for which the attribute is bound.
   * @param string name
   *        The attribute name.
   * @param GLint value
   *        The attribute value.
   */
  addAttribute: function(program, name, value) {
    this._programs.get(program).attributes[value] = {
      name: name,
      value: value
    };
  },

  /**
   * Adds a uniform to the cache.
   *
   * @param WebGLProgram program
   *        The program for which the uniform is bound.
   * @param string name
   *        The uniform name.
   * @param WebGLUniformLocation value
   *        The uniform value.
   */
  addUniform: function(program, name, value) {
    this._programs.get(program).uniforms.set(new XPCNativeWrapper(value), {
      name: name,
      value: value
    });
  },

  /**
   * Updates the attribute locations for a specific program.
   * This is necessary, for example, when the shader is relinked and all the
   * attribute locations become obsolete.
   *
   * @param WebGLProgram program
   *        The program for which the attributes need updating.
   */
  updateAttributesForProgram: function(program) {
    let attributes = this._programs.get(program).attributes;
    for (let attribute of attributes) {
      attribute.value = this._gl.getAttribLocation(program, attribute.name);
    }
  },

  /**
   * Updates the uniform locations for a specific program.
   * This is necessary, for example, when the shader is relinked and all the
   * uniform locations become obsolete.
   *
   * @param WebGLProgram program
   *        The program for which the uniforms need updating.
   */
  updateUniformsForProgram: function(program) {
    let uniforms = this._programs.get(program).uniforms;
    for (let [, uniform] of uniforms) {
      uniform.value = this._gl.getUniformLocation(program, uniform.name);
    }
  },

  /**
   * Gets the actual attribute location in a specific program.
   * When relinked, all the attribute locations become obsolete and are updated
   * in the cache. This method returns the (current) real attribute location.
   *
   * @param GLint initialValue
   *        The initial attribute value.
   * @return GLint
   *         The current attribute value, or the initial value if it's already
   *         up to date with its corresponding program.
   */
  getCurrentAttributeLocation: function(initialValue) {
    let attributes = this._currentAttributesMap;
    let currentInfo = attributes ? attributes[initialValue] : null;
    return currentInfo ? currentInfo.value : initialValue;
  },

  /**
   * Gets the actual uniform location in a specific program.
   * When relinked, all the uniform locations become obsolete and are updated
   * in the cache. This method returns the (current) real uniform location.
   *
   * @param WebGLUniformLocation initialValue
   *        The initial uniform value.
   * @return WebGLUniformLocation
   *         The current uniform value, or the initial value if it's already
   *         up to date with its corresponding program.
   */
  getCurrentUniformLocation: function(initialValue) {
    let uniforms = this._currentUniformsMap;
    let currentInfo = uniforms ? uniforms.get(initialValue) : null;
    return currentInfo ? currentInfo.value : initialValue;
  }
};

/**
 * A mechanism for injecting or qureying state into/from a single WebGL context.
 *
 * Any interaction with a WebGL context should go through this proxy.
 * Otherwise, the corresponding observer would register the calls as coming
 * from content, which is usually not desirable. Infinite call stacks are bad.
 *
 * @param number id
 *        The id of the window containing the WebGL context.
 * @param WebGLRenderingContext context
 *        The WebGL context used for the proxy methods.
 * @param WebGLCache cache
 *        The state storage for the corresponding context.
 * @param WebGLObserver observer
 *        The observer watching function calls in the corresponding context.
 */
function WebGLProxy(id, context, cache, observer) {
  this._id = id;
  this._gl = context;
  this._cache = cache;
  this._observer = observer;

  let exports = [
    "isEnabled",
    "getParameter",
    "getAttachedShaders",
    "getShaderSource",
    "getShaderOfType",
    "compileShader",
    "enableHighlighting",
    "disableHighlighting",
    "readPixels"
  ];
  exports.forEach(e => this[e] = (...args) => this._call(e, args));
}

WebGLProxy.prototype = {
  _id: 0,
  _gl: null,
  _cache: null,
  _observer: null,

  get ownerWindow() this._id,
  get ownerContext() this._gl,

  /**
   * Test whether a WebGL capability is enabled.
   *
   * @param string name
   *        The WebGL capability name, for example "BLEND".
   * @return boolean
   *         True if enabled, false otherwise.
   */
  _isEnabled: function(name) {
    return this._gl.isEnabled(this._gl[name]);
  },

  /**
   * Returns the value for the specified WebGL parameter name.
   *
   * @param string name
   *        The WebGL parameter name, for example "BLEND_COLOR".
   * @return any
   *         The corresponding parameter's value.
   */
  _getParameter: function(name) {
    return this._gl.getParameter(this._gl[name]);
  },

  /**
   * Returns the renderbuffer property value for the specified WebGL parameter.
   * If no renderbuffer binding is available, null is returned.
   *
   * @param string name
   *        The WebGL parameter name, for example "BLEND_COLOR".
   * @return any
   *         The corresponding parameter's value.
   */
  _getRenderbufferParameter: function(name) {
    if (!this._getParameter("RENDERBUFFER_BINDING")) {
      return null;
    }
    let gl = this._gl;
    return gl.getRenderbufferParameter(gl.RENDERBUFFER, gl[name]);
  },

  /**
   * Returns the framebuffer property value for the specified WebGL parameter.
   * If no framebuffer binding is available, null is returned.
   *
   * @param string type
   *        The framebuffer object attachment point, for example "COLOR_ATTACHMENT0".
   * @param string name
   *        The WebGL parameter name, for example "FRAMEBUFFER_ATTACHMENT_OBJECT_NAME".
   *        If unspecified, defaults to "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE".
   * @return any
   *         The corresponding parameter's value.
   */
  _getFramebufferAttachmentParameter: function(type, name = "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE") {
    if (!this._getParameter("FRAMEBUFFER_BINDING")) {
      return null;
    }
    let gl = this._gl;
    return gl.getFramebufferAttachmentParameter(gl.FRAMEBUFFER, gl[type], gl[name]);
  },

  /**
   * Returns the shader objects attached to a program object.
   *
   * @param WebGLProgram program
   *        The program for which to retrieve the attached shaders.
   * @return array
   *         The attached vertex and fragment shaders.
   */
  _getAttachedShaders: function(program) {
    return this._gl.getAttachedShaders(program);
  },

  /**
   * Returns the source code string from a shader object.
   *
   * @param WebGLShader shader
   *        The shader for which to retrieve the source code.
   * @return string
   *         The shader's source code.
   */
  _getShaderSource: function(shader) {
    return this._gl.getShaderSource(shader);
  },

  /**
   * Finds a shader of the specified type in a list.
   *
   * @param WebGLShader[] shaders
   *        The shaders for which to check the type.
   * @param string type
   *        Either "vertex" or "fragment".
   * @return WebGLShader | null
   *         The shader of the specified type, or null if nothing is found.
   */
  _getShaderOfType: function(shaders, type) {
    let gl = this._gl;
    let shaderTypeEnum = {
      vertex: gl.VERTEX_SHADER,
      fragment: gl.FRAGMENT_SHADER
    }[type];

    for (let shader of shaders) {
      if (gl.getShaderParameter(shader, gl.SHADER_TYPE) == shaderTypeEnum) {
        return shader;
      }
    }
    return null;
  },

  /**
   * Changes a shader's source code and relinks the respective program.
   *
   * @param WebGLProgram program
   *        The program who's linked shader is to be modified.
   * @param WebGLShader shader
   *        The shader to be modified.
   * @param string text
   *        The new shader source code.
   * @return object
   *         An object containing the compilation and linking status.
   */
  _compileShader: function(program, shader, text) {
    let gl = this._gl;
    gl.shaderSource(shader, text);
    gl.compileShader(shader);
    gl.linkProgram(program);

    let error = { compile: "", link: "" };

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      error.compile = gl.getShaderInfoLog(shader);
    }
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
      error.link = gl.getShaderInfoLog(shader);
    }

    this._cache.updateAttributesForProgram(program);
    this._cache.updateUniformsForProgram(program);

    return error;
  },

  /**
   * Enables color blending based on the geometry highlight tint.
   */
  _enableHighlighting: function() {
    let gl = this._gl;

    // Avoid changing the blending params when "rendering to texture".

    // Check drawing to a custom framebuffer bound to the default renderbuffer.
    let hasFramebuffer = this._getParameter("FRAMEBUFFER_BINDING");
    let hasRenderbuffer = this._getParameter("RENDERBUFFER_BINDING");
    if (hasFramebuffer && !hasRenderbuffer) {
      return;
    }

    // Check drawing to a depth or stencil component of the framebuffer.
    let writesDepth = this._getFramebufferAttachmentParameter("DEPTH_ATTACHMENT");
    let writesStencil = this._getFramebufferAttachmentParameter("STENCIL_ATTACHMENT");
    if (writesDepth || writesStencil) {
      return;
    }

    // Non-premultiplied alpha blending based on a predefined constant color.
    // Simply using gl.colorMask won't work, because we want non-tinted colors
    // to be drawn as black, not ignored.
    gl.enable(gl.BLEND);
    gl.blendColor.apply(gl, this.highlightTint);
    gl.blendEquation(gl.FUNC_ADD);
    gl.blendFunc(gl.CONSTANT_COLOR, gl.ONE_MINUS_SRC_ALPHA, gl.CONSTANT_COLOR, gl.ZERO);
    this.wasHighlighting = true;
  },

  /**
   * Disables color blending based on the geometry highlight tint, by
   * reverting the corresponding params back to their original values.
   */
  _disableHighlighting: function() {
    let gl = this._gl;
    let s = this._cache.currentState;

    gl[s[gl.BLEND] ? "enable" : "disable"](gl.BLEND);
    gl.blendColor.apply(gl, s.blendColor);
    gl.blendEquationSeparate(s.blendEquationRgb, s.blendEquationAlpha);
    gl.blendFuncSeparate(s.blendSrcRgb, s.blendDstRgb, s.blendSrcAlpha, s.blendDstAlpha);
  },

  /**
   * Returns the pixel values at the position specified on the canvas.
   */
  _readPixels: function(x, y, w, h, format, type, buffer) {
    this._gl.readPixels(x, y, w, h, format, type, buffer);
  },

  /**
   * The color tint used for highlighting geometry.
   * @see _enableHighlighting and _disableHighlighting.
   */
  highlightTint: [0, 0, 0, 0],

  /**
   * Executes a function in this object.
   *
   * This method makes sure that any handlers in the context observer are
   * suppressed, hence stopping observing any context function calls.
   *
   * @param string funcName
   *        The function to call.
   * @param array args
   *        An array of arguments.
   * @return any
   *         The called function result.
   */
  _call: function(funcName, args) {
    let prevState = this._observer.suppressHandlers;

    this._observer.suppressHandlers = true;
    let result = this["_" + funcName].apply(this, args);
    this._observer.suppressHandlers = prevState;

    return result;
  }
};

// Utility functions.

function removeFromMap(map, predicate) {
  for (let [key, value] of map) {
    if (predicate(value)) {
      map.delete(key);
    }
  }
};

function removeFromArray(array, predicate) {
  for (let i = 0; i < array.length;) {
    if (predicate(array[i])) {
      array.splice(i, 1);
    } else {
      i++;
    }
  }
}
