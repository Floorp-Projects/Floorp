/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const Services = require("Services");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const events = require("sdk/event/core");
const { on: systemOn, off: systemOff } = require("sdk/system/events");
const { setTimeout, clearTimeout } = require("sdk/timers");
const protocol = require("devtools/server/protocol");
const { CallWatcherActor, CallWatcherFront } = require("devtools/server/actors/call-watcher");
const { ThreadActor } = require("devtools/server/actors/script");
const { on, once, off, emit } = events;
const { method, Arg, Option, RetVal } = protocol;

exports.register = function(handle) {
  handle.addTabActor(WebAudioActor, "webaudioActor");
  handle.addGlobalActor(WebAudioActor, "webaudioActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(WebAudioActor);
  handle.removeGlobalActor(WebAudioActor);
};

// In milliseconds, how often should AudioNodes poll to see
// if an AudioParam's value has changed to emit to the client.
const PARAM_POLLING_FREQUENCY = 1000;

const AUDIO_GLOBALS = [
  "AudioContext", "AudioNode"
];

const NODE_CREATION_METHODS = [
  "createBufferSource", "createMediaElementSource", "createMediaStreamSource",
  "createMediaStreamDestination", "createScriptProcessor", "createAnalyser",
  "createGain", "createDelay", "createBiquadFilter", "createWaveShaper",
  "createPanner", "createConvolver", "createChannelSplitter", "createChannelMerger",
  "createDynamicsCompressor", "createOscillator"
];

const NODE_ROUTING_METHODS = [
  "connect", "disconnect"
];

const NODE_PROPERTIES = {
  "OscillatorNode": {
    "type": {},
    "frequency": {},
    "detune": {}
  },
  "GainNode": {
    "gain": {}
  },
  "DelayNode": {
    "delayTime": {}
  },
  "AudioBufferSourceNode": {
    "buffer": { "Buffer": true },
    "playbackRate": {},
    "loop": {},
    "loopStart": {},
    "loopEnd": {}
  },
  "ScriptProcessorNode": {
    "bufferSize": { "readonly": true }
  },
  "PannerNode": {
    "panningModel": {},
    "distanceModel": {},
    "refDistance": {},
    "maxDistance": {},
    "rolloffFactor": {},
    "coneInnerAngle": {},
    "coneOuterAngle": {},
    "coneOuterGain": {}
  },
  "ConvolverNode": {
    "buffer": { "Buffer": true },
    "normalize": {},
  },
  "DynamicsCompressorNode": {
    "threshold": {},
    "knee": {},
    "ratio": {},
    "reduction": {},
    "attack": {},
    "release": {}
  },
  "BiquadFilterNode": {
    "type": {},
    "frequency": {},
    "Q": {},
    "detune": {},
    "gain": {}
  },
  "WaveShaperNode": {
    "curve": { "Float32Array": true },
    "oversample": {}
  },
  "AnalyserNode": {
    "fftSize": {},
    "minDecibels": {},
    "maxDecibels": {},
    "smoothingTimeConstant": {},
    "frequencyBinCount": { "readonly": true },
  },
  "AudioDestinationNode": {},
  "ChannelSplitterNode": {},
  "ChannelMergerNode": {},
  "MediaElementAudioSourceNode": {},
  "MediaStreamAudioSourceNode": {},
  "MediaStreamAudioDestinationNode": {
    "stream": { "MediaStream": true }
  }
};

/**
 * An Audio Node actor allowing communication to a specific audio node in the
 * Audio Context graph.
 */
let AudioNodeActor = exports.AudioNodeActor = protocol.ActorClass({
  typeName: "audionode",

  /**
   * Create the Audio Node actor.
   *
   * @param DebuggerServerConnection conn
   *        The server connection.
   * @param AudioNode node
   *        The AudioNode that was created.
   */
  initialize: function (conn, node) {
    protocol.Actor.prototype.initialize.call(this, conn);

    // Store ChromeOnly property `id` to identify AudioNode,
    // rather than storing a strong reference, and store a weak
    // ref to underlying node for controlling.
    this.nativeID = node.id;
    this.node = Cu.getWeakReference(node);

    try {
      this.type = getConstructorName(node);
    } catch (e) {
      this.type = "";
    }
  },

  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
  },

  /**
   * Returns the name of the audio type.
   * Examples: "OscillatorNode", "MediaElementAudioSourceNode"
   */
  getType: method(function () {
    return this.type;
  }, {
    response: { type: RetVal("string") }
  }),

  /**
   * Returns a boolean indicating if the node is a source node,
   * like BufferSourceNode, MediaElementAudioSourceNode, OscillatorNode, etc.
   */
  isSource: method(function () {
    return !!~this.type.indexOf("Source") || this.type === "OscillatorNode";
  }, {
    response: { source: RetVal("boolean") }
  }),

  /**
   * Changes a param on the audio node. Responds with either `undefined`
   * on success, or a description of the error upon param set failure.
   *
   * @param String param
   *        Name of the AudioParam to change.
   * @param String value
   *        Value to change AudioParam to.
   */
  setParam: method(function (param, value) {
    let node = this.node.get();

    if (node === null) {
      return CollectedAudioNodeError();
    }

    try {
      if (isAudioParam(node, param))
        node[param].value = value;
      else
        node[param] = value;

      return undefined;
    } catch (e) {
      return constructError(e);
    }
  }, {
    request: {
      param: Arg(0, "string"),
      value: Arg(1, "nullable:primitive")
    },
    response: { error: RetVal("nullable:json") }
  }),

  /**
   * Gets a param on the audio node.
   *
   * @param String param
   *        Name of the AudioParam to fetch.
   */
  getParam: method(function (param) {
    let node = this.node.get();

    if (node === null) {
      return CollectedAudioNodeError();
    }

    // Check to see if it's an AudioParam -- if so,
    // return the `value` property of the parameter.
    let value = isAudioParam(node, param) ? node[param].value : node[param];

    // Return the grip form of the value; at this time,
    // there shouldn't be any non-primitives at the moment, other than
    // AudioBuffer or Float32Array references and the like,
    // so this just formats the value to be displayed in the VariablesView,
    // without using real grips and managing via actor pools.
    return createGrip(value);
  }, {
    request: {
      param: Arg(0, "string")
    },
    response: { text: RetVal("nullable:primitive") }
  }),

  /**
   * Get an object containing key-value pairs of additional attributes
   * to be consumed by a front end, like if a property should be read only,
   * or is a special type (Float32Array, Buffer, etc.)
   *
   * @param String param
   *        Name of the AudioParam whose flags are desired.
   */
  getParamFlags: method(function (param) {
    return (NODE_PROPERTIES[this.type] || {})[param];
  }, {
    request: { param: Arg(0, "string") },
    response: { flags: RetVal("nullable:primitive") }
  }),

  /**
   * Get an array of objects each containing a `param`, `value` and `flags` property,
   * corresponding to a property name and current value of the audio node, and any
   * associated flags as defined by NODE_PROPERTIES.
   */
  getParams: method(function () {
    let props = Object.keys(NODE_PROPERTIES[this.type]);
    return props.map(prop =>
      ({ param: prop, value: this.getParam(prop), flags: this.getParamFlags(prop) }));
  }, {
    response: { params: RetVal("json") }
  }),

  /**
   * Returns a boolean indicating whether or not
   * the underlying AudioNode has been collected yet or not.
   *
   * @return Boolean
   */
  isAlive: function () {
    return !!this.node.get();
  }
});

