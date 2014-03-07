/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");

const Services = require("Services");

const events = require("sdk/event/core");
const protocol = require("devtools/server/protocol");

const { on, once, off, emit } = events;
const { method, Arg, Option, RetVal } = protocol;
const { AudioNodeActor } = require("devtools/server/actors/audionode");
const console = Cu.import("resource://gre/modules/devtools/Console.jsm").console;

exports.register = function(handle) {
  handle.addTabActor(WebAudioActor, "webaudioActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(WebAudioActor);
};

/**
 * A WebGL Shader contributing to building a WebGL Program.

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
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    this._onGlobalDestroyed = this._onGlobalDestroyed.bind(this);

    this._onStartContext = this._onStartContext.bind(this);
    this._onConnectNode = this._onConnectNode.bind(this);
    this._onConnectParam = this._onConnectParam.bind(this);
    this._onDisconnectNode = this._onDisconnectNode.bind(this);
    this._onParamChange = this._onParamChange.bind(this);
    this._onCreateNode = this._onCreateNode.bind(this);
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

    this._contentObserver = new ContentObserver(this.tabActor);
    this._webaudioObserver = new WebAudioObserver();

    on(this._contentObserver, "global-created", this._onGlobalCreated);
    on(this._contentObserver, "global-destroyed", this._onGlobalDestroyed);

    on(this._webaudioObserver, "start-context", this._onStartContext);
    on(this._webaudioObserver, "connect-node", this._onConnectNode);
    on(this._webaudioObserver, "connect-param", this._onConnectParam);
    on(this._webaudioObserver, "disconnect-node", this._onDisconnectNode);
    on(this._webaudioObserver, "param-change", this._onParamChange);
    on(this._webaudioObserver, "create-node", this._onCreateNode);

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

    off(this._webaudioObserver, "start-context", this._onStartContext);
    off(this._webaudioObserver, "connect-node", this._onConnectNode);
    off(this._webaudioObserver, "connect-param", this._onConnectParam);
    off(this._webaudioObserver, "disconnect-node", this._onDisconnectNode);
    off(this._webaudioObserver, "param-change", this._onParamChange);
    off(this._webaudioObserver, "create-node", this._onCreateNode);

    this._contentObserver = null;
    this._webaudioObserver = null;
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
   * Invoked whenever the current tab actor's document global is created.
   */
  _onGlobalCreated: function(window) {
    WebAudioInstrumenter.handle(window, this._webaudioObserver);
  },

  /**
   * Invoked whenever the current tab actor's inner window is destroyed.
   */
  _onGlobalDestroyed: function(id) {
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

/**
 * Handles adding an observer for the creation of content document globals,
 * event sent immediately after a web content document window has been set up,
 * but before any script code has been executed. This will allow us to
 * instrument the HTMLCanvasElement with the appropriate inspection methods.
 * TODO use ContentObserver from bug 917226 once landed, bug 986704
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
 * Instruments an AudioContext with inspector methods.
 * TODO refactor with CallWatcherActor, bug 986704
 */
let WebAudioInstrumenter = {
  /**
   * Overrides all AudioContext methods.
   *
   * @param nsIDOMWindow window
   *        The window to perform the instrumentation in.
   * @param WebAudioObserver observer
   *        The observer watching function calls in the context.
   */
  handle: function(window, observer) {
    let self = this;

    let AudioContext = unwrap(window.AudioContext);
    let AudioNode = unwrap(window.AudioNode);
    let ctxProto = AudioContext.prototype;
    let nodeProto = AudioNode.prototype;

    // All Web Audio nodes inherit from AudioNode's prototype, so
    // hook into the `connect` and `disconnect` methods

    // audionode.connect(node|param);
    let originalConnect = nodeProto.connect;
    nodeProto.connect = function (...args) {
      let source = unwrap(this);
      let nodeOrParam = unwrap(args[0]);
      originalConnect.apply(source, args);

      // Alert observer differently if connecting to an AudioNode or AudioParam
      if (nodeOrParam instanceof AudioNode)
        observer.connectNode(source, nodeOrParam);
      else
        observer.connectParam(source, nodeOrParam);
    };

    // audionode.disconnect()
    let originalDisconnect = nodeProto.disconnect;
    nodeProto.disconnect = function (...args) {
      let source = unwrap(this);
      originalDisconnect.apply(source, args);
      observer.disconnectNode(source);
    };


    // Keep track of the first node created, so we can alert
    // the front end that an audio context is being used since
    // we're not hooking into the constructor itself, just its
    // instance's methods.
    let firstNodeCreated = false;

    // Patch all of AudioContext's methods that create an audio node
    // and hook into the observer
    NODE_CREATION_METHODS.forEach(method => {
      let originalMethod = ctxProto[method];
      ctxProto[method] = function (...args) {
        let node = originalMethod.apply(this, args);
        // Fire the start-up event if this is the first node created
        // and trigger a `create-node` event for the context destination
        if (!firstNodeCreated) {
          firstNodeCreated = true;
          observer.startContext();
          observer.createNode(node.context.destination);
        }
        observer.createNode(node);
        return node;
      };
    });
  }
};

/**
 * An observer that captures an Audio Context's actions and emits
 * events
 */
function WebAudioObserver () {}

WebAudioObserver.prototype = {
  startContext: function () {
    emit(this, "start-context");
  },

  connectNode: function (source, dest) {
    emit(this, "connect-node", source, dest);
  },

  connectParam: function (source, param) {
    emit(this, "connect-param", source, param);
  },

  disconnectNode: function (source) {
    emit(this, "disconnect-node", source);
  },

  createNode: function (source) {
    emit(this, "create-node", source);
  },

  paramChange: function (node, param, val) {
    emit(this, "param-change", node, param, val);
  }
};

function unwrap (obj) {
  return XPCNativeWrapper.unwrap(obj);
}

let NODE_CREATION_METHODS = [
  "createBufferSource", "createMediaElementSource", "createMediaStreamSource",
  "createMediaStreamDestination", "createScriptProcessor", "createAnalyser",
  "createGain", "createDelay", "createBiquadFilter", "createWaveShaper",
  "createPanner", "createConvolver", "createChannelSplitter", "createChannelMerger",
  "createDynamicsCompressor", "createOscillator"
];
