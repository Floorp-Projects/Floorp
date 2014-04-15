/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

const Services = require("Services");

const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const events = require("sdk/event/core");
const protocol = require("devtools/server/protocol");
const { CallWatcherActor, CallWatcherFront } = require("devtools/server/actors/call-watcher");

const { on, once, off, emit } = events;
const { method, Arg, Option, RetVal } = protocol;

exports.register = function(handle) {
  handle.addTabActor(WebAudioActor, "webaudioActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(WebAudioActor);
};

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
    "smoothingTimeConstraint": {},
    "frequencyBinCount": { "readonly": true },
  },
  "AudioDestinationNode": {},
  "ChannelSplitterNode": {},
  "ChannelMergerNode": {}
};

/**
 * Track an array of audio nodes

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
    this.node = unwrap(node);
    try {
      this.type = this.node.toString().match(/\[object (.*)\]$/)[1];
    } catch (e) {
      this.type = "";
    }
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
   * Changes a param on the audio node. Responds with a `string` that's either
   * an empty string `""` on success, or a description of the error upon
   * param set failure.
   *
   * @param String param
   *        Name of the AudioParam to change.
   * @param String value
   *        Value to change AudioParam to.
   */
  setParam: method(function (param, value) {
    // Strip quotes because sometimes UIs include that for strings
    if (typeof value === "string") {
      value = value.replace(/[\'\"]*/g, "");
    }
    try {
      if (isAudioParam(this.node, param))
        this.node[param].value = value;
      else
        this.node[param] = value;
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
    // If property does not exist, just return "undefined"
    if (!this.node[param])
      return undefined;
    let value = isAudioParam(this.node, param) ? this.node[param].value : this.node[param];
    return value;
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
   * Get an array of objects each containing a `param` and `value` property,
   * corresponding to a property name and current value of the audio node.
   */
  getParams: method(function (param) {
    let props = Object.keys(NODE_PROPERTIES[this.type]);
    return props.map(prop =>
      ({ param: prop, value: this.getParam(prop), flags: this.getParamFlags(prop) }));
  }, {
    response: { params: RetVal("json") }
  })
});

/**
 * The corresponding Front object for the AudioNodeActor.
 */
let AudioNodeFront = protocol.FrontClass(AudioNodeActor, {
  initialize: function (client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    client.addActorPool(this);
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
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    // Weak map mapping audio nodes to their corresponding actors
    this._nodeActors = new Map();

    this._callWatcher = new CallWatcherActor(this.conn, this.tabActor);
    this._callWatcher.onCall = this._onContentFunctionCall;
    this._callWatcher.setup({
      tracedGlobals: AUDIO_GLOBALS,
      startRecording: true,
      performReload: reload
    });

    // Used to track when something is happening with the web audio API
    // the first time, to ultimately fire `start-context` event
    this._firstNodeCreated = false;
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
    let source = unwrap(caller);
    let dest = unwrap(args[0]);
    let isAudioParam = dest instanceof unwrap(window.AudioParam);

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
      this._onCreateNode(unwrap(caller.destination));
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
    this._initialized = false;
    this._callWatcher.eraseRecording();

    this._callWatcher.finalize();
    this._callWatcher = null;
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
      source: Arg(0, "audionode"),
      param: Arg(1, "string")
    },
    "change-param": {
      type: "changeParam",
      source: Option(0, "audionode"),
      param: Option(0, "string"),
      value: Option(0, "string")
    },
    "create-node": {
      type: "createNode",
      source: Arg(0, "audionode")
    }
  },

  /**
   * Helper for constructing an AudioNodeActor, assigning to
   * internal weak map, and tracking via `manage` so it is assigned
   * an `actorID`.
   */
  _constructAudioNode: function (node) {
    let actor = new AudioNodeActor(this.conn, node);
    this.manage(actor);
    this._nodeActors.set(node, actor);
    return actor;
  },

  /**
   * Takes an AudioNode and returns the stored actor for it.
   * In some cases, we won't have an actor stored (for example,
   * connecting to an AudioDestinationNode, since it's implicitly
   * created), so make a new actor and store that.
   */
  _actorFor: function (node) {
    let actor = this._nodeActors.get(node);
    if (!actor) {
      actor = this._constructAudioNode(node);
    }
    return actor;
  },

  /**
   * Called on first audio node creation, signifying audio context usage
   */
  _onStartContext: function () {
    events.emit(this, "start-context");
  },

  /**
   * Called when one audio node is connected to another.
   */
  _onConnectNode: function (source, dest) {
    let sourceActor = this._actorFor(source);
    let destActor = this._actorFor(dest);
    events.emit(this, "connect-node", {
      source: sourceActor,
      dest: destActor
    });
  },

  /**
   * Called when an audio node is connected to an audio param.
   * Implement in bug 986705
   */
  _onConnectParam: function (source, dest) {
    // TODO bug 986705
  },

  /**
   * Called when an audio node is disconnected.
   */
  _onDisconnectNode: function (node) {
    let actor = this._actorFor(node);
    events.emit(this, "disconnect-node", actor);
  },

  /**
   * Called when a parameter changes on an audio node
   */
  _onParamChange: function (node, param, value) {
    let actor = this._actorFor(node);
    events.emit(this, "param-change", {
      source: actor,
      param: param,
      value: value
    });
  },

  /**
   * Called on node creation.
   */
  _onCreateNode: function (node) {
    let actor = this._constructAudioNode(node);
    events.emit(this, "create-node", actor);
  }
});

/**
 * The corresponding Front object for the WebAudioActor.
 */
let WebAudioFront = exports.WebAudioFront = protocol.FrontClass(WebAudioActor, {
  initialize: function(client, { webaudioActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: webaudioActor });
    client.addActorPool(this);
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
  return /AudioParam/.test(node[prop].toString());
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

function unwrap (obj) {
  return XPCNativeWrapper.unwrap(obj);
}