/**
 * The corresponding Front object for the AudioNodeActor.
 */
let AudioNodeFront = protocol.FrontClass(AudioNodeActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.manage(this);
  }
});

/**
 * The Web Audio Actor handles simple interaction with an AudioContext
 * high-level methods. After instantiating this actor, you'll need to set it
 * up by calling setup().
 */
let WebAudioActor = exports.WebAudioActor = protocol.ActorClass({
  typeName: "webaudio",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;

    this._onContentFunctionCall = this._onContentFunctionCall.bind(this);

    // Store ChromeOnly ID (`nativeID` property on AudioNodeActor) mapped
    // to the associated actorID, so we don't have to expose `nativeID`
    // to the client in any way.
    this._nativeToActorID = new Map();

    this._onDestroyNode = this._onDestroyNode.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);
  },

  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
  },

  /**
   * Starts waiting for the current tab actor's document global to be
   * created, in order to instrument the Canvas context and become
   * aware of everything the content does with Web Audio.
   *
   * See ContentObserver and WebAudioInstrumenter for more details.
   */
  setup: method(function({ reload }) {
    // Used to track when something is happening with the web audio API
    // the first time, to ultimately fire `start-context` event
    this._firstNodeCreated = false;

    // Clear out stored nativeIDs on reload as we do not want to track
    // AudioNodes that are no longer on this document.
    this._nativeToActorID.clear();

    if (this._initialized) {
      return;
    }

    this._initialized = true;

    this._callWatcher = new CallWatcherActor(this.conn, this.tabActor);
    this._callWatcher.onCall = this._onContentFunctionCall;
    this._callWatcher.setup({
      tracedGlobals: AUDIO_GLOBALS,
      startRecording: true,
      performReload: reload,
      holdWeak: true,
      storeCalls: false
    });
    // Bind to the `global-destroyed` event on the content observer so we can
    // unbind events between the global destruction and the `finalize` cleanup
    // method on the actor.
    // TODO expose these events on CallWatcherActor itself, bug 1021321
    on(this._callWatcher._contentObserver, "global-destroyed", this._onGlobalDestroyed);
  }, {
    request: { reload: Option(0, "boolean") },
    oneway: true
  }),

  /**
   * Invoked whenever an instrumented function is called, like an AudioContext
   * method or an AudioNode method.
   */
  _onContentFunctionCall: function(functionCall) {
    let { name } = functionCall.details;

    // All Web Audio nodes inherit from AudioNode's prototype, so
    // hook into the `connect` and `disconnect` methods
    if (WebAudioFront.NODE_ROUTING_METHODS.has(name)) {
      this._handleRoutingCall(functionCall);
    }
    else if (WebAudioFront.NODE_CREATION_METHODS.has(name)) {
      this._handleCreationCall(functionCall);
    }
  },

  _handleRoutingCall: function(functionCall) {
    let { caller, args, window, name } = functionCall.details;
    let source = caller;
    let dest = args[0];
    let isAudioParam = dest ? getConstructorName(dest) === "AudioParam" : false;

    // audionode.connect(param)
    if (name === "connect" && isAudioParam) {
      this._onConnectParam(source, dest);
    }
    // audionode.connect(node)
    else if (name === "connect") {
      this._onConnectNode(source, dest);
    }
    // audionode.disconnect()
    else if (name === "disconnect") {
      this._onDisconnectNode(source);
    }
  },

  _handleCreationCall: function (functionCall) {
    let { caller, result } = functionCall.details;
    // Keep track of the first node created, so we can alert
    // the front end that an audio context is being used since
    // we're not hooking into the constructor itself, just its
    // instance's methods.
    if (!this._firstNodeCreated) {
      // Fire the start-up event if this is the first node created
      // and trigger a `create-node` event for the context destination
      this._onStartContext();
      this._onCreateNode(caller.destination);
      this._firstNodeCreated = true;
    }
    this._onCreateNode(result);
  },

  /**
   * Stops listening for document global changes and puts this actor
   * to hibernation. This method is called automatically just before the
   * actor is destroyed.
   */
  finalize: method(function() {
    if (!this._initialized) {
      return;
    }
    this.tabActor = null;
    this._initialized = false;
    off(this._callWatcher._contentObserver, "global-destroyed", this._onGlobalDestroyed);
    this.disableChangeParamEvents();
    this._nativeToActorID = null;
    this._callWatcher.eraseRecording();
    this._callWatcher.finalize();
    this._callWatcher = null;
  }, {
   oneway: true
  }),

  /**
   * Takes an AudioNodeActor and a duration specifying how often
   * should the node's parameters be polled to detect changes. Emits
   * `change-param` when a change is found.
   *
   * Currently, only one AudioNodeActor can be listened to at a time.
   *
   * `wait` is used in tests to specify the poll timer.
   */
  enableChangeParamEvents: method(function (nodeActor, wait) {
    // For now, only have one node being polled
    this.disableChangeParamEvents();

    // Ignore if node is dead
    if (!nodeActor.isAlive()) {
      return;
    }

    let previous = mapAudioParams(nodeActor);

    // Store the ID of the node being polled
    this._pollingID = nodeActor.actorID;

    this.poller = new Poller(() => {
      // If node has been collected, disable param polling
      if (!nodeActor.isAlive()) {
        this.disableChangeParamEvents();
        return;
      }

      let current = mapAudioParams(nodeActor);
      diffAudioParams(previous, current).forEach(changed => {
        this._onChangeParam(nodeActor, changed);
      });
      previous = current;
    }).on(wait || PARAM_POLLING_FREQUENCY);
  }, {
    request: {
      node: Arg(0, "audionode"),
      wait: Arg(1, "nullable:number"),
    },
    oneway: true
  }),

  disableChangeParamEvents: method(function () {
    if (this.poller) {
      this.poller.off();
    }
    this._pollingID = null;
  }, {
    oneway: true
  }),

  /**
   * Events emitted by this actor.
   */
  events: {
    "start-context": {
      type: "startContext"
    },
    "connect-node": {
      type: "connectNode",
      source: Option(0, "audionode"),
      dest: Option(0, "audionode")
    },
    "disconnect-node": {
      type: "disconnectNode",
      source: Arg(0, "audionode")
    },
    "connect-param": {
      type: "connectParam",
      source: Option(0, "audionode"),
      dest: Option(0, "audionode"),
      param: Option(0, "string")
    },
    "create-node": {
      type: "createNode",
      source: Arg(0, "audionode")
    },
    "destroy-node": {
      type: "destroyNode",
      source: Arg(0, "audionode")
    },
    "change-param": {
      type: "changeParam",
      param: Option(0, "string"),
      newValue: Option(0, "json"),
      oldValue: Option(0, "json"),
      actorID: Option(0, "string")
    }
  },

  /**
   * Helper for constructing an AudioNodeActor, assigning to
   * internal weak map, and tracking via `manage` so it is assigned
   * an `actorID`.
   */
  _constructAudioNode: function (node) {
    // Ensure AudioNode is wrapped.
    node = new XPCNativeWrapper(node);

    this._instrumentParams(node);

    let actor = new AudioNodeActor(this.conn, node);
    this.manage(actor);
    this._nativeToActorID.set(node.id, actor.actorID);
    return actor;
  },

  /**
   * Takes an XrayWrapper node, and attaches the node's `nativeID`
   * to the AudioParams as `_parentID`, as well as the the type of param
   * as a string on `_paramName`. Used to tag AudioParams for `connect-param` events.
   */
  _instrumentParams: function (node) {
    let type = getConstructorName(node);
    Object.keys(NODE_PROPERTIES[type])
      .filter(isAudioParam.bind(null, node))
      .forEach(paramName => {
        let param = node[paramName];
        param._parentID = node.id;
        param._paramName = paramName;
      });
  },

  /**
   * Takes an AudioNode and returns the stored actor for it.
   * In some cases, we won't have an actor stored (for example,
   * connecting to an AudioDestinationNode, since it's implicitly
   * created), so make a new actor and store that.
   */
  _getActorByNativeID: function (nativeID) {
    // If the WebAudioActor has already been finalized, the `_nativeToActorID`
    // map will already be destroyed -- the lingering destruction events
    // seem to only occur in e10s, so add an extra check here to disregard
    // these late events
    if (!this._nativeToActorID) {
      return null;
    }

    // Ensure we have a Number, rather than a string
    // return via notification.
    nativeID = ~~nativeID;

    let actorID = this._nativeToActorID.get(nativeID);
    let actor = actorID != null ? this.conn.getActor(actorID) : null;
    return actor;
  },

  /**
   * Called on first audio node creation, signifying audio context usage
   */
  _onStartContext: function () {
    systemOn("webaudio-node-demise", this._onDestroyNode);
    emit(this, "start-context");
  },

  /**
   * Called when one audio node is connected to another.
   */
  _onConnectNode: function (source, dest) {
    let sourceActor = this._getActorByNativeID(source.id);
    let destActor = this._getActorByNativeID(dest.id);
    emit(this, "connect-node", {
      source: sourceActor,
      dest: destActor
    });
  },

  /**
   * Called when an audio node is connected to an audio param.
   */
  _onConnectParam: function (source, param) {
    let sourceActor = this._getActorByNativeID(source.id);
    let destActor = this._getActorByNativeID(param._parentID);
    emit(this, "connect-param", {
      source: sourceActor,
      dest: destActor,
      param: param._paramName
    });
  },

  /**
   * Called when an audio node is disconnected.
   */
  _onDisconnectNode: function (node) {
    let actor = this._getActorByNativeID(node.id);
    emit(this, "disconnect-node", actor);
  },

  /**
   * Called when an AudioParam that's being listened to changes.
   * Takes an AudioNodeActor and an object with `newValue`, `oldValue`, and `param` name.
   */
  _onChangeParam: function (actor, changed) {
    changed.actorID = actor.actorID;
    emit(this, "change-param", changed);
  },

  /**
   * Called on node creation.
   */
  _onCreateNode: function (node) {
    let actor = this._constructAudioNode(node);
    emit(this, "create-node", actor);
  },

  /**
   * Called when `webaudio-node-demise` is triggered,
   * and emits the associated actor to the front if found.
   */
  _onDestroyNode: function ({data}) {
    // Cast to integer.
    let nativeID = ~~data;

    let actor = this._getActorByNativeID(nativeID);

    // If actorID exists, emit; in the case where we get demise
    // notifications for a document that no longer exists,
    // the mapping should not be found, so we do not emit an event.
    if (actor) {
      // Turn off polling for changes if on for this node
      if (this._pollingID === actor.actorID) {
        this.disableChangeParamEvents();
      }
      this._nativeToActorID.delete(nativeID);
      emit(this, "destroy-node", actor);
    }
  },

  /**
   * Called when the underlying ContentObserver fires `global-destroyed`
   * so we can cleanup some things between the global being destroyed and
   * when the actor's `finalize` method gets called.
   */
  _onGlobalDestroyed: function (id) {
    if (this._callWatcher._tracedWindowId !== id) {
      return;
    }

    if (this._nativeToActorID) {
      this._nativeToActorID.clear();
    }
    systemOff("webaudio-node-demise", this._onDestroyNode);
  }
});

