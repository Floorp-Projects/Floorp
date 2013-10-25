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
  initialize: function(conn, id) {
    protocol.Actor.prototype.initialize.call(this, conn);
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
    let { context, shader, program, observer: { proxy } } = this;

    // Get the new shader source to inject.
    let oldText = this.text;
    let newText = text;

    // Overwrite the shader's source.
    let error = proxy.call("compileShader", context, program, shader, this.text = newText);

    // If something went wrong, revert to the previous shader.
    if (error.compile || error.link) {
      proxy.call("compileShader", context, program, shader, this.text = oldText);
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
  initialize: function(conn, id) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._shaderActorsCache = { vertex: null, fragment: null };
  },

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

    let shaderActor = new ShaderActor(this.conn);
    shaderActor.context = this.context;
    shaderActor.observer = this.observer;
    shaderActor.program = this.program;
    shaderActor.shader = this.shadersData[type].ref;
    shaderActor.text = this.shadersData[type].text;

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
  setup: method(function() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;
    this._contentObserver = new ContentObserver(this.tabActor);
    this._webglObserver = new WebGLObserver();
    on(this._contentObserver, "global-created", this._onGlobalCreated);
    on(this._webglObserver, "program-linked", this._onProgramLinked);

    this.tabActor.window.location.reload();
  }, {
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
    off(this._webglObserver, "program-linked", this._onProgramLinked);
  }, {
   oneway: true
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
   * Invoked whenever the current WebGL context links a program.
   */
  _onProgramLinked: function(gl, program, shaders) {
    let observer = this._webglObserver;
    let shadersData = { vertex: null, fragment: null };

    for (let shader of shaders) {
      let text = observer.cache.call("getShaderInfo", shader);
      let data = { ref: shader, text: text };

      // Make sure the shader data object always contains the vertex shader
      // first, and the fragment shader second. There are no guarantees that
      // the compilation order of shaders in the debuggee is always the same.
      if (gl.getShaderParameter(shader, gl.SHADER_TYPE) == gl.VERTEX_SHADER) {
        shadersData.vertex = data;
      } else {
        shadersData.fragment = data;
      }
    }

    let programActor = new ProgramActor(this.conn);
    programActor.context = gl;
    programActor.observer = observer;
    programActor.program = program;
    programActor.shadersData = shadersData;

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
  this._contentWindow = tabActor.browser.contentWindow;
  this._onContentGlobalCreated = this._onContentGlobalCreated.bind(this);
  this.startListening();
}

ContentObserver.prototype = {
  /**
   * Starts listening for the required observer messages.
   */
  startListening: function() {
    Services.obs.addObserver(
      this._onContentGlobalCreated, "content-document-global-created", false);
  },

  /**
   * Stops listening for the required observer messages.
   */
  stopListening: function() {
    Services.obs.removeObserver(
      this._onContentGlobalCreated, "content-document-global-created", false);
  },

  /**
   * Fired immediately after a web content document window has been set up.
   */
  _onContentGlobalCreated: function(subject, topic, data) {
    if (subject == this._contentWindow) {
      emit(this, "global-created", subject);
    }
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

      // Link our observer to the new WebGL context methods.
      for (let { timing, callback, functions } of self._methods) {
        for (let func of functions) {
          self._instrument(observer, context, func, timing, callback);
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
   *        The targeted context instance.
   * @param string funcName
   *        The function to override.
   * @param string timing [optional]
   *        When to issue the callback in relation to the actual context
   *        function call. Availalble values are "before" and "after" (default).
   * @param string callbackName [optional]
   *        A custom callback function name in the observer. If unspecified,
   *        it will default to the name of the function to override.
   */
  _instrument: function(observer, context, funcName, timing, callbackName) {
    let originalFunc = context[funcName];

    context[funcName] = function() {
      let glArgs = Array.slice(arguments);
      let glResult, glBreak;

      if (timing == "before" && !observer.suppressHandlers) {
        glBreak = observer.call(callbackName || funcName, context, glArgs);
        if (glBreak) return undefined;
      }

      glResult = originalFunc.apply(this, glArgs);

      if (timing == "after" && !observer.suppressHandlers) {
        glBreak = observer.call(callbackName || funcName, context, glArgs, glResult);
        if (glBreak) return undefined;
      }

      return glResult;
    };
  },

  /**
   * Override mappings for WebGL methods.
   */
  _methods: [{
    timing: "after",
    functions: [
      "linkProgram", "getAttribLocation", "getUniformLocation"
    ]
  }, {
    timing: "before",
    callback: "toggleVertexAttribArray",
    functions: [
      "enableVertexAttribArray", "disableVertexAttribArray"
    ]
  }, {
    timing: "before",
    callback: "attribute_",
    functions: [
      "vertexAttrib1f", "vertexAttrib2f", "vertexAttrib3f", "vertexAttrib4f",
      "vertexAttrib1fv", "vertexAttrib2fv", "vertexAttrib3fv", "vertexAttrib4fv",
      "vertexAttribPointer"
    ]
  }, {
    timing: "before",
    callback: "uniform_",
    functions: [
      "uniform1i", "uniform2i", "uniform3i", "uniform4i",
      "uniform1f", "uniform2f", "uniform3f", "uniform4f",
      "uniform1iv", "uniform2iv", "uniform3iv", "uniform4iv",
      "uniform1fv", "uniform2fv", "uniform3fv", "uniform4fv",
      "uniformMatrix2fv", "uniformMatrix3fv", "uniformMatrix4fv"
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
  this.cache = new WebGLCache(this);
  this.proxy = new WebGLProxy(this);
}

WebGLObserver.prototype = {
  /**
   * Set this flag to true to stop observing any context function calls.
   */
  suppressHandlers: false,

  /**
   * Called immediately *after* 'linkProgram' is requested in the context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context initiating this call.
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param void glResult
   *        The returned value of the original function call.
   */
  linkProgram: function(gl, glArgs, glResult) {
    let program = glArgs[0];
    let shaders = gl.getAttachedShaders(program);

    for (let shader of shaders) {
      let source = gl.getShaderSource(shader);
      this.cache.call("addShaderInfo", shader, source);
    }

    emit(this, "program-linked", gl, program, shaders);
  },

  /**
   * Called immediately *after* 'getAttribLocation' is requested in the context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context initiating this call.
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param GLint glResult
   *        The returned value of the original function call.
   */
  getAttribLocation: function(gl, glArgs, glResult) {
    let [program, name] = glArgs;
    this.cache.call("addAttribute", program, name, glResult);
  },

  /**
   * Called immediately *after* 'getUniformLocation' is requested in the context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context initiating this call.
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   * @param WebGLUniformLocation glResult
   *        The returned value of the original function call.
   */
  getUniformLocation: function(gl, glArgs, glResult) {
    let [program, name] = glArgs;
    this.cache.call("addUniform", program, name, glResult);
  },

  /**
   * Called immediately *before* 'enableVertexAttribArray' or
   * 'disableVertexAttribArray'is requested in the context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context initiating this call.
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   */
  toggleVertexAttribArray: function(gl, glArgs) {
    glArgs[0] = this.cache.call("getCurrentAttributeLocation", glArgs[0]);
    return glArgs[0] < 0;
  },

  /**
   * Called immediately *before* 'attribute_' is requested in the context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context initiating this call.
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   */
  attribute_: function(gl, glArgs) {
    glArgs[0] = this.cache.call("getCurrentAttributeLocation", glArgs[0]);
    return glArgs[0] < 0;
  },

  /**
   * Called immediately *before* 'uniform_' is requested in the context.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context initiating this call.
   * @param array glArgs
   *        Overridable arguments with which the function is called.
   */
  uniform_: function(gl, glArgs) {
    glArgs[0] = this.cache.call("getCurrentUniformLocation", glArgs[0]);
    return !glArgs[0];
  },

  /**
   * Executes a function in this object.
   * This method makes sure that any handlers in the context observer are
   * suppressed, hence stopping observing any context function calls.
   *
   * @param string funcName
   *        The function to call.
   */
  call: function(funcName, ...args) {
    let prevState = this.suppressHandlers;

    this.suppressHandlers = true;
    let result = this[funcName].apply(this, args);
    this.suppressHandlers = prevState;

    return result;
  }
};

/**
 * A cache storing WebGL state, like shaders, attributes or uniforms.
 *
 * @param WebGLObserver observer
 *        The observer for the target context.
 */
function WebGLCache(observer) {
  this._observer = observer;

  this._shaders = new Map();
  this._attributes = [];
  this._uniforms = [];
  this._attributesBridge = new Map();
  this._uniformsBridge = new Map();
}

WebGLCache.prototype = {
  /**
   * Adds shader information to the cache.
   *
   * @param WebGLShader shader
   *        The shader for which the source is to be cached. If the shader
   *        was already cached, nothing happens.
   * @param string text
   *        The current shader text.
   */
  _addShaderInfo: function(shader, text) {
    if (!this._shaders.has(shader)) {
      this._shaders.set(shader, text);
    }
  },

  /**
   * Gets shader information from the cache.
   *
   * @param WebGLShader shader
   *        The shader for which the source was cached.
   * @return object | null
   *         The original shader source, or null if there's a cache miss.
   */
  _getShaderInfo: function(shader) {
    return this._shaders.get(shader);
  },

  /**
   * Adds an attribute to the cache.
   *
   * @param WebGLProgram program
   *        The program for which the attribute is bound. If the attribute
   *        was already cached, nothing happens.
   * @param string name
   *        The attribute name.
   * @param GLint value
   *        The attribute value.
   */
  _addAttribute: function(program, name, value) {
    let isCached = this._attributes.some(e => e.program == program && e.name == name);
    if (isCached || value < 0) {
      return;
    }
    let attributeInfo = {
      program: program,
      name: name,
      value: value
    };
    this._attributes.push(attributeInfo);
    this._attributesBridge.set(value, attributeInfo);
  },

  /**
   * Adds a uniform to the cache.
   *
   * @param WebGLProgram program
   *        The program for which the uniform is bound. If the uniform
   *        was already cached, nothing happens.
   * @param string name
   *        The uniform name.
   * @param WebGLUniformLocation value
   *        The uniform value.
   */
  _addUniform: function(program, name, value) {
    let isCached = this._uniforms.some(e => e.program == program && e.name == name);
    if (isCached || !value) {
      return;
    }
    let uniformInfo = {
      program: program,
      name: name,
      value: value
    };
    this._uniforms.push(uniformInfo);
    this._uniformsBridge.set(new XPCNativeWrapper(value), uniformInfo);
  },

  /**
   * Gets all the cached attributes for a specific program.
   *
   * @param WebGLProgram program
   *        The program for which the attributes are bound.
   * @return array
   *         A list containing information about all the attributes.
   */
  _getAttributesForProgram: function(program) {
    return this._attributes.filter(e => e.program == program);
  },

  /**
   * Gets all the cached uniforms for a specific program.
   *
   * @param WebGLProgram program
   *        The program for which the uniforms are bound.
   * @return array
   *         A list containing information about all the uniforms.
   */
  _getUniformsForProgram: function(program) {
    return this._uniforms.filter(e => e.program == program);
  },

  /**
   * Updates the attribute locations for a specific program.
   * This is necessary, for example, when the shader is relinked and all the
   * attribute locations become obsolete.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context owning the program.
   * @param WebGLProgram program
   *        The program for which the attributes need updating.
   */
  _updateAttributesForProgram: function(gl, program) {
    let dirty = this._attributes.filter(e => e.program == program);
    dirty.forEach(e => e.value = gl.getAttribLocation(program, e.name));
  },

  /**
   * Updates the uniform locations for a specific program.
   * This is necessary, for example, when the shader is relinked and all the
   * uniform locations become obsolete.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context owning the program.
   * @param WebGLProgram program
   *        The program for which the uniforms need updating.
   */
  _updateUniformsForProgram: function(gl, program) {
    let dirty = this._uniforms.filter(e => e.program == program);
    dirty.forEach(e => e.value = gl.getUniformLocation(program, e.name));
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
  _getCurrentAttributeLocation: function(initialValue) {
    let currentInfo = this._attributesBridge.get(initialValue);
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
  _getCurrentUniformLocation: function(initialValue) {
    let currentInfo = this._uniformsBridge.get(initialValue);
    return currentInfo ? currentInfo.value : initialValue;
  },

  /**
   * Executes a function in this object.
   * This method makes sure that any handlers in the context observer are
   * suppressed, hence stopping observing any context function calls.
   *
   * @param string funcName
   *        The function to call.
   * @return any
   *         The called function result.
   */
  call: function(funcName, ...aArgs) {
    let prevState = this._observer.suppressHandlers;

    this._observer.suppressHandlers = true;
    let result = this["_" + funcName].apply(this, aArgs);
    this._observer.suppressHandlers = prevState;

    return result;
  }
};

/**
 * A mechanism for injecting or qureying state into/from a WebGL context.
 *
 * @param WebGLObserver observer
 *        The observer for the target context.
 */
function WebGLProxy(observer) {
  this._observer = observer;
}

WebGLProxy.prototype = {
  get cache() this._observer.cache,

  /**
   * Changes a shader's source code and relinks the respective program.
   *
   * @param WebGLRenderingContext gl
   *        The WebGL context owning the program.
   * @param WebGLProgram program
   *        The program who's linked shader is to be modified.
   * @param WebGLShader shader
   *        The shader to be modified.
   * @param string text
   *        The new shader source code.
   * @return string
   *         The shader's compilation and linking status.
   */
  _compileShader: function(gl, program, shader, text) {
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

    this.cache.call("updateAttributesForProgram", gl, program);
    this.cache.call("updateUniformsForProgram", gl, program);

    return error;
  },

  /**
   * Executes a function in this object.
   * This method makes sure that any handlers in the context observer are
   * suppressed, hence stopping observing any context function calls.
   *
   * @param string funcName
   *        The function to call.
   * @return any
   *         The called function result.
   */
  call: function(funcName, ...aArgs) {
    let prevState = this._observer.suppressHandlers;

    this._observer.suppressHandlers = true;
    let result = this["_" + funcName].apply(this, aArgs);
    this._observer.suppressHandlers = prevState;

    return result;
  }
};
