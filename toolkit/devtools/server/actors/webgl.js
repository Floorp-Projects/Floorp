/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

Cu.import("resource://gre/modules/Services.jsm");

const events = require("sdk/event/core");
const protocol = require("devtools/server/protocol");

const { on, once, off, emit } = events;
const { method, Arg, Option, RetVal } = protocol;

const WEBGL_CONTEXT_NAMES = ["webgl", "experimental-webgl", "moz-webgl"];

const HIGHLIGHT_FRAG_SHADER = [
  "precision lowp float;",
  "void main() {",
    "gl_FragColor.rgba = vec4(%color);",
  "}"
].join("\n");

// These traits are bit masks. Make sure they're powers of 2.
const PROGRAM_DEFAULT_TRAITS = 0;
const PROGRAM_BLACKBOX_TRAIT = 1;

exports.register = function(handle) {
  handle.addTabActor(WebGLActor, "webglActor");
}

exports.unregister = function(handle) {
  handle.removeTabActor(WebGLActor);
}

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
   * Replaces this program's fragment shader with an temporary
   * easy-to-distinguish alternative. See HIGHLIGHT_FRAG_SHADER.
   */
  highlight: method(function(color) {
    let shaderActor = this._getShaderActor("fragment");
    let oldText = shaderActor.text;
    let newText = HIGHLIGHT_FRAG_SHADER.replace("%color", color)
    shaderActor.compile(newText);
    shaderActor.text = oldText;
  }, {
    request: { color: Arg(0, "array:string") },
    oneway: true
  }),

  /**
   * Reverts this program's fragment shader to the latest user-defined source.
   */
  unhighlight: method(function() {
    let shaderActor = this._getShaderActor("fragment");
    shaderActor.compile(shaderActor.text);
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
    this._contentObserver = new ContentObserver(this.tabActor);
    this._webglObserver = new WebGLObserver();

    on(this._contentObserver, "global-created", this._onGlobalCreated);
    on(this._contentObserver, "global-destroyed", this._onGlobalDestroyed);
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

    this._contentObserver.stopListening();
    off(this._contentObserver, "global-created", this._onGlobalCreated);
    off(this._contentObserver, "global-destroyed", this._onGlobalDestroyed);
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
    let id = getInnerWindowID(this.tabActor.window);
    return this._programActorsCache.filter(e => e.ownerWindow == id);
  }, {
    response: { programs: RetVal("array:gl-program") }
  }),

  /**
   * Events emitted by this actor. The "program-linked" event is fired
   * every time a WebGL program was linked with its respective two shaders.
   */
  events: {
    "program-linked": {
      type: "programLinked",
      program: Arg(0, "gl-program")
    }
  },

  /**
   * Invoked whenever the current tab actor's document global is created.
   */
  _onGlobalCreated: function(window) {
    WebGLInstrumenter.handle(window, this._webglObserver);
  },

  /**
   * Invoked whenever the current tab actor's inner window is destroyed.
   */
  _onGlobalDestroyed: function(id) {
    removeFromArray(this._programActorsCache, e => e.ownerWindow == id);
    this._webglObserver.unregisterContextsForWindow(id);
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
    client.addActorPool(this);
    this.manage(this);
  }
});

/**
 * Handles adding an observer for the creation of content document globals,
 * event sent immediately after a web content document window has been set up,
 * but before any script code has been executed. This will allow us to
 * instrument the HTMLCanvasElement with the appropriate inspection methods.
 */
function ContentObserver(tabActor) {
  this._contentWindow = tabActor.window;
  this._onContentGlobalCreated = this._onContentGlobalCreated.bind(this);
  this._onInnerWindowDestroyed = this._onInnerWindowDestroyed.bind(this);
  this.startListening();
}