/**
 * The corresponding Front object for the WebAudioActor.
 */
let WebAudioFront = exports.WebAudioFront = protocol.FrontClass(WebAudioActor, {
  initialize: function(client, { webaudioActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: webaudioActor });
    this.manage(this);
  }
});

WebAudioFront.NODE_CREATION_METHODS = new Set(NODE_CREATION_METHODS);
WebAudioFront.NODE_ROUTING_METHODS = new Set(NODE_ROUTING_METHODS);

/**
 * Determines whether or not property is an AudioParam.
 *
 * @param AudioNode node
 *        An AudioNode.
 * @param String prop
 *        Property of `node` to evaluate to see if it's an AudioParam.
 * @return Boolean
 */
function isAudioParam (node, prop) {
  return !!(node[prop] && /AudioParam/.test(node[prop].toString()));
}

/**
 * Takes an `Error` object and constructs a JSON-able response
 *
 * @param Error err
 *        A TypeError, RangeError, etc.
 * @return Object
 */
function constructError (err) {
  return {
    message: err.message,
    type: err.constructor.name
  };
}

/**
 * Creates and returns a JSON-able response used to indicate
 * attempt to access an AudioNode that has been GC'd.
 *
 * @return Object
 */
function CollectedAudioNodeError () {
  return {
    message: "AudioNode has been garbage collected and can no longer be reached.",
    type: "UnreachableAudioNode"
  };
}

