/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

this.EXPORTED_SYMBOLS = ["DebuggerTransport",
                         "DebuggerClient",
                         "debuggerSocketConnect",
                         "LongStringClient",
                         "GripClient"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
const { defer, resolve, reject } = Promise;

XPCOMUtils.defineLazyServiceGetter(this, "socketTransportService",
                                   "@mozilla.org/network/socket-transport-service;1",
                                   "nsISocketTransportService");

XPCOMUtils.defineLazyModuleGetter(this, "WebConsoleClient",
                                  "resource://gre/modules/devtools/WebConsoleClient.jsm");

let wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");

function dumpn(str)
{
  if (wantLogging) {
    dump("DBG-CLIENT: " + str + "\n");
  }
}

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://global/content/devtools/dbg-transport.js", this);

/**
 * Add simple event notification to a prototype object. Any object that has
 * some use for event notifications or the observer pattern in general can be
 * augmented with the necessary facilities by passing its prototype to this
 * function.
 *
 * @param aProto object
 *        The prototype object that will be modified.
 */
function eventSource(aProto) {
  /**
   * Add a listener to the event source for a given event.
   *
   * @param aName string
   *        The event to listen for.
   * @param aListener function
   *        Called when the event is fired. If the same listener
   *        is added more than once, it will be called once per
   *        addListener call.
   */
  aProto.addListener = function EV_addListener(aName, aListener) {
    if (typeof aListener != "function") {
      throw TypeError("Listeners must be functions.");
    }

    if (!this._listeners) {
      this._listeners = {};
    }

    this._getListeners(aName).push(aListener);
  };

  /**
   * Add a listener to the event source for a given event. The
   * listener will be removed after it is called for the first time.
   *
   * @param aName string
   *        The event to listen for.
   * @param aListener function
   *        Called when the event is fired.
   */
  aProto.addOneTimeListener = function EV_addOneTimeListener(aName, aListener) {
    let self = this;

    let l = function() {
      self.removeListener(aName, l);
      aListener.apply(null, arguments);
    };
    this.addListener(aName, l);
  };

  /**
   * Remove a listener from the event source previously added with
   * addListener().
   *
   * @param aName string
   *        The event name used during addListener to add the listener.
   * @param aListener function
   *        The callback to remove. If addListener was called multiple
   *        times, all instances will be removed.
   */
  aProto.removeListener = function EV_removeListener(aName, aListener) {
    if (!this._listeners || !this._listeners[aName]) {
      return;
    }
    this._listeners[aName] =
      this._listeners[aName].filter(function(l) { return l != aListener });
  };

  /**
   * Returns the listeners for the specified event name. If none are defined it
   * initializes an empty list and returns that.
   *
   * @param aName string
   *        The event name.
   */
  aProto._getListeners = function EV_getListeners(aName) {
    if (aName in this._listeners) {
      return this._listeners[aName];
    }
    this._listeners[aName] = [];
    return this._listeners[aName];
  };

  /**
   * Notify listeners of an event.
   *
   * @param aName string
   *        The event to fire.
   * @param arguments
   *        All arguments will be passed along to the listeners,
   *        including the name argument.
   */
  aProto.notify = function EV_notify() {
    if (!this._listeners) {
      return;
    }

    let name = arguments[0];
    let listeners = this._getListeners(name).slice(0);

    for each (let listener in listeners) {
      try {
        listener.apply(null, arguments);
      } catch (e) {
        // Prevent a bad listener from interfering with the others.
        let msg = e + ": " + e.stack;
        Cu.reportError(msg);
        dumpn(msg);
      }
    }
  }
}

/**
 * Set of protocol messages that affect thread state, and the
 * state the actor is in after each message.
 */
const ThreadStateTypes = {
  "paused": "paused",
  "resumed": "attached",
  "detached": "detached"
};

/**
 * Set of protocol messages that are sent by the server without a prior request
 * by the client.
 */
const UnsolicitedNotifications = {
  "consoleAPICall": "consoleAPICall",
  "eventNotification": "eventNotification",
  "fileActivity": "fileActivity",
  "networkEvent": "networkEvent",
  "networkEventUpdate": "networkEventUpdate",
  "newGlobal": "newGlobal",
  "newScript": "newScript",
  "newSource": "newSource",
  "tabDetached": "tabDetached",
  "tabNavigated": "tabNavigated",
  "pageError": "pageError",
  "webappsEvent": "webappsEvent",
  "documentLoad": "documentLoad"
};

/**
 * Set of pause types that are sent by the server and not as an immediate
 * response to a client request.
 */
const UnsolicitedPauses = {
  "resumeLimit": "resumeLimit",
  "debuggerStatement": "debuggerStatement",
  "breakpoint": "breakpoint",
  "watchpoint": "watchpoint",
  "exception": "exception"
};

const ROOT_ACTOR_NAME = "root";

/**
 * Creates a client for the remote debugging protocol server. This client
 * provides the means to communicate with the server and exchange the messages
 * required by the protocol in a traditional JavaScript API.
 */
this.DebuggerClient = function DebuggerClient(aTransport)
{
  this._transport = aTransport;
  this._transport.hooks = this;
  this._threadClients = {};
  this._tabClients = {};
  this._consoleClients = {};

  this._pendingRequests = [];
  this._activeRequests = {};
  this._eventsEnabled = true;

  this.compat = new ProtocolCompatibility(this, [
    new SourcesShim(),
  ]);

  this.request = this.request.bind(this);
}

/**
 * A declarative helper for defining methods that send requests to the server.
 *
 * @param aPacketSkeleton
 *        The form of the packet to send. Can specify fields to be filled from
 *        the parameters by using the |args| function.
 * @param telemetry
 *        The unique suffix of the telemetry histogram id.
 * @param before
 *        The function to call before sending the packet. Is passed the packet,
 *        and the return value is used as the new packet. The |this| context is
 *        the instance of the client object we are defining a method for.
 * @param after
 *        The function to call after the response is received. It is passed the
 *        response, and the return value is considered the new response that
 *        will be passed to the callback. The |this| context is the instance of
 *        the client object we are defining a method for.
 */
DebuggerClient.requester = function DC_requester(aPacketSkeleton, { telemetry,
                                                 before, after }) {
  return function (...args) {
    let histogram, startTime;
    if (telemetry) {
      let transportType = this._transport.onOutputStreamReady === undefined
        ? "LOCAL_"
        : "REMOTE_";
      let histogramId = "DEVTOOLS_DEBUGGER_RDP_"
        + transportType + telemetry + "_MS";
      histogram = Services.telemetry.getHistogramById(histogramId);
      startTime = +new Date;
    }
    let outgoingPacket = {
      to: aPacketSkeleton.to || this.actor
    };

    let maxPosition = -1;
    for (let k of Object.keys(aPacketSkeleton)) {
      if (aPacketSkeleton[k] instanceof DebuggerClient.Argument) {
        let { position } = aPacketSkeleton[k];
        outgoingPacket[k] = aPacketSkeleton[k].getArgument(args);
        maxPosition = Math.max(position, maxPosition);
      } else {
        outgoingPacket[k] = aPacketSkeleton[k];
      }
    }

    if (before) {
      outgoingPacket = before.call(this, outgoingPacket);
    }

    this.request(outgoingPacket, function (aResponse) {
      if (after) {
        let { from } = aResponse;
        aResponse = after.call(this, aResponse);
        if (!aResponse.from) {
          aResponse.from = from;
        }
      }

      // The callback is always the last parameter.
      let thisCallback = args[maxPosition + 1];
      if (thisCallback) {
        thisCallback(aResponse);
      }

      if (histogram) {
        histogram.add(+new Date - startTime);
      }
    }.bind(this));

  };
};

function args(aPos) {
  return new DebuggerClient.Argument(aPos);
}

DebuggerClient.Argument = function DCP(aPosition) {
  this.position = aPosition;
};

DebuggerClient.Argument.prototype.getArgument = function DCP_getArgument(aParams) {
  if (!this.position in aParams) {
    throw new Error("Bad index into params: " + this.position);
  }
  return aParams[this.position];
};

DebuggerClient.prototype = {
  /**
   * Connect to the server and start exchanging protocol messages.
   *
   * @param aOnConnected function
   *        If specified, will be called when the greeting packet is
   *        received from the debugging server.
   */
  connect: function DC_connect(aOnConnected) {
    if (aOnConnected) {
      this.addOneTimeListener("connected", function(aName, aApplicationType, aTraits) {
        aOnConnected(aApplicationType, aTraits);
      });
    }

    this._transport.ready();
  },

  /**
   * Shut down communication with the debugging server.
   *
   * @param aOnClosed function
   *        If specified, will be called when the debugging connection
   *        has been closed.
   */
  close: function DC_close(aOnClosed) {
    // Disable detach event notifications, because event handlers will be in a
    // cleared scope by the time they run.
    this._eventsEnabled = false;

    if (aOnClosed) {
      this.addOneTimeListener('closed', function(aEvent) {
        aOnClosed();
      });
    }

    let self = this;

    let continuation = function () {
      self._consoleClients = {};
      detachThread();
    }

    for each (let client in this._consoleClients) {
      continuation = client.close.bind(client, continuation);
    }

    continuation();

    function detachThread() {
      if (self.activeThread) {
        self.activeThread.detach(detachTab);
      } else {
        detachTab();
      }
    }

    function detachTab() {
      if (self.activeTab) {
        self.activeTab.detach(closeTransport);
      } else {
        closeTransport();
      }
    }

    function closeTransport() {
      self._transport.close();
      self._transport = null;
    }
  },

  /**
   * List the open tabs.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listTabs: DebuggerClient.requester({
    to: ROOT_ACTOR_NAME,
    type: "listTabs"
  }, {
    telemetry: "LISTTABS"
  }),

  /**
   * Attach to a tab actor.
   *
   * @param string aTabActor
   *        The actor ID for the tab to attach.
   * @param function aOnResponse
   *        Called with the response packet and a TabClient
   *        (which will be undefined on error).
   */
  attachTab: function DC_attachTab(aTabActor, aOnResponse) {
    let self = this;
    let packet = {
      to: aTabActor,
      type: "attach"
    };
    this.request(packet, function(aResponse) {
      let tabClient;
      if (!aResponse.error) {
        tabClient = new TabClient(self, aTabActor);
        self._tabClients[aTabActor] = tabClient;
        self.activeTab = tabClient;
      }
      aOnResponse(aResponse, tabClient);
    });
  },

  /**
   * Attach to a Web Console actor.
   *
   * @param string aConsoleActor
   *        The ID for the console actor to attach to.
   * @param array aListeners
   *        The console listeners you want to start.
   * @param function aOnResponse
   *        Called with the response packet and a WebConsoleClient
   *        instance (which will be undefined on error).
   */
  attachConsole:
  function DC_attachConsole(aConsoleActor, aListeners, aOnResponse) {
    let self = this;
    let packet = {
      to: aConsoleActor,
      type: "startListeners",
      listeners: aListeners,
    };

    this.request(packet, function(aResponse) {
      let consoleClient;
      if (!aResponse.error) {
        consoleClient = new WebConsoleClient(self, aConsoleActor);
        self._consoleClients[aConsoleActor] = consoleClient;
      }
      aOnResponse(aResponse, consoleClient);
    });
  },

  /**
   * Attach to a thread actor.
   *
   * @param string aThreadActor
   *        The actor ID for the thread to attach.
   * @param function aOnResponse
   *        Called with the response packet and a ThreadClient
   *        (which will be undefined on error).
   * @param object aOptions
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   */
  attachThread: function DC_attachThread(aThreadActor, aOnResponse, aOptions={}) {
    let self = this;
    let packet = {
      to: aThreadActor,
      type: "attach",
      options: aOptions
    };
    this.request(packet, function(aResponse) {
      if (!aResponse.error) {
        var threadClient = new ThreadClient(self, aThreadActor);
        self._threadClients[aThreadActor] = threadClient;
        self.activeThread = threadClient;
      }
      aOnResponse(aResponse, threadClient);
    });
  },

  /**
   * Reconfigure a thread actor.
   *
   * @param boolean aUseSourceMaps
   *        A flag denoting whether to use source maps or not.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  reconfigureThread: function DC_reconfigureThread(aUseSourceMaps, aOnResponse) {
    let packet = {
      to: this.activeThread._actor,
      type: "reconfigure",
      options: { useSourceMaps: aUseSourceMaps }
    };
    this.request(packet, aOnResponse);
  },

  /**
   * Release an object actor.
   *
   * @param string aActor
   *        The actor ID to send the request to.
   * @param aOnResponse function
   *        If specified, will be called with the response packet when
   *        debugging server responds.
   */
  release: DebuggerClient.requester({
    to: args(0),
    type: "release"
  }, {
    telemetry: "RELEASE"
  }),

  /**
   * Send a request to the debugging server.
   *
   * @param aRequest object
   *        A JSON packet to send to the debugging server.
   * @param aOnResponse function
   *        If specified, will be called with the response packet when
   *        debugging server responds.
   */
  request: function DC_request(aRequest, aOnResponse) {
    if (!this._connected) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    if (!aRequest.to) {
      let type = aRequest.type || "";
      throw Error("'" + type + "' request packet has no destination.");
    }

    this._pendingRequests.push({ to: aRequest.to,
                                 request: aRequest,
                                 onResponse: aOnResponse });
    this._sendRequests();
  },

  /**
   * Send pending requests to any actors that don't already have an
   * active request.
   */
  _sendRequests: function DC_sendRequests() {
    let self = this;
    this._pendingRequests = this._pendingRequests.filter(function (request) {
      if (request.to in self._activeRequests) {
        return true;
      }

      self._activeRequests[request.to] = request;
      self._transport.send(request.request);

      return false;
    });
  },

  // Transport hooks.

  /**
   * Called by DebuggerTransport to dispatch incoming packets as appropriate.
   *
   * @param aPacket object
   *        The incoming packet.
   * @param aIgnoreCompatibility boolean
   *        Set true to not pass the packet through the compatibility layer.
   */
  onPacket: function DC_onPacket(aPacket, aIgnoreCompatibility=false) {
    let packet = aIgnoreCompatibility
      ? aPacket
      : this.compat.onPacket(aPacket);

    resolve(packet).then((aPacket) => {
      if (!this._connected) {
        // Hello packet.
        this._connected = true;
        this.notify("connected",
                    aPacket.applicationType,
                    aPacket.traits);
        return;
      }

      if (!aPacket.from) {
        let msg = "Server did not specify an actor, dropping packet: " +
                  JSON.stringify(aPacket);
        Cu.reportError(msg);
        dumpn(msg);
        return;
      }

      let onResponse;
      // Don't count unsolicited notifications or pauses as responses.
      if (aPacket.from in this._activeRequests &&
          !(aPacket.type in UnsolicitedNotifications) &&
          !(aPacket.type == ThreadStateTypes.paused &&
            aPacket.why.type in UnsolicitedPauses)) {
        onResponse = this._activeRequests[aPacket.from].onResponse;
        delete this._activeRequests[aPacket.from];
      }

      // Packets that indicate thread state changes get special treatment.
      if (aPacket.type in ThreadStateTypes &&
          aPacket.from in this._threadClients) {
        this._threadClients[aPacket.from]._onThreadState(aPacket);
      }
      // On navigation the server resumes, so the client must resume as well.
      // We achieve that by generating a fake resumption packet that triggers
      // the client's thread state change listeners.
      if (this.activeThread &&
          aPacket.type == UnsolicitedNotifications.tabNavigated &&
          aPacket.from in this._tabClients) {
        let resumption = { from: this.activeThread._actor, type: "resumed" };
        this.activeThread._onThreadState(resumption);
      }
      // Only try to notify listeners on events, not responses to requests
      // that lack a packet type.
      if (aPacket.type) {
        this.notify(aPacket.type, aPacket);
      }

      if (onResponse) {
        onResponse(aPacket);
      }

      this._sendRequests();
    }, function (ex) {
      dumpn("Error handling response: " + ex + " - stack:\n" + ex.stack);
      Cu.reportError(ex.message + "\n" + ex.stack);
    });
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param aStatus nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed: function DC_onClosed(aStatus) {
    this.notify("closed");
  },
}

eventSource(DebuggerClient.prototype);

// Constants returned by `FeatureCompatibilityShim.onPacketTest`.
const SUPPORTED = 1;
const NOT_SUPPORTED = 2;
const SKIP = 3;

/**
 * This object provides an abstraction layer over all of our backwards
 * compatibility, feature detection, and shimming with regards to the remote
 * debugging prototcol.
 *
 * @param aFeatures Array
 *        An array of FeatureCompatibilityShim objects
 */
function ProtocolCompatibility(aClient, aFeatures) {
  this._client = aClient;
  this._featuresWithUnknownSupport = new Set(aFeatures);
  this._featuresWithoutSupport = new Set();

  this._featureDeferreds = Object.create(null)
  for (let f of aFeatures) {
    this._featureDeferreds[f.name] = defer();
  }
}

ProtocolCompatibility.prototype = {
  /**
   * Returns a promise that resolves to true if the RDP supports the feature,
   * and is rejected otherwise.
   *
   * @param aFeatureName String
   *        The name of the feature we are testing.
   */
  supportsFeature: function PC_supportsFeature(aFeatureName) {
    return this._featureDeferreds[aFeatureName].promise;
  },

  /**
   * Force a feature to be considered unsupported.
   *
   * @param aFeatureName String
   *        The name of the feature we are testing.
   */
  rejectFeature: function PC_rejectFeature(aFeatureName) {
    this._featureDeferreds[aFeatureName].reject(false);
  },

  /**
   * Called for each packet received over the RDP from the server. Tests for
   * protocol features and shims packets to support needed features.
   *
   * @param aPacket Object
   *        Packet received over the RDP from the server.
   */
  onPacket: function PC_onPacket(aPacket) {
    this._detectFeatures(aPacket);
    return this._shimPacket(aPacket);
  },

  /**
   * For each of the features we don't know whether the server supports or not,
   * attempt to detect support based on the packet we just received.
   */
  _detectFeatures: function PC__detectFeatures(aPacket) {
    for (let feature of this._featuresWithUnknownSupport) {
      try {
        switch (feature.onPacketTest(aPacket)) {
        case SKIP:
          break;
        case SUPPORTED:
          this._featuresWithUnknownSupport.delete(feature);
          this._featureDeferreds[feature.name].resolve(true);
          break;
        case NOT_SUPPORTED:
          this._featuresWithUnknownSupport.delete(feature);
          this._featuresWithoutSupport.add(feature);
          this.rejectFeature(feature.name);
          break;
        default:
          Cu.reportError(new Error(
            "Bad return value from `onPacketTest` for feature '"
              + feature.name + "'"));
        }
      } catch (ex) {
        Cu.reportError("Error detecting support for feature '"
                       + feature.name + "':" + ex.message + "\n"
                       + ex.stack);
      }
    }
  },

  /**
   * Go through each of the features that we know are unsupported by the current
   * server and attempt to shim support.
   */
  _shimPacket: function PC__shimPacket(aPacket) {
    let extraPackets = [];

    let loop = function (aFeatures, aPacket) {
      if (aFeatures.length === 0) {
        for (let packet of extraPackets) {
          this._client.onPacket(packet, true);
        }
        return aPacket;
      } else {
        let replacePacket = function (aNewPacket) {
          return aNewPacket;
        };
        let extraPacket = function (aExtraPacket) {
          extraPackets.push(aExtraPacket);
          return aPacket;
        };
        let keepPacket = function () {
          return aPacket;
        };
        let newPacket = aFeatures[0].translatePacket(aPacket,
                                                     replacePacket,
                                                     extraPacket,
                                                     keepPacket);
        return resolve(newPacket).then(loop.bind(null, aFeatures.slice(1)));
      }
    }.bind(this);

    return loop([f for (f of this._featuresWithoutSupport)],
                aPacket);
  }
};

/**
 * Interface defining what methods a feature compatibility shim should have.
 */
const FeatureCompatibilityShim = {
  // The name of the feature
  name: null,

  /**
   * Takes a packet and returns boolean (or promise of boolean) indicating
   * whether the server supports the RDP feature we are possibly shimming.
   */
  onPacketTest: function (aPacket) {
    throw new Error("Not yet implemented");
  },

  /**
   * Takes a packet actually sent from the server and decides whether to replace
   * it with a new packet, create an extra packet, or keep it.
   */
  translatePacket: function (aPacket, aReplacePacket, aExtraPacket, aKeepPacket) {
    throw new Error("Not yet implemented");
  }
};

/**
 * A shim to support the "sources" and "newSource" packets for older servers
 * which don't support them.
 */
function SourcesShim() {
  this._sourcesSeen = new Set();
}

SourcesShim.prototype = Object.create(FeatureCompatibilityShim);
let SSProto = SourcesShim.prototype;

SSProto.name = "sources";

SSProto.onPacketTest = function SS_onPacketTest(aPacket) {
  if (aPacket.traits) {
    return aPacket.traits.sources
      ? SUPPORTED
      : NOT_SUPPORTED;
  }
  return SKIP;
};

SSProto.translatePacket = function SS_translatePacket(aPacket,
                                                      aReplacePacket,
                                                      aExtraPacket,
                                                      aKeepPacket) {
  if (aPacket.type !== "newScript" || this._sourcesSeen.has(aPacket.url)) {
    return aKeepPacket();
  }
  this._sourcesSeen.add(aPacket.url);
  return aExtraPacket({
    from: aPacket.from,
    type: "newSource",
    source: aPacket.source
  });
};

/**
 * Creates a tab client for the remote debugging protocol server. This client
 * is a front to the tab actor created in the server side, hiding the protocol
 * details in a traditional JavaScript API.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aActor string
 *        The actor ID for this tab.
 */
function TabClient(aClient, aActor) {
  this._client = aClient;
  this._actor = aActor;
  this.request = this._client.request;
}

TabClient.prototype = {
  get actor() { return this._actor },
  get _transport() { return this._client._transport; },

  /**
   * Detach the client from the tab actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (aResponse) {
      if (this.activeTab === this._client._tabClients[this.actor]) {
        delete this.activeTab;
      }
      delete this._client._tabClients[this.actor];
      return aResponse;
    },
    telemetry: "TABDETACH"
  }),
};

eventSource(TabClient.prototype);

/**
 * Creates a thread client for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aActor string
 *        The actor ID for this thread.
 */
function ThreadClient(aClient, aActor) {
  this._client = aClient;
  this._actor = aActor;
  this._frameCache = [];
  this._scriptCache = {};
  this._pauseGrips = {};
  this._threadGrips = {};
  this.request = this._client.request;
}

ThreadClient.prototype = {
  _state: "paused",
  get state() { return this._state; },
  get paused() { return this._state === "paused"; },

  _pauseOnExceptions: false,

  _actor: null,
  get actor() { return this._actor; },

  get compat() { return this._client.compat; },
  get _transport() { return this._client._transport; },

  _assertPaused: function TC_assertPaused(aCommand) {
    if (!this.paused) {
      throw Error(aCommand + " command sent while not paused.");
    }
  },

  /**
   * Resume a paused thread. If the optional aLimit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param [optional] object aLimit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   *        Use null to specify no limit.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  _doResume: DebuggerClient.requester({
    type: "resume",
    resumeLimit: args(0)
  }, {
    before: function (aPacket) {
      this._assertPaused("resume");

      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._state = "resuming";

      aPacket.pauseOnExceptions = this._pauseOnExceptions;
      return aPacket;
    },
    after: function (aResponse) {
      if (aResponse.error) {
        // There was an error resuming, back to paused state.
        this._state = "paused";
      }
      return aResponse;
    },
    telemetry: "RESUME"
  }),

  /**
   * Resume a paused thread.
   */
  resume: function TC_resume(aOnResponse) {
    this._doResume(null, aOnResponse);
  },

  /**
   * Step over a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOver: function TC_stepOver(aOnResponse) {
    this._doResume({ type: "next" }, aOnResponse);
  },

  /**
   * Step into a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepIn: function TC_stepIn(aOnResponse) {
    this._doResume({ type: "step" }, aOnResponse);
  },

  /**
   * Step out of a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOut: function TC_stepOut(aOnResponse) {
    this._doResume({ type: "finish" }, aOnResponse);
  },

  /**
   * Interrupt a running thread.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  interrupt: DebuggerClient.requester({
    type: "interrupt"
  }, {
    telemetry: "INTERRUPT"
  }),

  /**
   * Enable or disable pausing when an exception is thrown.
   *
   * @param boolean aFlag
   *        Enables pausing if true, disables otherwise.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  pauseOnExceptions: function TC_pauseOnExceptions(aFlag, aOnResponse) {
    this._pauseOnExceptions = aFlag;
    // If the debuggee is paused, the value of the flag will be communicated in
    // the next resumption. Otherwise we have to force a pause in order to send
    // the flag.
    if (!this.paused) {
      this.interrupt(function(aResponse) {
        if (aResponse.error) {
          // Can't continue if pausing failed.
          aOnResponse(aResponse);
          return;
        }
        this.resume(aOnResponse);
      }.bind(this));
    }
  },

  /**
   * Send a clientEvaluate packet to the debuggee. Response
   * will be a resume packet.
   *
   * @param string aFrame
   *        The actor ID of the frame where the evaluation should take place.
   * @param string aExpression
   *        The expression that will be evaluated in the scope of the frame
   *        above.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  eval: DebuggerClient.requester({
    type: "clientEvaluate",
    frame: args(0),
    expression: args(1)
  }, {
    before: function (aPacket) {
      this._assertPaused("eval");
      // Put the client in a tentative "resuming" state so we can prevent
      // further requests that should only be sent in the paused state.
      this._state = "resuming";
      return aPacket;
    },
    after: function (aResponse) {
      if (aResponse.error) {
        // There was an error resuming, back to paused state.
        self._state = "paused";
      }
      return aResponse;
    },
    telemetry: "CLIENTEVALUATE"
  }),

  /**
   * Detach from the thread actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (aResponse) {
      if (this.activeThread === this._client._threadClients[this.actor]) {
        delete this.activeThread;
      }
      delete this._client._threadClients[this.actor];
      return aResponse;
    },
    telemetry: "THREADDETACH"
  }),

  /**
   * Request to set a breakpoint in the specified location.
   *
   * @param object aLocation
   *        The source location object where the breakpoint will be set.
   * @param function aOnResponse
   *        Called with the thread's response.
   */
  setBreakpoint: function TC_setBreakpoint(aLocation, aOnResponse) {
    // A helper function that sets the breakpoint.
    let doSetBreakpoint = function _doSetBreakpoint(aCallback) {
      let packet = { to: this._actor, type: "setBreakpoint",
                     location: aLocation };
      this._client.request(packet, function (aResponse) {
        // Ignoring errors, since the user may be setting a breakpoint in a
        // dead script that will reappear on a page reload.
        if (aOnResponse) {
          let bpClient = new BreakpointClient(this._client, aResponse.actor,
                                              aLocation);
          if (aCallback) {
            aCallback(aOnResponse(aResponse, bpClient));
          } else {
            aOnResponse(aResponse, bpClient);
          }
        }
      }.bind(this));
    }.bind(this);

    // If the debuggee is paused, just set the breakpoint.
    if (this.paused) {
      doSetBreakpoint();
      return;
    }
    // Otherwise, force a pause in order to set the breakpoint.
    this.interrupt(function(aResponse) {
      if (aResponse.error) {
        // Can't set the breakpoint if pausing failed.
        aOnResponse(aResponse);
        return;
      }
      doSetBreakpoint(this.resume.bind(this));
    }.bind(this));
  },

  /**
   * Release multiple thread-lifetime object actors. If any pause-lifetime
   * actors are included in the request, a |notReleasable| error will return,
   * but all the thread-lifetime ones will have been released.
   *
   * @param array actors
   *        An array with actor IDs to release.
   */
  releaseMany: DebuggerClient.requester({
    type: "releaseMany",
    actors: args(0),
  }, {
    telemetry: "RELEASEMANY"
  }),

  /**
   * Promote multiple pause-lifetime object actors to thread-lifetime ones.
   *
   * @param array actors
   *        An array with actor IDs to promote.
   */
  threadGrips: DebuggerClient.requester({
    type: "threadGrips",
    actors: args(0)
  }, {
    telemetry: "THREADGRIPS"
  }),

  /**
   * Request the loaded sources for the current thread.
   *
   * @param aOnResponse Function
   *        Called with the thread's response.
   */
  getSources: function TC_getSources(aOnResponse) {
    // This is how we should get sources if the server supports "sources"
    // requests.
    let getSources = DebuggerClient.requester({
      type: "sources"
    }, {
      telemetry: "SOURCES"
    });

    // This is how we should deduct what sources exist from the existing scripts
    // when the server does not support "sources" requests.
    let getSourcesBackwardsCompat = DebuggerClient.requester({
      type: "scripts"
    }, {
      after: function (aResponse) {
        if (aResponse.error) {
          return aResponse;
        }

        let sourceActorsByURL = aResponse.scripts
          .reduce(function (aSourceActorsByURL, aScript) {
            aSourceActorsByURL[aScript.url] = aScript.source;
            return aSourceActorsByURL;
          }, {});

        return {
          sources: [
            { url: url, actor: sourceActorsByURL[url] }
            for (url of Object.keys(sourceActorsByURL))
          ]
        }
      },
      telemetry: "SOURCES"
    });

    // On the first time `getSources` is called, patch the thread client with
    // the best method for the server's capabilities.
    let threadClient = this;
    this.compat.supportsFeature("sources").then(function () {
      threadClient.getSources = getSources;
    }, function () {
      threadClient.getSources = getSourcesBackwardsCompat;
    }).then(function () {
      threadClient.getSources(aOnResponse);
    });
  },

  _doInterrupted: function TC_doInterrupted(aAction, aError) {
    if (this.paused) {
      aAction();
      return;
    }
    this.interrupt(function(aResponse) {
      if (aResponse) {
        aError(aResponse);
        return;
      }
      aAction();
      this.resume();
    }.bind(this));
  },

  /**
   * Clear the thread's source script cache. A scriptscleared event
   * will be sent.
   */
  _clearScripts: function TC_clearScripts() {
    if (Object.keys(this._scriptCache).length > 0) {
      this._scriptCache = {}
      this.notify("scriptscleared");
    }
  },

  /**
   * Request frames from the callstack for the current thread.
   *
   * @param aStart integer
   *        The number of the youngest stack frame to return (the youngest
   *        frame is 0).
   * @param aCount integer
   *        The maximum number of frames to return, or null to return all
   *        frames.
   * @param aOnResponse function
   *        Called with the thread's response.
   */
  getFrames: DebuggerClient.requester({
    type: "frames",
    start: args(0),
    count: args(1)
  }, {
    telemetry: "FRAMES"
  }),

  /**
   * An array of cached frames. Clients can observe the framesadded and
   * framescleared event to keep up to date on changes to this cache,
   * and can fill it using the fillFrames method.
   */
  get cachedFrames() { return this._frameCache; },

  /**
   * true if there are more stack frames available on the server.
   */
  get moreFrames() {
    return this.paused && (!this._frameCache || this._frameCache.length == 0
          || !this._frameCache[this._frameCache.length - 1].oldest);
  },

  /**
   * Ensure that at least aTotal stack frames have been loaded in the
   * ThreadClient's stack frame cache. A framesadded event will be
   * sent when the stack frame cache is updated.
   *
   * @param aTotal number
   *        The minimum number of stack frames to be included.
   *
   * @returns true if a framesadded notification should be expected.
   */
  fillFrames: function TC_fillFrames(aTotal) {
    this._assertPaused("fillFrames");

    if (this._frameCache.length >= aTotal) {
      return false;
    }

    let numFrames = this._frameCache.length;

    let self = this;
    this.getFrames(numFrames, aTotal - numFrames, function(aResponse) {
      for each (let frame in aResponse.frames) {
        self._frameCache[frame.depth] = frame;
      }
      // If we got as many frames as we asked for, there might be more
      // frames available.

      self.notify("framesadded");
    });
    return true;
  },

  /**
   * Clear the thread's stack frame cache. A framescleared event
   * will be sent.
   */
  _clearFrames: function TC_clearFrames() {
    if (this._frameCache.length > 0) {
      this._frameCache = [];
      this.notify("framescleared");
    }
  },

  /**
   * Return a GripClient object for the given object grip.
   *
   * @param aGrip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip: function TC_pauseGrip(aGrip) {
    if (aGrip.actor in this._pauseGrips) {
      return this._pauseGrips[aGrip.actor];
    }

    let client = new GripClient(this._client, aGrip);
    this._pauseGrips[aGrip.actor] = client;
    return client;
  },

  /**
   * Get or create a long string client, checking the grip client cache if it
   * already exists.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   * @param aGripCacheName String
   *        The property name of the grip client cache to check for existing
   *        clients in.
   */
  _longString: function TC__longString(aGrip, aGripCacheName) {
    if (aGrip.actor in this[aGripCacheName]) {
      return this[aGripCacheName][aGrip.actor];
    }

    let client = new LongStringClient(this._client, aGrip);
    this[aGripCacheName][aGrip.actor] = client;
    return client;
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the current pause.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   */
  pauseLongString: function TC_pauseLongString(aGrip) {
    return this._longString(aGrip, "_pauseGrips");
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the thread lifetime.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   */
  threadLongString: function TC_threadLongString(aGrip) {
    return this._longString(aGrip, "_threadGrips");
  },

  /**
   * Clear and invalidate all the grip clients from the given cache.
   *
   * @param aGripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearGripClients: function TC_clearGrips(aGripCacheName) {
    for each (let grip in this[aGripCacheName]) {
      grip.valid = false;
    }
    this[aGripCacheName] = {};
  },

  /**
   * Invalidate pause-lifetime grip clients and clear the list of
   * current grip clients.
   */
  _clearPauseGrips: function TC_clearPauseGrips() {
    this._clearGripClients("_pauseGrips");
  },

  /**
   * Invalidate pause-lifetime grip clients and clear the list of
   * current grip clients.
   */
  _clearThreadGrips: function TC_clearPauseGrips() {
    this._clearGripClients("_threadGrips");
  },

  /**
   * Handle thread state change by doing necessary cleanup and notifying all
   * registered listeners.
   */
  _onThreadState: function TC_onThreadState(aPacket) {
    this._state = ThreadStateTypes[aPacket.type];
    this._clearFrames();
    this._clearPauseGrips();
    aPacket.type === ThreadStateTypes.detached && this._clearThreadGrips();
    this._client._eventsEnabled && this.notify(aPacket.type, aPacket);
  },

  /**
   * Return an instance of SourceClient for the given source actor form.
   */
  source: function TC_source(aForm) {
    return new SourceClient(this._client, aForm);
  }

};

eventSource(ThreadClient.prototype);

/**
 * Grip clients are used to retrieve information about the relevant object.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aGrip object
 *        A pause-lifetime object grip returned by the protocol.
 */
function GripClient(aClient, aGrip)
{
  this._grip = aGrip;
  this._client = aClient;
  this.request = this._client.request;
}

GripClient.prototype = {
  get actor() { return this._grip.actor },
  get _transport() { return this._client._transport; },

  valid: true,

  /**
   * Request the names of a function's formal parameters.
   *
   * @param aOnResponse function
   *        Called with an object of the form:
   *        { parameterNames:[<parameterName>, ...] }
   *        where each <parameterName> is the name of a parameter.
   */
  getParameterNames: DebuggerClient.requester({
    type: "parameterNames"
  }, {
    before: function (aPacket) {
      if (this._grip["class"] !== "Function") {
        throw new Error("getParameterNames is only valid for function grips.");
      }
      return aPacket;
    },
    telemetry: "PARAMETERNAMES"
  }),

  /**
   * Request the names of the properties defined on the object and not its
   * prototype.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getOwnPropertyNames: DebuggerClient.requester({
    type: "ownPropertyNames"
  }, {
    telemetry: "OWNPROPERTYNAMES"
  }),

  /**
   * Request the prototype and own properties of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getPrototypeAndProperties: DebuggerClient.requester({
    type: "prototypeAndProperties"
  }, {
    telemetry: "PROTOTYPEANDPROPERTIES"
  }),

  /**
   * Request the property descriptor of the object's specified property.
   *
   * @param aName string The name of the requested property.
   * @param aOnResponse function Called with the request's response.
   */
  getProperty: DebuggerClient.requester({
    type: "property",
    name: args(0)
  }, {
    telemetry: "PROPERTY"
  }),

  /**
   * Request the prototype of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getPrototype: DebuggerClient.requester({
    type: "prototype"
  }, {
    telemetry: "PROTOTYPE"
  }),
};

/**
 * A LongStringClient provides a way to access "very long" strings from the
 * debugger server.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aGrip Object
 *        A pause-lifetime long string grip returned by the protocol.
 */
function LongStringClient(aClient, aGrip) {
  this._grip = aGrip;
  this._client = aClient;
  this.request = this._client.request;
}

LongStringClient.prototype = {
  get actor() { return this._grip.actor; },
  get length() { return this._grip.length; },
  get initial() { return this._grip.initial; },
  get _transport() { return this._client._transport; },

  valid: true,

  /**
   * Get the substring of this LongString from aStart to aEnd.
   *
   * @param aStart Number
   *        The starting index.
   * @param aEnd Number
   *        The ending index.
   * @param aCallback Function
   *        The function called when we receive the substring.
   */
  substring: DebuggerClient.requester({
    type: "substring",
    start: args(0),
    end: args(1)
  }, {
    telemetry: "SUBSTRING"
  }),
};

/**
 * A SourceClient provides a way to access the source text of a script.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aForm Object
 *        The form sent across the remote debugging protocol.
 */
function SourceClient(aClient, aForm) {
  this._form = aForm;
  this._client = aClient;
}

SourceClient.prototype = {
  get _transport() { return this._client._transport; },

  /**
   * Get a long string grip for this SourceClient's source.
   */
  source: function SC_source(aCallback) {
    let packet = {
      to: this._form.actor,
      type: "source"
    };
    this._client.request(packet, function (aResponse) {
      if (aResponse.error) {
        aCallback(aResponse);
        return;
      }

      if (typeof aResponse.source === "string") {
        aCallback(aResponse);
        return;
      }

      let longString = this._client.activeThread.threadLongString(
        aResponse.source);
      longString.substring(0, longString.length, function (aResponse) {
        if (aResponse.error) {
          aCallback(aResponse);
          return;
        }

        aCallback({
          source: aResponse.substring
        });
      });
    }.bind(this));
  }
};

/**
 * Breakpoint clients are used to remove breakpoints that are no longer used.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aActor string
 *        The actor ID for this breakpoint.
 * @param aLocation object
 *        The location of the breakpoint. This is an object with two properties:
 *        url and line.
 */
function BreakpointClient(aClient, aActor, aLocation) {
  this._client = aClient;
  this._actor = aActor;
  this.location = aLocation;
  this.request = this._client.request;
}

BreakpointClient.prototype = {

  _actor: null,
  get actor() { return this._actor; },
  get _transport() { return this._client._transport; },

  /**
   * Remove the breakpoint from the server.
   */
  remove: DebuggerClient.requester({
    type: "delete"
  }, {
    telemetry: "DELETE"
  }),
};

eventSource(BreakpointClient.prototype);

/**
 * Connects to a debugger server socket and returns a DebuggerTransport.
 *
 * @param aHost string
 *        The host name or IP address of the debugger server.
 * @param aPort number
 *        The port number of the debugger server.
 */
this.debuggerSocketConnect = function debuggerSocketConnect(aHost, aPort)
{
  let s = socketTransportService.createTransport(null, 0, aHost, aPort, null);
  let transport = new DebuggerTransport(s.openInputStream(0, 0, 0),
                                        s.openOutputStream(0, 0, 0));
  return transport;
}

/**
 * Takes a pair of items and returns them as an array.
 */
function pair(aItemOne, aItemTwo) {
  return [aItemOne, aItemTwo];
}