ContentObserver.prototype = {
  /**
   * Starts listening for the required observer messages.
   */
  startListening: function() {
    Services.obs.addObserver(
      this._onContentGlobalCreated, "content-document-global-created", false);
    Services.obs.addObserver(
      this._onInnerWindowDestroyed, "inner-window-destroyed", false);
  },

  /**
   * Stops listening for the required observer messages.
   */
  stopListening: function() {
    Services.obs.removeObserver(
      this._onContentGlobalCreated, "content-document-global-created", false);
    Services.obs.removeObserver(
      this._onInnerWindowDestroyed, "inner-window-destroyed", false);
  },

  /**
   * Fired immediately after a web content document window has been set up.
   */
  _onContentGlobalCreated: function(subject, topic, data) {
    if (subject == this._contentWindow) {
      emit(this, "global-created", subject);
    }
  },

  /**
   * Fired when an inner window is removed from the backward/forward cache.
   */
  _onInnerWindowDestroyed: function(subject, topic, data) {
    let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
    emit(this, "global-destroyed", id);
  }
};

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

    let id = getInnerWindowID(window);
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
   * @param string callbackName [optional]
   *        A custom callback function name in the observer. If unspecified,
   *        it will default to the name of the function to override.
   * @param number timing [optional]
   *        When to issue the callback in relation to the actual context
   *        function call. Availalble values are 0 for "before" (default)
   *        and 1 for "after".
   */
  _instrument: function(observer, context, funcName, callbackName, timing = 0) {
    let { cache, proxy } = observer.for(context);
    let originalFunc = context[funcName];
    let proxyFuncName = callbackName || funcName;

    context[funcName] = function(...glArgs) {
      if (timing == 0 && !observer.suppressHandlers) {
        let glBreak = observer[proxyFuncName](glArgs, cache, proxy);
        if (glBreak) return undefined;
      }

      let glResult = originalFunc.apply(this, glArgs);

      if (timing == 1 && !observer.suppressHandlers) {
        let glBreak = observer[proxyFuncName](glArgs, glResult, cache, proxy);
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
    callback: "toggleVertexAttribArray",
    functions: [
      "enableVertexAttribArray", "disableVertexAttribArray"
    ]
  }, {
    callback: "attribute_",
    functions: [
      "vertexAttrib1f", "vertexAttrib2f", "vertexAttrib3f", "vertexAttrib4f",
      "vertexAttrib1fv", "vertexAttrib2fv", "vertexAttrib3fv", "vertexAttrib4fv",
      "vertexAttribPointer"
    ]
  }, {
    callback: "uniform_",
    functions: [
      "uniform1i", "uniform2i", "uniform3i", "uniform4i",
      "uniform1f", "uniform2f", "uniform3f", "uniform4f",
      "uniform1iv", "uniform2iv", "uniform3iv", "uniform4iv",
      "uniform1fv", "uniform2fv", "uniform3fv", "uniform4fv",
      "uniformMatrix2fv", "uniformMatrix3fv", "uniformMatrix4fv"
    ]
  }, {
    timing: 1, // after
    functions: ["useProgram"]
  }, {
    callback: "draw_",
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
   * Called immediately *after* 'useProgram' is requested in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param void glResult
   *        The returned value of the original function call.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  useProgram: function(glArgs, glResult, cache) {
    // Manually keeping a cache and not using gl.getParameter(CURRENT_PROGRAM)
    // because gl.get* functions are slow as potatoes.
    cache.currentProgram = glArgs[0];
  },

  /**
   * Called immediately *before* 'drawArrays' or 'drawElements' is requested
   * in the context.
   *
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLCache cache
   *        The state storage for the WebGL context initiating this call.
   */
  draw_: function(glArgs, cache) {
    // Return true to break original function call.
    return cache.currentProgramTraits & PROGRAM_BLACKBOX_TRAIT;
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
    "getAttachedShaders",
    "getShaderSource",
    "getShaderOfType",
    "compileShader"
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

function getInnerWindowID(window) {
  return window
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils)
    .currentInnerWindowID;
}

function removeFromMap(map, predicate) {
  for (let [key, value] of map) {
    if (predicate(value)) {
      map.delete(key);
    }
  }
};

function removeFromArray(array, predicate) {
  for (let value of array) {
    if (predicate(value)) {
      array.splice(array.indexOf(value), 1);
    }
  }
}