/**
 * Takes an object and converts it's `toString()` form, like
 * "[object OscillatorNode]" or "[object Float32Array]",
 * or XrayWrapper objects like "[object XrayWrapper [object Array]]"
 * to a string of just the constructor name, like "OscillatorNode",
 * or "Float32Array".
 */
function getConstructorName (obj) {
  return obj.toString().match(/\[object ([^\[\]]*)\]\]?$/)[1];
}

/**
 * Create a value grip for `value`, or fallback to a grip-like object
 * for renderable information for the front-end for things like Float32Arrays,
 * AudioBuffers, without tracking them in an actor pool.
 */
function createGrip (value) {
  try {
    return ThreadActor.prototype.createValueGrip(value);
  }
  catch (e) {
    return {
      type: "object",
      preview: {
        kind: "ObjectWithText",
        text: ""
      },
      class: getConstructorName(value)
    };
  }
}

/**
 * Takes an AudioNodeActor and maps its current parameter values
 * to a hash, where the property is the AudioParam name, and value
 * is the current value.
 */
function mapAudioParams (node) {
  return node.getParams().reduce(function (obj, p) {
    obj[p.param] = p.value;
    return obj;
  }, {});
}

/**
 * Takes an object of previous and current values of audio parameters,
 * and compares them. If they differ, emit a `change-param` event.
 *
 * @param Object prev
 *        Hash of previous set of AudioParam values.
 * @param Object current
 *        Hash of current set of AudioParam values.
 */
