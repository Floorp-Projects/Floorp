/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;
var Cr = Components.results;
var CC = Components.Constructor;
// On B2G scope object misbehaves and we have to bind globals to `this`
// in order to ensure theses variable to be visible in transport.js
this.Ci = Ci;
this.Cc = Cc;
this.Cu = Cu;
this.Cr = Cr;
this.CC = CC;

this.EXPORTED_SYMBOLS = ["DebuggerTransport",
                         "DebuggerClient",
                         "RootClient",
                         "debuggerSocketConnect",
                         "LongStringClient",
                         "EnvironmentClient",
                         "ObjectClient"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let promise = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js").Promise;
const { defer, resolve, reject } = promise;

XPCOMUtils.defineLazyServiceGetter(this, "socketTransportService",
                                   "@mozilla.org/network/socket-transport-service;1",
                                   "nsISocketTransportService");

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://gre/modules/devtools/Loader.jsm");

XPCOMUtils.defineLazyGetter(this, "events", () => {
  return devtools.require("sdk/event/core");
});

Object.defineProperty(this, "WebConsoleClient", {
  get: function () {
    return devtools.require("devtools/toolkit/webconsole/client").WebConsoleClient;
  },
  configurable: true,
  enumerable: true
});

Components.utils.import("resource://gre/modules/devtools/DevToolsUtils.jsm");
this.makeInfallible = DevToolsUtils.makeInfallible;

let LOG_PREF = "devtools.debugger.log";
let VERBOSE_PREF = "devtools.debugger.log.verbose";
let wantLogging = Services.prefs.getBoolPref(LOG_PREF);
let wantVerbose =
  Services.prefs.getPrefType(VERBOSE_PREF) !== Services.prefs.PREF_INVALID &&
  Services.prefs.getBoolPref(VERBOSE_PREF);

const noop = () => {};

function dumpn(str) {
  if (wantLogging) {
    dump("DBG-CLIENT: " + str + "\n");
  }
}

function dumpv(msg) {
  if (wantVerbose) {
    dumpn(msg);
  }
}

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("resource://gre/modules/devtools/transport/transport.js", this);

/**
 * TODO: Get rid of this API in favor of EventTarget (bug 1042642)
 *
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
  aProto.addListener = function (aName, aListener) {
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
  aProto.addOneTimeListener = function (aName, aListener) {
    let l = (...args) => {
      this.removeListener(aName, l);
      aListener.apply(null, args);
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
  aProto.removeListener = function (aName, aListener) {
    if (!this._listeners || !this._listeners[aName]) {
      return;
    }
    this._listeners[aName] =
      this._listeners[aName].filter(function (l) { return l != aListener });
  };

  /**
   * Returns the listeners for the specified event name. If none are defined it
   * initializes an empty list and returns that.
   *
   * @param aName string
   *        The event name.
   */
  aProto._getListeners = function (aName) {
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
  aProto.emit = function () {
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
        DevToolsUtils.reportException("notify event '" + name + "'", e);
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
  "lastPrivateContextExited": "lastPrivateContextExited",
  "logMessage": "logMessage",
  "networkEvent": "networkEvent",
  "networkEventUpdate": "networkEventUpdate",
  "newGlobal": "newGlobal",
  "newScript": "newScript",
  "newSource": "newSource",
  "tabDetached": "tabDetached",
  "tabListChanged": "tabListChanged",
  "reflowActivity": "reflowActivity",
  "addonListChanged": "addonListChanged",
  "tabNavigated": "tabNavigated",
  "frameUpdate": "frameUpdate",
  "pageError": "pageError",
  "documentLoad": "documentLoad",
  "enteredFrame": "enteredFrame",
  "exitedFrame": "exitedFrame",
  "appOpen": "appOpen",
  "appClose": "appClose",
  "appInstall": "appInstall",
  "appUninstall": "appUninstall"
};

/**
 * Set of pause types that are sent by the server and not as an immediate
 * response to a client request.
 */
const UnsolicitedPauses = {
  "resumeLimit": "resumeLimit",
  "debuggerStatement": "debuggerStatement",
  "breakpoint": "breakpoint",
  "DOMEvent": "DOMEvent",
  "watchpoint": "watchpoint",
  "exception": "exception"
};

/**
 * Creates a client for the remote debugging protocol server. This client
 * provides the means to communicate with the server and exchange the messages
 * required by the protocol in a traditional JavaScript API.
 */
this.DebuggerClient = function (aTransport)
{
  this._transport = aTransport;
  this._transport.hooks = this;

  // Map actor ID to client instance for each actor type.
  this._clients = new Map;

  this._pendingRequests = [];
  this._activeRequests = new Map;
  this._eventsEnabled = true;

  this.compat = new ProtocolCompatibility(this, []);
  this.traits = {};

  this.request = this.request.bind(this);
  this.localTransport = this._transport.onOutputStreamReady === undefined;

  /*
   * As the first thing on the connection, expect a greeting packet from
   * the connection's root actor.
   */
  this.mainRoot = null;
  this.expectReply("root", (aPacket) => {
    this.mainRoot = new RootClient(this, aPacket);
    this.emit("connected", aPacket.applicationType, aPacket.traits);
  });
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
DebuggerClient.requester = function (aPacketSkeleton,
                                     { telemetry, before, after }) {
  return DevToolsUtils.makeInfallible(function (...args) {
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

    this.request(outgoingPacket, DevToolsUtils.makeInfallible((aResponse) => {
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
    }, "DebuggerClient.requester request callback"));

  }, "DebuggerClient.requester");
};

function args(aPos) {
  return new DebuggerClient.Argument(aPos);
}

DebuggerClient.Argument = function (aPosition) {
  this.position = aPosition;
};

DebuggerClient.Argument.prototype.getArgument = function (aParams) {
  if (!(this.position in aParams)) {
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
  connect: function (aOnConnected) {
    this.addOneTimeListener("connected", (aName, aApplicationType, aTraits) => {
      this.traits = aTraits;
      if (aOnConnected) {
        aOnConnected(aApplicationType, aTraits);
      }
    });

    this._transport.ready();
  },

  /**
   * Shut down communication with the debugging server.
   *
   * @param aOnClosed function
   *        If specified, will be called when the debugging connection
   *        has been closed.
   */
  close: function (aOnClosed) {
    // Disable detach event notifications, because event handlers will be in a
    // cleared scope by the time they run.
    this._eventsEnabled = false;

    if (aOnClosed) {
      this.addOneTimeListener('closed', function (aEvent) {
        aOnClosed();
      });
    }

    // Call each client's `detach` method by calling
    // lastly registered ones first to give a chance
    // to detach child clients first.
    let clients = [...this._clients.values()];
    this._clients.clear();
    const detachClients = () => {
      let client = clients.pop();
      if (!client) {
        // All clients detached.
        this._transport.close();
        this._transport = null;
        return;
      }
      if (client.detach) {
        client.detach(detachClients);
        return;
      }
      detachClients();
    };
    detachClients();
  },

  /*
   * This function exists only to preserve DebuggerClient's interface;
   * new code should say 'client.mainRoot.listTabs()'.
   */
  listTabs: function (aOnResponse) { return this.mainRoot.listTabs(aOnResponse); },

  /*
   * This function exists only to preserve DebuggerClient's interface;
   * new code should say 'client.mainRoot.listAddons()'.
   */
  listAddons: function (aOnResponse) { return this.mainRoot.listAddons(aOnResponse); },

  /**
   * Attach to a tab actor.
   *
   * @param string aTabActor
   *        The actor ID for the tab to attach.
   * @param function aOnResponse
   *        Called with the response packet and a TabClient
   *        (which will be undefined on error).
   */
  attachTab: function (aTabActor, aOnResponse = noop) {
    if (this._clients.has(aTabActor)) {
      let cachedTab = this._clients.get(aTabActor);
      let cachedResponse = {
        cacheDisabled: cachedTab.cacheDisabled,
        javascriptEnabled: cachedTab.javascriptEnabled,
        traits: cachedTab.traits,
      };
      DevToolsUtils.executeSoon(() => aOnResponse(cachedResponse, cachedTab));
      return;
    }

    let packet = {
      to: aTabActor,
      type: "attach"
    };
    this.request(packet, (aResponse) => {
      let tabClient;
      if (!aResponse.error) {
        tabClient = new TabClient(this, aResponse);
        this.registerClient(tabClient);
      }
      aOnResponse(aResponse, tabClient);
    });
  },

  /**
   * Attach to an addon actor.
   *
   * @param string aAddonActor
   *        The actor ID for the addon to attach.
   * @param function aOnResponse
   *        Called with the response packet and a AddonClient
   *        (which will be undefined on error).
   */
  attachAddon: function DC_attachAddon(aAddonActor, aOnResponse = noop) {
    let packet = {
      to: aAddonActor,
      type: "attach"
    };
    this.request(packet, aResponse => {
      let addonClient;
      if (!aResponse.error) {
        addonClient = new AddonClient(this, aAddonActor);
        this.registerClient(addonClient);
        this.activeAddon = addonClient;
      }
      aOnResponse(aResponse, addonClient);
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
  function (aConsoleActor, aListeners, aOnResponse = noop) {
    let packet = {
      to: aConsoleActor,
      type: "startListeners",
      listeners: aListeners,
    };

    this.request(packet, (aResponse) => {
      let consoleClient;
      if (!aResponse.error) {
        if (this._clients.has(aConsoleActor)) {
          consoleClient = this._clients.get(aConsoleActor);
        } else {
          consoleClient = new WebConsoleClient(this, aResponse);
          this.registerClient(consoleClient);
        }
      }
      aOnResponse(aResponse, consoleClient);
    });
  },

  /**
   * Attach to a global-scoped thread actor for chrome debugging.
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
  attachThread: function (aThreadActor, aOnResponse = noop, aOptions={}) {
    if (this._clients.has(aThreadActor)) {
      DevToolsUtils.executeSoon(() => aOnResponse({}, this._clients.get(aThreadActor)));
      return;
    }

   let packet = {
      to: aThreadActor,
      type: "attach",
      options: aOptions
    };
    this.request(packet, (aResponse) => {
      if (!aResponse.error) {
        var threadClient = new ThreadClient(this, aThreadActor);
        this.registerClient(threadClient);
      }
      aOnResponse(aResponse, threadClient);
    });
  },

  /**
   * Attach to a trace actor.
   *
   * @param string aTraceActor
   *        The actor ID for the tracer to attach.
   * @param function aOnResponse
   *        Called with the response packet and a TraceClient
   *        (which will be undefined on error).
   */
  attachTracer: function (aTraceActor, aOnResponse = noop) {
    if (this._clients.has(aTraceActor)) {
      DevToolsUtils.executeSoon(() => aOnResponse({}, this._clients.get(aTraceActor)));
      return;
    }

    let packet = {
      to: aTraceActor,
      type: "attach"
    };
    this.request(packet, (aResponse) => {
      if (!aResponse.error) {
        var traceClient = new TraceClient(this, aTraceActor);
        this.registerClient(traceClient);
      }
      aOnResponse(aResponse, traceClient);
    });
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
   *        If specified, will be called with the JSON response packet when
   *        debugging server responds.
   * @return Request
   *         This object emits a number of events to allow you to respond to
   *         different parts of the request lifecycle.
   *         It is also a Promise object, with a `then` method, that is resolved
   *         whenever a JSON or a Bulk response is received; and is rejected
   *         if the response is an error.
   *         Note: This return value can be ignored if you are using JSON alone,
   *         because the callback provided in |aOnResponse| will be bound to the
   *         "json-reply" event automatically.
   *
   *         Events emitted:
   *         * json-reply: The server replied with a JSON packet, which is
   *           passed as event data.
   *         * bulk-reply: The server replied with bulk data, which you can read
   *           using the event data object containing:
   *           * actor:  Name of actor that received the packet
   *           * type:   Name of actor's method that was called on receipt
   *           * length: Size of the data to be read
   *           * stream: This input stream should only be used directly if you
   *                     can ensure that you will read exactly |length| bytes
   *                     and will not close the stream when reading is complete
   *           * done:   If you use the stream directly (instead of |copyTo|
   *                     below), you must signal completion by resolving /
   *                     rejecting this deferred.  If it's rejected, the
   *                     transport will be closed.  If an Error is supplied as a
   *                     rejection value, it will be logged via |dumpn|.  If you
   *                     do use |copyTo|, resolving is taken care of for you
   *                     when copying completes.
   *           * copyTo: A helper function for getting your data out of the
   *                     stream that meets the stream handling requirements
   *                     above, and has the following signature:
   *             @param  output nsIAsyncOutputStream
   *                     The stream to copy to.
   *             @return Promise
   *                     The promise is resolved when copying completes or
   *                     rejected if any (unexpected) errors occur.
   *                     This object also emits "progress" events for each chunk
   *                     that is copied.  See stream-utils.js.
   */
  request: function (aRequest, aOnResponse) {
    if (!this.mainRoot) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    if (!aRequest.to) {
      let type = aRequest.type || "";
      throw Error("'" + type + "' request packet has no destination.");
    }

    let request = new Request(aRequest);
    request.format = "json";
    if (aOnResponse) {
      request.on("json-reply", aOnResponse);
    }

    this._pendingRequests.push(request);
    this._sendRequests();

    // Implement a Promise like API on the returned object
    // that resolves/rejects on request response
    let deferred = promise.defer();
    function listenerJson(resp) {
      request.off("json-reply", listenerJson);
      request.off("bulk-reply", listenerBulk);
      if (resp.error) {
        deferred.reject(resp);
      } else {
        deferred.resolve(resp);
      }
    }
    function listenerBulk(resp) {
      request.off("json-reply", listenerJson);
      request.off("bulk-reply", listenerBulk);
      deferred.resolve(resp);
    }
    request.on("json-reply", listenerJson);
    request.on("bulk-reply", listenerBulk);
    request.then = deferred.promise.then.bind(deferred.promise);

    return request;
  },

  /**
   * Transmit streaming data via a bulk request.
   *
   * This method initiates the bulk send process by queuing up the header data.
   * The caller receives eventual access to a stream for writing.
   *
   * Since this opens up more options for how the server might respond (it could
   * send back either JSON or bulk data), and the returned Request object emits
   * events for different stages of the request process that you may want to
   * react to.
   *
   * @param request Object
   *        This is modeled after the format of JSON packets above, but does not
   *        actually contain the data, but is instead just a routing header:
   *          * actor:  Name of actor that will receive the packet
   *          * type:   Name of actor's method that should be called on receipt
   *          * length: Size of the data to be sent
   * @return Request
   *         This object emits a number of events to allow you to respond to
   *         different parts of the request lifecycle.
   *
   *         Events emitted:
   *         * bulk-send-ready: Ready to send bulk data to the server, using the
   *           event data object containing:
   *           * stream:   This output stream should only be used directly if
   *                       you can ensure that you will write exactly |length|
   *                       bytes and will not close the stream when writing is
   *                       complete
   *           * done:     If you use the stream directly (instead of |copyFrom|
   *                       below), you must signal completion by resolving /
   *                       rejecting this deferred.  If it's rejected, the
   *                       transport will be closed.  If an Error is supplied as
   *                       a rejection value, it will be logged via |dumpn|.  If
   *                       you do use |copyFrom|, resolving is taken care of for
   *                       you when copying completes.
   *           * copyFrom: A helper function for getting your data onto the
   *                       stream that meets the stream handling requirements
   *                       above, and has the following signature:
   *             @param  input nsIAsyncInputStream
   *                     The stream to copy from.
   *             @return Promise
   *                     The promise is resolved when copying completes or
   *                     rejected if any (unexpected) errors occur.
   *                     This object also emits "progress" events for each chunk
   *                     that is copied.  See stream-utils.js.
   *         * json-reply: The server replied with a JSON packet, which is
   *           passed as event data.
   *         * bulk-reply: The server replied with bulk data, which you can read
   *           using the event data object containing:
   *           * actor:  Name of actor that received the packet
   *           * type:   Name of actor's method that was called on receipt
   *           * length: Size of the data to be read
   *           * stream: This input stream should only be used directly if you
   *                     can ensure that you will read exactly |length| bytes
   *                     and will not close the stream when reading is complete
   *           * done:   If you use the stream directly (instead of |copyTo|
   *                     below), you must signal completion by resolving /
   *                     rejecting this deferred.  If it's rejected, the
   *                     transport will be closed.  If an Error is supplied as a
   *                     rejection value, it will be logged via |dumpn|.  If you
   *                     do use |copyTo|, resolving is taken care of for you
   *                     when copying completes.
   *           * copyTo: A helper function for getting your data out of the
   *                     stream that meets the stream handling requirements
   *                     above, and has the following signature:
   *             @param  output nsIAsyncOutputStream
   *                     The stream to copy to.
   *             @return Promise
   *                     The promise is resolved when copying completes or
   *                     rejected if any (unexpected) errors occur.
   *                     This object also emits "progress" events for each chunk
   *                     that is copied.  See stream-utils.js.
   */
  startBulkRequest: function(request) {
    if (!this.traits.bulk) {
      throw Error("Server doesn't support bulk transfers");
    }
    if (!this.mainRoot) {
      throw Error("Have not yet received a hello packet from the server.");
    }
    if (!request.type) {
      throw Error("Bulk packet is missing the required 'type' field.");
    }
    if (!request.actor) {
      throw Error("'" + request.type + "' bulk packet has no destination.");
    }
    if (!request.length) {
      throw Error("'" + request.type + "' bulk packet has no length.");
    }

    request = new Request(request);
    request.format = "bulk";

    this._pendingRequests.push(request);
    this._sendRequests();

    return request;
  },

  /**
   * Send pending requests to any actors that don't already have an
   * active request.
   */
  _sendRequests: function () {
    this._pendingRequests = this._pendingRequests.filter((request) => {
      let dest = request.actor;

      if (this._activeRequests.has(dest)) {
        return true;
      }

      this.expectReply(dest, request);

      if (request.format === "json") {
        this._transport.send(request.request);
        return false;
      }

      this._transport.startBulkSend(request.request).then((...args) => {
        request.emit("bulk-send-ready", ...args);
      });

      return false;
    });
  },

  /**
   * Arrange to hand the next reply from |aActor| to the handler bound to
   * |aRequest|.
   *
   * DebuggerClient.prototype.request / startBulkRequest usually takes care of
   * establishing the handler for a given request, but in rare cases (well,
   * greetings from new root actors, is the only case at the moment) we must be
   * prepared for a "reply" that doesn't correspond to any request we sent.
   */
  expectReply: function (aActor, aRequest) {
    if (this._activeRequests.has(aActor)) {
      throw Error("clashing handlers for next reply from " + uneval(aActor));
    }

    // If a handler is passed directly (as it is with the handler for the root
    // actor greeting), create a dummy request to bind this to.
    if (typeof aRequest === "function") {
      let handler = aRequest;
      aRequest = new Request();
      aRequest.on("json-reply", handler);
    }

    this._activeRequests.set(aActor, aRequest);
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
  onPacket: function (aPacket, aIgnoreCompatibility=false) {
    let packet = aIgnoreCompatibility
      ? aPacket
      : this.compat.onPacket(aPacket);

    resolve(packet).then(aPacket => {
      if (!aPacket.from) {
        DevToolsUtils.reportException(
          "onPacket",
          new Error("Server did not specify an actor, dropping packet: " +
                    JSON.stringify(aPacket)));
        return;
      }

      // If we have a registered Front for this actor, let it handle the packet
      // and skip all the rest of this unpleasantness.
      let front = this.getActor(aPacket.from);
      if (front) {
        front.onPacket(aPacket);
        return;
      }

      if (this._clients.has(aPacket.from) && aPacket.type) {
        let client = this._clients.get(aPacket.from);
        let type = aPacket.type;
        if (client.events.indexOf(type) != -1) {
          client.emit(type, aPacket);
          // we ignore the rest, as the client is expected to handle this packet.
          return;
        }
      }

      let activeRequest;
      // See if we have a handler function waiting for a reply from this
      // actor. (Don't count unsolicited notifications or pauses as
      // replies.)
      if (this._activeRequests.has(aPacket.from) &&
          !(aPacket.type in UnsolicitedNotifications) &&
          !(aPacket.type == ThreadStateTypes.paused &&
            aPacket.why.type in UnsolicitedPauses)) {
        activeRequest = this._activeRequests.get(aPacket.from);
        this._activeRequests.delete(aPacket.from);
      }

      // Packets that indicate thread state changes get special treatment.
      if (aPacket.type in ThreadStateTypes &&
          this._clients.has(aPacket.from) &&
          typeof this._clients.get(aPacket.from)._onThreadState == "function") {
        this._clients.get(aPacket.from)._onThreadState(aPacket);
      }
      // On navigation the server resumes, so the client must resume as well.
      // We achieve that by generating a fake resumption packet that triggers
      // the client's thread state change listeners.
      if (aPacket.type == UnsolicitedNotifications.tabNavigated &&
          this._clients.has(aPacket.from) &&
          this._clients.get(aPacket.from).thread) {
        let thread = this._clients.get(aPacket.from).thread;
        let resumption = { from: thread._actor, type: "resumed" };
        thread._onThreadState(resumption);
      }
      // Only try to notify listeners on events, not responses to requests
      // that lack a packet type.
      if (aPacket.type) {
        this.emit(aPacket.type, aPacket);
      }

      if (activeRequest) {
        activeRequest.emit("json-reply", aPacket);
      }

      this._sendRequests();
    }, ex => DevToolsUtils.reportException("onPacket handler", ex));
  },

  /**
   * Called by the DebuggerTransport to dispatch incoming bulk packets as
   * appropriate.
   *
   * @param packet object
   *        The incoming packet, which contains:
   *        * actor:  Name of actor that will receive the packet
   *        * type:   Name of actor's method that should be called on receipt
   *        * length: Size of the data to be read
   *        * stream: This input stream should only be used directly if you can
   *                  ensure that you will read exactly |length| bytes and will
   *                  not close the stream when reading is complete
   *        * done:   If you use the stream directly (instead of |copyTo|
   *                  below), you must signal completion by resolving /
   *                  rejecting this deferred.  If it's rejected, the transport
   *                  will be closed.  If an Error is supplied as a rejection
   *                  value, it will be logged via |dumpn|.  If you do use
   *                  |copyTo|, resolving is taken care of for you when copying
   *                  completes.
   *        * copyTo: A helper function for getting your data out of the stream
   *                  that meets the stream handling requirements above, and has
   *                  the following signature:
   *          @param  output nsIAsyncOutputStream
   *                  The stream to copy to.
   *          @return Promise
   *                  The promise is resolved when copying completes or rejected
   *                  if any (unexpected) errors occur.
   *                  This object also emits "progress" events for each chunk
   *                  that is copied.  See stream-utils.js.
   */
  onBulkPacket: function(packet) {
    let { actor, type, length } = packet;

    if (!actor) {
      DevToolsUtils.reportException(
        "onBulkPacket",
        new Error("Server did not specify an actor, dropping bulk packet: " +
                  JSON.stringify(packet)));
      return;
    }

    // See if we have a handler function waiting for a reply from this
    // actor.
    if (!this._activeRequests.has(actor)) {
      return;
    }

    let activeRequest = this._activeRequests.get(actor);
    this._activeRequests.delete(actor);
    activeRequest.emit("bulk-reply", packet);

    this._sendRequests();
  },

  /**
   * Called by DebuggerTransport when the underlying stream is closed.
   *
   * @param aStatus nsresult
   *        The status code that corresponds to the reason for closing
   *        the stream.
   */
  onClosed: function (aStatus) {
    this.emit("closed");
  },

  registerClient: function (client) {
    let actorID = client.actor;
    if (!actorID) {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with an `actor` attribute.");
    }
    if (!Array.isArray(client.events)) {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with an `events` attribute " +
                      "that is an array.");
    }
    if (client.events.length > 0 && typeof(client.emit) != "function") {
      throw new Error("DebuggerServer.registerClient expects " +
                      "a client instance with non-empty `events` array to" +
                      "have an `emit` function.");
    }
    if (this._clients.has(actorID)) {
      throw new Error("DebuggerServer.registerClient already registered " +
                      "a client for this actor.");
    }
    this._clients.set(actorID, client);
  },

  unregisterClient: function (client) {
    let actorID = client.actor;
    if (!actorID) {
      throw new Error("DebuggerServer.unregisterClient expects " +
                      "a Client instance with a `actor` attribute.");
    }
    this._clients.delete(actorID);
  },

  /**
   * Actor lifetime management, echos the server's actor pools.
   */
  __pools: null,
  get _pools() {
    if (this.__pools) {
      return this.__pools;
    }
    this.__pools = new Set();
    return this.__pools;
  },

  addActorPool: function (pool) {
    this._pools.add(pool);
  },
  removeActorPool: function (pool) {
    this._pools.delete(pool);
  },
  getActor: function (actorID) {
    let pool = this.poolFor(actorID);
    return pool ? pool.get(actorID) : null;
  },

  poolFor: function (actorID) {
    for (let pool of this._pools) {
      if (pool.has(actorID)) return pool;
    }
    return null;
  },

  /**
   * Currently attached addon.
   */
  activeAddon: null
}

eventSource(DebuggerClient.prototype);

function Request(request) {
  this.request = request;
}

Request.prototype = {

  on: function(type, listener) {
    events.on(this, type, listener);
  },

  off: function(type, listener) {
    events.off(this, type, listener);
  },

  once: function(type, listener) {
    events.once(this, type, listener);
  },

  emit: function(type, ...args) {
    events.emit(this, type, ...args);
  },

  get actor() { return this.request.to || this.request.actor; }

};

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
  supportsFeature: function (aFeatureName) {
    return this._featureDeferreds[aFeatureName].promise;
  },

  /**
   * Force a feature to be considered unsupported.
   *
   * @param aFeatureName String
   *        The name of the feature we are testing.
   */
  rejectFeature: function (aFeatureName) {
    this._featureDeferreds[aFeatureName].reject(false);
  },

  /**
   * Called for each packet received over the RDP from the server. Tests for
   * protocol features and shims packets to support needed features.
   *
   * @param aPacket Object
   *        Packet received over the RDP from the server.
   */
  onPacket: function (aPacket) {
    this._detectFeatures(aPacket);
    return this._shimPacket(aPacket);
  },

  /**
   * For each of the features we don't know whether the server supports or not,
   * attempt to detect support based on the packet we just received.
   */
  _detectFeatures: function (aPacket) {
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
          DevToolsUtils.reportException(
            "PC__detectFeatures",
            new Error("Bad return value from `onPacketTest` for feature '"
                      + feature.name + "'"));
        }
      } catch (ex) {
        DevToolsUtils.reportException("PC__detectFeatures", ex);
      }
    }
  },

  /**
   * Go through each of the features that we know are unsupported by the current
   * server and attempt to shim support.
   */
  _shimPacket: function (aPacket) {
    let extraPackets = [];

    let loop = (aFeatures, aPacket) => {
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
    };

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
 * Creates a tab client for the remote debugging protocol server. This client
 * is a front to the tab actor created in the server side, hiding the protocol
 * details in a traditional JavaScript API.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aForm object
 *        The protocol form for this tab.
 */
function TabClient(aClient, aForm) {
  this.client = aClient;
  this._actor = aForm.from;
  this._threadActor = aForm.threadActor;
  this.javascriptEnabled = aForm.javascriptEnabled;
  this.cacheDisabled = aForm.cacheDisabled;
  this.thread = null;
  this.request = this.client.request;
  this.traits = aForm.traits || {};
  this.events = [];
}

TabClient.prototype = {
  get actor() { return this._actor },
  get _transport() { return this.client._transport; },

  /**
   * Attach to a thread actor.
   *
   * @param object aOptions
   *        Configuration options.
   *        - useSourceMaps: whether to use source maps or not.
   * @param function aOnResponse
   *        Called with the response packet and a ThreadClient
   *        (which will be undefined on error).
   */
  attachThread: function(aOptions={}, aOnResponse = noop) {
    if (this.thread) {
      DevToolsUtils.executeSoon(() => aOnResponse({}, this.thread));
      return;
    }

    let packet = {
      to: this._threadActor,
      type: "attach",
      options: aOptions
    };
    this.request(packet, (aResponse) => {
      if (!aResponse.error) {
        this.thread = new ThreadClient(this, this._threadActor);
        this.client.registerClient(this.thread);
      }
      aOnResponse(aResponse, this.thread);
    });
  },

  /**
   * Detach the client from the tab actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    before: function (aPacket) {
      if (this.thread) {
        this.thread.detach();
      }
      return aPacket;
    },
    after: function (aResponse) {
      this.client.unregisterClient(this);
      return aResponse;
    },
    telemetry: "TABDETACH"
  }),

  /**
   * Reload the page in this tab.
   *
   * @param [optional] object options
   *        An object with a `force` property indicating whether or not
   *        this reload should skip the cache
   */
  reload: function(options = { force: false }) {
    return this._reload(options);
  },
  _reload: DebuggerClient.requester({
    type: "reload",
    options: args(0)
  }, {
    telemetry: "RELOAD"
  }),

  /**
   * Navigate to another URL.
   *
   * @param string url
   *        The URL to navigate to.
   */
  navigateTo: DebuggerClient.requester({
    type: "navigateTo",
    url: args(0)
  }, {
    telemetry: "NAVIGATETO"
  }),

  /**
   * Reconfigure the tab actor.
   *
   * @param object aOptions
   *        A dictionary object of the new options to use in the tab actor.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: args(0)
  }, {
    telemetry: "RECONFIGURETAB"
  }),
};

eventSource(TabClient.prototype);

function AddonClient(aClient, aActor) {
  this._client = aClient;
  this._actor = aActor;
  this.request = this._client.request;
  this.events = [];
}

AddonClient.prototype = {
  get actor() { return this._actor; },
  get _transport() { return this._client._transport; },

  /**
   * Detach the client from the addon actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function(aResponse) {
      if (this._client.activeAddon === this) {
        this._client.activeAddon = null
      }
      this._client.unregisterClient(this);
      return aResponse;
    },
    telemetry: "ADDONDETACH"
  })
};

/**
 * A RootClient object represents a root actor on the server. Each
 * DebuggerClient keeps a RootClient instance representing the root actor
 * for the initial connection; DebuggerClient's 'listTabs' and
 * 'listChildProcesses' methods forward to that root actor.
 *
 * @param aClient object
 *      The client connection to which this actor belongs.
 * @param aGreeting string
 *      The greeting packet from the root actor we're to represent.
 *
 * Properties of a RootClient instance:
 *
 * @property actor string
 *      The name of this child's root actor.
 * @property applicationType string
 *      The application type, as given in the root actor's greeting packet.
 * @property traits object
 *      The traits object, as given in the root actor's greeting packet.
 */
function RootClient(aClient, aGreeting) {
  this._client = aClient;
  this.actor = aGreeting.from;
  this.applicationType = aGreeting.applicationType;
  this.traits = aGreeting.traits;
}

RootClient.prototype = {
  constructor: RootClient,

  /**
   * List the open tabs.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listTabs: DebuggerClient.requester({ type: "listTabs" },
                                     { telemetry: "LISTTABS" }),

  /**
   * List the installed addons.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listAddons: DebuggerClient.requester({ type: "listAddons" },
                                       { telemetry: "LISTADDONS" }),

  /*
   * Methods constructed by DebuggerClient.requester require these forwards
   * on their 'this'.
   */
  get _transport() { return this._client._transport; },
  get request()    { return this._client.request;    }
};

/**
 * Creates a thread client for the remote debugging protocol server. This client
 * is a front to the thread actor created in the server side, hiding the
 * protocol details in a traditional JavaScript API.
 *
 * @param aClient DebuggerClient|TabClient
 *        The parent of the thread (tab for tab-scoped debuggers, DebuggerClient
 *        for chrome debuggers).
 * @param aActor string
 *        The actor ID for this thread.
 */
function ThreadClient(aClient, aActor) {
  this._parent = aClient;
  this.client = aClient instanceof DebuggerClient ? aClient : aClient.client;
  this._actor = aActor;
  this._frameCache = [];
  this._scriptCache = {};
  this._pauseGrips = {};
  this._threadGrips = {};
  this.request = this.client.request;
  this.events = [];
}

ThreadClient.prototype = {
  _state: "paused",
  get state() { return this._state; },
  get paused() { return this._state === "paused"; },

  _pauseOnExceptions: false,
  _ignoreCaughtExceptions: false,
  _pauseOnDOMEvents: null,

  _actor: null,
  get actor() { return this._actor; },

  get compat() { return this.client.compat; },
  get _transport() { return this.client._transport; },

  _assertPaused: function (aCommand) {
    if (!this.paused) {
      throw Error(aCommand + " command sent while not paused. Currently " + this._state);
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

      if (this._pauseOnExceptions) {
        aPacket.pauseOnExceptions = this._pauseOnExceptions;
      }
      if (this._ignoreCaughtExceptions) {
        aPacket.ignoreCaughtExceptions = this._ignoreCaughtExceptions;
      }
      if (this._pauseOnDOMEvents) {
        aPacket.pauseOnDOMEvents = this._pauseOnDOMEvents;
      }
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
   * Reconfigure the thread actor.
   *
   * @param object aOptions
   *        A dictionary object of the new options to use in the thread actor.
   * @param function aOnResponse
   *        Called with the response packet.
   */
  reconfigure: DebuggerClient.requester({
    type: "reconfigure",
    options: args(0)
  }, {
    telemetry: "RECONFIGURETHREAD"
  }),

  /**
   * Resume a paused thread.
   */
  resume: function (aOnResponse) {
    this._doResume(null, aOnResponse);
  },

  /**
   * Resume then pause without stepping.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  breakOnNext: function (aOnResponse) {
    this._doResume({ type: "break" }, aOnResponse);
  },

  /**
   * Step over a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOver: function (aOnResponse) {
    this._doResume({ type: "next" }, aOnResponse);
  },

  /**
   * Step into a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepIn: function (aOnResponse) {
    this._doResume({ type: "step" }, aOnResponse);
  },

  /**
   * Step out of a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOut: function (aOnResponse) {
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
  pauseOnExceptions: function (aPauseOnExceptions,
                               aIgnoreCaughtExceptions,
                               aOnResponse = noop) {
    this._pauseOnExceptions = aPauseOnExceptions;
    this._ignoreCaughtExceptions = aIgnoreCaughtExceptions;

    // If the debuggee is paused, we have to send the flag via a reconfigure
    // request.
    if (this.paused) {
      this.reconfigure({
        pauseOnExceptions: aPauseOnExceptions,
        ignoreCaughtExceptions: aIgnoreCaughtExceptions
      }, aOnResponse);
      return;
    }
    // Otherwise send the flag using a standard resume request.
    this.interrupt(aResponse => {
      if (aResponse.error) {
        // Can't continue if pausing failed.
        aOnResponse(aResponse);
        return;
      }
      this.resume(aOnResponse);
    });
  },

  /**
   * Enable pausing when the specified DOM events are triggered. Disabling
   * pausing on an event can be realized by calling this method with the updated
   * array of events that doesn't contain it.
   *
   * @param array|string events
   *        An array of strings, representing the DOM event types to pause on,
   *        or "*" to pause on all DOM events. Pass an empty array to
   *        completely disable pausing on DOM events.
   * @param function onResponse
   *        Called with the response packet in a future turn of the event loop.
   */
  pauseOnDOMEvents: function (events, onResponse = noop) {
    this._pauseOnDOMEvents = events;
    // If the debuggee is paused, the value of the array will be communicated in
    // the next resumption. Otherwise we have to force a pause in order to send
    // the array.
    if (this.paused) {
      DevToolsUtils.executeSoon(() => onResponse({}));
      return;
    }
    this.interrupt(response => {
      // Can't continue if pausing failed.
      if (response.error) {
        onResponse(response);
        return;
      }
      this.resume(onResponse);
    });
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
        this._state = "paused";
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
      this.client.unregisterClient(this);
      this._parent.thread = null;
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
  setBreakpoint: function ({ url, line, column, condition },
                           aOnResponse = noop) {
    // A helper function that sets the breakpoint.
    let doSetBreakpoint = (aCallback) => {
      const location = {
        url: url,
        line: line,
        column: column
      };

      let packet = {
        to: this._actor,
        type: "setBreakpoint",
        location: location,
        condition: condition
      };
      this.client.request(packet, (aResponse) => {
        // Ignoring errors, since the user may be setting a breakpoint in a
        // dead script that will reappear on a page reload.
        let bpClient;
        if (aResponse.actor) {
          let root = this.client.mainRoot;
          bpClient = new BreakpointClient(
            this.client,
            aResponse.actor,
            location,
            root.traits.conditionalBreakpoints ? condition : undefined
          );
        }
        aOnResponse(aResponse, bpClient);
        if (aCallback) {
          aCallback();
        }
      });
    };

    // If the debuggee is paused, just set the breakpoint.
    if (this.paused) {
      doSetBreakpoint();
      return;
    }
    // Otherwise, force a pause in order to set the breakpoint.
    this.interrupt((aResponse) => {
      if (aResponse.error) {
        // Can't set the breakpoint if pausing failed.
        aOnResponse(aResponse);
        return;
      }

      const { type, why } = aResponse;
      const cleanUp = type == "paused" && why.type == "interrupted"
        ? () => this.resume()
        : noop;

      doSetBreakpoint(cleanUp);
    });
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
   * Return the event listeners defined on the page.
   *
   * @param aOnResponse Function
   *        Called with the thread's response.
   */
  eventListeners: DebuggerClient.requester({
    type: "eventListeners"
  }, {
    telemetry: "EVENTLISTENERS"
  }),

  /**
   * Request the loaded sources for the current thread.
   *
   * @param aOnResponse Function
   *        Called with the thread's response.
   */
  getSources: DebuggerClient.requester({
    type: "sources"
  }, {
    telemetry: "SOURCES"
  }),

  /**
   * Clear the thread's source script cache. A scriptscleared event
   * will be sent.
   */
  _clearScripts: function () {
    if (Object.keys(this._scriptCache).length > 0) {
      this._scriptCache = {}
      this.emit("scriptscleared");
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
   * @param aCallback function
   *        Optional callback function called when frames have been loaded
   * @returns true if a framesadded notification should be expected.
   */
  fillFrames: function (aTotal, aCallback=noop) {
    this._assertPaused("fillFrames");
    if (this._frameCache.length >= aTotal) {
      return false;
    }

    let numFrames = this._frameCache.length;

    this.getFrames(numFrames, aTotal - numFrames, (aResponse) => {
      if (aResponse.error) {
        aCallback(aResponse);
        return;
      }

      for each (let frame in aResponse.frames) {
        this._frameCache[frame.depth] = frame;
      }

      // If we got as many frames as we asked for, there might be more
      // frames available.
      this.emit("framesadded");

      aCallback(aResponse);
    });

    return true;
  },

  /**
   * Clear the thread's stack frame cache. A framescleared event
   * will be sent.
   */
  _clearFrames: function () {
    if (this._frameCache.length > 0) {
      this._frameCache = [];
      this.emit("framescleared");
    }
  },

  /**
   * Return a ObjectClient object for the given object grip.
   *
   * @param aGrip object
   *        A pause-lifetime object grip returned by the protocol.
   */
  pauseGrip: function (aGrip) {
    if (aGrip.actor in this._pauseGrips) {
      return this._pauseGrips[aGrip.actor];
    }

    let client = new ObjectClient(this.client, aGrip);
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
  _longString: function (aGrip, aGripCacheName) {
    if (aGrip.actor in this[aGripCacheName]) {
      return this[aGripCacheName][aGrip.actor];
    }

    let client = new LongStringClient(this.client, aGrip);
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
  pauseLongString: function (aGrip) {
    return this._longString(aGrip, "_pauseGrips");
  },

  /**
   * Return an instance of LongStringClient for the given long string grip that
   * is scoped to the thread lifetime.
   *
   * @param aGrip Object
   *        The long string grip returned by the protocol.
   */
  threadLongString: function (aGrip) {
    return this._longString(aGrip, "_threadGrips");
  },

  /**
   * Clear and invalidate all the grip clients from the given cache.
   *
   * @param aGripCacheName
   *        The property name of the grip cache we want to clear.
   */
  _clearObjectClients: function (aGripCacheName) {
    for each (let grip in this[aGripCacheName]) {
      grip.valid = false;
    }
    this[aGripCacheName] = {};
  },

  /**
   * Invalidate pause-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearPauseGrips: function () {
    this._clearObjectClients("_pauseGrips");
  },

  /**
   * Invalidate thread-lifetime grip clients and clear the list of current grip
   * clients.
   */
  _clearThreadGrips: function () {
    this._clearObjectClients("_threadGrips");
  },

  /**
   * Handle thread state change by doing necessary cleanup and notifying all
   * registered listeners.
   */
  _onThreadState: function (aPacket) {
    this._state = ThreadStateTypes[aPacket.type];
    this._clearFrames();
    this._clearPauseGrips();
    aPacket.type === ThreadStateTypes.detached && this._clearThreadGrips();
    this.client._eventsEnabled && this.emit(aPacket.type, aPacket);
  },

  /**
   * Return an EnvironmentClient instance for the given environment actor form.
   */
  environment: function (aForm) {
    return new EnvironmentClient(this.client, aForm);
  },

  /**
   * Return an instance of SourceClient for the given source actor form.
   */
  source: function (aForm) {
    if (aForm.actor in this._threadGrips) {
      return this._threadGrips[aForm.actor];
    }

    return this._threadGrips[aForm.actor] = new SourceClient(this, aForm);
  },

  /**
   * Request the prototype and own properties of mutlipleObjects.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   * @param actors [string]
   *        List of actor ID of the queried objects.
   */
  getPrototypesAndProperties: DebuggerClient.requester({
    type: "prototypesAndProperties",
    actors: args(0)
  }, {
    telemetry: "PROTOTYPESANDPROPERTIES"
  })
};

eventSource(ThreadClient.prototype);

/**
 * Creates a tracing profiler client for the remote debugging protocol
 * server. This client is a front to the trace actor created on the
 * server side, hiding the protocol details in a traditional
 * JavaScript API.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aActor string
 *        The actor ID for this thread.
 */
function TraceClient(aClient, aActor) {
  this._client = aClient;
  this._actor = aActor;
  this._activeTraces = new Set();
  this._waitingPackets = new Map();
  this._expectedPacket = 0;
  this.request = this._client.request;
  this.events = [];
}

TraceClient.prototype = {
  get actor()   { return this._actor; },
  get tracing() { return this._activeTraces.size > 0; },

  get _transport() { return this._client._transport; },

  /**
   * Detach from the trace actor.
   */
  detach: DebuggerClient.requester({
    type: "detach"
  }, {
    after: function (aResponse) {
      this._client.unregisterClient(this);
      return aResponse;
    },
    telemetry: "TRACERDETACH"
  }),

  /**
   * Start a new trace.
   *
   * @param aTrace [string]
   *        An array of trace types to be recorded by the new trace.
   *
   * @param aName string
   *        The name of the new trace.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   */
  startTrace: DebuggerClient.requester({
    type: "startTrace",
    name: args(1),
    trace: args(0)
  }, {
    after: function (aResponse) {
      if (aResponse.error) {
        return aResponse;
      }

      if (!this.tracing) {
        this._waitingPackets.clear();
        this._expectedPacket = 0;
      }
      this._activeTraces.add(aResponse.name);

      return aResponse;
    },
    telemetry: "STARTTRACE"
  }),

  /**
   * End a trace. If a name is provided, stop the named
   * trace. Otherwise, stop the most recently started trace.
   *
   * @param aName string
   *        The name of the trace to stop.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   */
  stopTrace: DebuggerClient.requester({
    type: "stopTrace",
    name: args(0)
  }, {
    after: function (aResponse) {
      if (aResponse.error) {
        return aResponse;
      }

      this._activeTraces.delete(aResponse.name);

      return aResponse;
    },
    telemetry: "STOPTRACE"
  })
};

/**
 * Grip clients are used to retrieve information about the relevant object.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aGrip object
 *        A pause-lifetime object grip returned by the protocol.
 */
function ObjectClient(aClient, aGrip)
{
  this._grip = aGrip;
  this._client = aClient;
  this.request = this._client.request;
}

ObjectClient.prototype = {
  get actor() { return this._grip.actor },
  get _transport() { return this._client._transport; },

  valid: true,

  get isFrozen() this._grip.frozen,
  get isSealed() this._grip.sealed,
  get isExtensible() this._grip.extensible,

  getDefinitionSite: DebuggerClient.requester({
    type: "definitionSite"
  }, {
    before: function (aPacket) {
      if (this._grip.class != "Function") {
        throw new Error("getDefinitionSite is only valid for function grips.");
      }
      return aPacket;
    }
  }),

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

  /**
   * Request the display string of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getDisplayString: DebuggerClient.requester({
    type: "displayString"
  }, {
    telemetry: "DISPLAYSTRING"
  }),

  /**
   * Request the scope of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getScope: DebuggerClient.requester({
    type: "scope"
  }, {
    before: function (aPacket) {
      if (this._grip.class !== "Function") {
        throw new Error("scope is only valid for function grips.");
      }
      return aPacket;
    },
    telemetry: "SCOPE"
  })
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
 * @param aClient ThreadClient
 *        The thread client parent.
 * @param aForm Object
 *        The form sent across the remote debugging protocol.
 */
function SourceClient(aClient, aForm) {
  this._form = aForm;
  this._isBlackBoxed = aForm.isBlackBoxed;
  this._isPrettyPrinted = aForm.isPrettyPrinted;
  this._activeThread = aClient;
  this._client = aClient.client;
}

SourceClient.prototype = {
  get _transport() this._client._transport,
  get isBlackBoxed() this._isBlackBoxed,
  get isPrettyPrinted() this._isPrettyPrinted,
  get actor() this._form.actor,
  get request() this._client.request,
  get url() this._form.url,

  /**
   * Black box this SourceClient's source.
   *
   * @param aCallback Function
   *        The callback function called when we receive the response from the server.
   */
  blackBox: DebuggerClient.requester({
    type: "blackbox"
  }, {
    telemetry: "BLACKBOX",
    after: function (aResponse) {
      if (!aResponse.error) {
        this._isBlackBoxed = true;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return aResponse;
    }
  }),

  /**
   * Un-black box this SourceClient's source.
   *
   * @param aCallback Function
   *        The callback function called when we receive the response from the server.
   */
  unblackBox: DebuggerClient.requester({
    type: "unblackbox"
  }, {
    telemetry: "UNBLACKBOX",
    after: function (aResponse) {
      if (!aResponse.error) {
        this._isBlackBoxed = false;
        if (this._activeThread) {
          this._activeThread.emit("blackboxchange", this);
        }
      }
      return aResponse;
    }
  }),

  /**
   * Get Executable Lines from a source
   *
   * @param aCallback Function
   *        The callback function called when we receive the response from the server.
   */
  getExecutableLines: function(cb){
    let packet = {
      to: this._form.actor,
      type: "getExecutableLines"
    };

    this._client.request(packet, res => {
      cb(res.lines);
    });
  },

  /**
   * Get a long string grip for this SourceClient's source.
   */
  source: function (aCallback) {
    let packet = {
      to: this._form.actor,
      type: "source"
    };
    this._client.request(packet, aResponse => {
      this._onSourceResponse(aResponse, aCallback)
    });
  },

  /**
   * Pretty print this source's text.
   */
  prettyPrint: function (aIndent, aCallback) {
    const packet = {
      to: this._form.actor,
      type: "prettyPrint",
      indent: aIndent
    };
    this._client.request(packet, aResponse => {
      if (!aResponse.error) {
        this._isPrettyPrinted = true;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      this._onSourceResponse(aResponse, aCallback);
    });
  },

  /**
   * Stop pretty printing this source's text.
   */
  disablePrettyPrint: function (aCallback) {
    const packet = {
      to: this._form.actor,
      type: "disablePrettyPrint"
    };
    this._client.request(packet, aResponse => {
      if (!aResponse.error) {
        this._isPrettyPrinted = false;
        this._activeThread._clearFrames();
        this._activeThread.emit("prettyprintchange", this);
      }
      this._onSourceResponse(aResponse, aCallback);
    });
  },

  _onSourceResponse: function (aResponse, aCallback) {
    if (aResponse.error) {
      aCallback(aResponse);
      return;
    }

    if (typeof aResponse.source === "string") {
      aCallback(aResponse);
      return;
    }

    let { contentType, source } = aResponse;
    let longString = this._activeThread.threadLongString(source);
    longString.substring(0, longString.length, function (aResponse) {
      if (aResponse.error) {
        aCallback(aResponse);
        return;
      }

      aCallback({
        source: aResponse.substring,
        contentType: contentType
      });
    });
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
 * @param aCondition string
 *        The conditional expression of the breakpoint
 */
function BreakpointClient(aClient, aActor, aLocation, aCondition) {
  this._client = aClient;
  this._actor = aActor;
  this.location = aLocation;
  this.request = this._client.request;

  // The condition property should only exist if it's a truthy value
  if (aCondition) {
    this.condition = aCondition;
  }
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

  /**
   * Determines if this breakpoint has a condition
   */
  hasCondition: function() {
    let root = this._client.mainRoot;
    // XXX bug 990137: We will remove support for client-side handling of
    // conditional breakpoints
    if (root.traits.conditionalBreakpoints) {
      return "condition" in this;
    } else {
      return "conditionalExpression" in this;
    }
  },

  /**
   * Get the condition of this breakpoint. Currently we have to
   * support locally emulated conditional breakpoints until the
   * debugger servers are updated (see bug 990137). We used a
   * different property when moving it server-side to ensure that we
   * are testing the right code.
   */
  getCondition: function() {
    let root = this._client.mainRoot;
    if (root.traits.conditionalBreakpoints) {
      return this.condition;
    } else {
      return this.conditionalExpression;
    }
  },

  /**
   * Set the condition of this breakpoint
   */
  setCondition: function(gThreadClient, aCondition) {
    let root = this._client.mainRoot;
    let deferred = promise.defer();

    if (root.traits.conditionalBreakpoints) {
      let info = {
        url: this.location.url,
        line: this.location.line,
        column: this.location.column,
        condition: aCondition
      };

      // Remove the current breakpoint and add a new one with the
      // condition.
      this.remove(aResponse => {
        if (aResponse && aResponse.error) {
          deferred.reject(aResponse);
          return;
        }

        gThreadClient.setBreakpoint(info, (aResponse, aNewBreakpoint) => {
          if (aResponse && aResponse.error) {
            deferred.reject(aResponse);
          } else {
            deferred.resolve(aNewBreakpoint);
          }
        });
      });
    } else {
      // The property shouldn't even exist if the condition is blank
      if(aCondition === "") {
        delete this.conditionalExpression;
      }
      else {
        this.conditionalExpression = aCondition;
      }
      deferred.resolve(this);
    }

    return deferred.promise;
  }
};

eventSource(BreakpointClient.prototype);

/**
 * Environment clients are used to manipulate the lexical environment actors.
 *
 * @param aClient DebuggerClient
 *        The debugger client parent.
 * @param aForm Object
 *        The form sent across the remote debugging protocol.
 */
function EnvironmentClient(aClient, aForm) {
  this._client = aClient;
  this._form = aForm;
  this.request = this._client.request;
}

EnvironmentClient.prototype = {

  get actor() this._form.actor,
  get _transport() { return this._client._transport; },

  /**
   * Fetches the bindings introduced by this lexical environment.
   */
  getBindings: DebuggerClient.requester({
    type: "bindings"
  }, {
    telemetry: "BINDINGS"
  }),

  /**
   * Changes the value of the identifier whose name is name (a string) to that
   * represented by value (a grip).
   */
  assign: DebuggerClient.requester({
    type: "assign",
    name: args(0),
    value: args(1)
  }, {
    telemetry: "ASSIGN"
  })
};

eventSource(EnvironmentClient.prototype);

/**
 * Connects to a debugger server socket and returns a DebuggerTransport.
 *
 * @param aHost string
 *        The host name or IP address of the debugger server.
 * @param aPort number
 *        The port number of the debugger server.
 */
this.debuggerSocketConnect = function (aHost, aPort)
{
  let s = socketTransportService.createTransport(null, 0, aHost, aPort, null);
  // By default the CONNECT socket timeout is very long, 65535 seconds,
  // so that if we race to be in CONNECT state while the server socket is still
  // initializing, the connection is stuck in connecting state for 18.20 hours!
  s.setTimeout(Ci.nsISocketTransport.TIMEOUT_CONNECT, 2);

  // openOutputStream may throw NS_ERROR_NOT_INITIALIZED if we hit some race
  // where the nsISocketTransport gets shutdown in between its instantiation and
  // the call to this method.
  let transport;
  try {
    transport = new DebuggerTransport(s.openInputStream(0, 0, 0),
                                      s.openOutputStream(0, 0, 0));
  } catch(e) {
    DevToolsUtils.reportException("debuggerSocketConnect", e);
    throw e;
  }
  return transport;
}