function diffAudioParams (prev, current) {
  return Object.keys(current).reduce((changed, param) => {
    if (!equalGrips(current[param], prev[param])) {
      changed.push({
        param: param,
        oldValue: prev[param],
        newValue: current[param]
      });
    }
    return changed;
  }, []);
}

/**
 * Compares two grip objects to determine if they're equal or not.
 *
 * @param Any a
 * @param Any a
 * @return Boolean
 */
function equalGrips (a, b) {
  let aType = typeof a;
  let bType = typeof b;
  if (aType !== bType) {
    return false;
  } else if (aType === "object") {
    // In this case, we are comparing two objects, like an ArrayBuffer or Float32Array,
    // or even just plain "null"s (which grip's will have `type` property "null",
    // and we have no way of showing more information than its class, so assume
    // these are equal since nothing can be updated with information of value.
    if (a.type === b.type) {
      return true;
    }
    // Otherwise return false -- this could be a case of a property going from `null`
    // to having an ArrayBuffer or an object, in which case we should update it.
    return false;
  } else {
    return a === b;
  }
}

/**
 * Poller class -- takes a function, and call be turned on and off
 * via methods to execute `fn` on the interval specified during `on`.
 */
function Poller (fn) {
  this.fn = fn;
}

Poller.prototype.on = function (wait) {
  let poller = this;
  poller.timer = setTimeout(poll, wait);
  function poll () {
    poller.fn();
    poller.timer = setTimeout(poll, wait);
  }
  return this;
};

Poller.prototype.off = function () {
  if (this.timer) {
    clearTimeout(this.timer);
  }
  return this;
};
