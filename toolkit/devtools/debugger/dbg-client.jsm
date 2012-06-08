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

var EXPORTED_SYMBOLS = ["DebuggerTransport",
                        "DebuggerClient",
                        "debuggerSocketConnect"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "socketTransportService",
                                   "@mozilla.org/network/socket-transport-service;1",
                                   "nsISocketTransportService");

let wantLogging = Services.prefs.getBoolPref("devtools.debugger.log");

function dumpn(str)
{
  if (wantLogging) {
    dump("DBG-CLIENT: " + str + "\n");
  }
}

let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://global/content/devtools/dbg-transport.js");

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
   *        The event to listen for, or null to listen to all events.
   * @param aListener function
   *        Called when the event is fired. If the same listener
   *        is added more the once, it will be called once per
   *        addListener call.
   */
  aProto.addListener = function EV_addListener(aName, aListener) {
    if (typeof aListener != "function") {
      return;
    }

    if (!this._listeners) {
      this._listeners = {};
    }

    if (!aName) {
      aName = '*';
    }

    this._getListeners(aName).push(aListener);
  };

  /**
   * Add a listener to the event source for a given event. The
   * listener will be removed after it is called for the first time.
   *
   * @param aName string
   *        The event to listen for, or null to respond to the first event
   *        fired by the object.
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
    if (this._listeners['*']) {
      listeners.concat(this._listeners['*']);
    }

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
  "newScript": "newScript",
  "tabDetached": "tabDetached",
  "tabNavigated": "tabNavigated"
};

/**
 * Set of debug protocol request types that specify the protocol request being
 * sent to the server.
 */
const DebugProtocolTypes = {
  "assign": "assign",
  "attach": "attach",
  "clientEvaluate": "clientEvaluate",
  "delete": "delete",
  "detach": "detach",
  "frames": "frames",
  "interrupt": "interrupt",
  "listTabs": "listTabs",
  "nameAndParameters": "nameAndParameters",
  "ownPropertyNames": "ownPropertyNames",
  "property": "property",
  "prototype": "prototype",
  "prototypeAndProperties": "prototypeAndProperties",
  "resume": "resume",
  "scripts": "scripts",
  "setBreakpoint": "setBreakpoint"
};

const ROOT_ACTOR_NAME = "root";

/**
 * Creates a client for the remote debugging protocol server. This client
 * provides the means to communicate with the server and exchange the messages
 * required by the protocol in a traditional JavaScript API.
 */
function DebuggerClient(aTransport)
{
  this._transport = aTransport;
  this._transport.hooks = this;
  this._threadClients = {};
  this._tabClients = {};

  this._pendingRequests = [];
  this._activeRequests = {};
  this._eventsEnabled = true;
}

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

    let closeTransport = function _closeTransport() {
      this._transport.close();
      this._transport = null;
    }.bind(this);

    let detachTab = function _detachTab() {
      if (this.activeTab) {
        this.activeTab.detach(closeTransport);
      } else {
        closeTransport();
      }
    }.bind(this);

    if (this.activeThread) {
      this.activeThread.detach(detachTab);
    } else {
      detachTab();
    }
  },

  /**
   * List the open tabs.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  listTabs: function DC_listTabs(aOnResponse) {
    let packet = { to: ROOT_ACTOR_NAME, type: DebugProtocolTypes.listTabs };
    this.request(packet, function(aResponse) {
      aOnResponse(aResponse);
    });
  },

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
    let packet = { to: aTabActor, type: DebugProtocolTypes.attach };
    this.request(packet, function(aResponse) {
      if (!aResponse.error) {
        var tabClient = new TabClient(self, aTabActor);
        self._tabClients[aTabActor] = tabClient;
        self.activeTab = tabClient;
      }
      aOnResponse(aResponse, tabClient);
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
   */
  attachThread: function DC_attachThread(aThreadActor, aOnResponse) {
    let self = this;
    let packet = { to: aThreadActor, type: DebugProtocolTypes.attach };
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
    this._pendingRequests = this._pendingRequests.filter(function(request) {
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
   */
  onPacket: function DC_onPacket(aPacket) {
    if (!this._connected) {
      // Hello packet.
      this._connected = true;
      this.notify("connected",
                  aPacket.applicationType,
                  aPacket.traits);
      return;
    }

    try {
      if (!aPacket.from) {
        Cu.reportError("Server did not specify an actor, dropping packet: " +
                       JSON.stringify(aPacket));
        return;
      }

      let onResponse;
      // Don't count unsolicited notifications as responses.
      if (aPacket.from in this._activeRequests &&
          !(aPacket.type in UnsolicitedNotifications)) {
        onResponse = this._activeRequests[aPacket.from].onResponse;
        delete this._activeRequests[aPacket.from];
      }

      // paused/resumed/detached get special treatment...
      if (aPacket.type in ThreadStateTypes &&
          aPacket.from in this._threadClients) {
        this._threadClients[aPacket.from]._onThreadState(aPacket);
      }
      this.notify(aPacket.type, aPacket);

      if (onResponse) {
        onResponse(aPacket);
      }
    } catch(ex) {
      dumpn("Error handling response: " + ex + " - stack:\n" + ex.stack);
      Cu.reportError(ex);
    }

    this._sendRequests();
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
}

TabClient.prototype = {
  /**
   * Detach the client from the tab actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: function TabC_detach(aOnResponse) {
    let self = this;
    let packet = { to: this._actor, type: DebugProtocolTypes.detach };
    this._client.request(packet, function(aResponse) {
      if (self.activeTab === self._client._tabClients[self._actor]) {
        delete self.activeTab;
      }
      delete self._client._tabClients[self._actor];
      if (aOnResponse) {
        aOnResponse(aResponse);
      }
    });
  }
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
}

ThreadClient.prototype = {
  _state: "paused",
  get state() { return this._state; },
  get paused() { return this._state === "paused"; },

  _pauseOnExceptions: false,

  _actor: null,
  get actor() { return this._actor; },

  _assertPaused: function TC_assertPaused(aCommand) {
    if (!this.paused) {
      throw Error(aCommand + " command sent while not paused.");
    }
  },

  /**
   * Resume a paused thread. If the optional aLimit parameter is present, then
   * the thread will also pause when that limit is reached.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   * @param [optional] object aLimit
   *        An object with a type property set to the appropriate limit (next,
   *        step, or finish) per the remote debugging protocol specification.
   */
  resume: function TC_resume(aOnResponse, aLimit) {
    this._assertPaused("resume");

    // Put the client in a tentative "resuming" state so we can prevent
    // further requests that should only be sent in the paused state.
    this._state = "resuming";

    let self = this;
    let packet = {
      to: this._actor,
      type: DebugProtocolTypes.resume,
      resumeLimit: aLimit,
      pauseOnExceptions: this._pauseOnExceptions
    };
    this._client.request(packet, function(aResponse) {
      if (aResponse.error) {
        // There was an error resuming, back to paused state.
        self._state = "paused";
      }
      if (aOnResponse) {
        aOnResponse(aResponse);
      }
    });
  },

  /**
   * Step over a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOver: function TC_stepOver(aOnResponse) {
    this.resume(aOnResponse, { type: "next" });
  },

  /**
   * Step into a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepIn: function TC_stepIn(aOnResponse) {
    this.resume(aOnResponse, { type: "step" });
  },

  /**
   * Step out of a function call.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  stepOut: function TC_stepOut(aOnResponse) {
    this.resume(aOnResponse, { type: "finish" });
  },

  /**
   * Interrupt a running thread.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  interrupt: function TC_interrupt(aOnResponse) {
    let packet = { to: this._actor, type: DebugProtocolTypes.interrupt };
    this._client.request(packet, function(aResponse) {
      if (aOnResponse) {
        aOnResponse(aResponse);
      }
    });
  },

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
  eval: function TC_eval(aFrame, aExpression, aOnResponse) {
    this._assertPaused("eval");

    // Put the client in a tentative "resuming" state so we can prevent
    // further requests that should only be sent in the paused state.
    this._state = "resuming";

    let self = this;
    let request = { to: this._actor, type: DebugProtocolTypes.clientEvaluate,
                    frame: aFrame, expression: aExpression };
    this._client.request(request, function(aResponse) {
      if (aResponse.error) {
        // There was an error resuming, back to paused state.
        self._state = "paused";
      }

      if (aOnResponse) {
        aOnResponse(aResponse);
      }
    });
  },

  /**
   * Detach from the thread actor.
   *
   * @param function aOnResponse
   *        Called with the response packet.
   */
  detach: function TC_detach(aOnResponse) {
    let self = this;
    let packet = { to: this._actor, type: DebugProtocolTypes.detach };
    this._client.request(packet, function(aResponse) {
      if (self.activeThread === self._client._threadClients[self._actor]) {
        delete self.activeThread;
      }
      delete self._client._threadClients[self._actor];
      if (aOnResponse) {
        aOnResponse(aResponse);
      }
    });
  },

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
      let packet = { to: this._actor, type: DebugProtocolTypes.setBreakpoint,
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
   * Request the loaded scripts for the current thread.
   *
   * @param aOnResponse integer
   *        Called with the thread's response.
   */
  getScripts: function TC_getScripts(aOnResponse) {
    let packet = { to: this._actor, type: DebugProtocolTypes.scripts };
    this._client.request(packet, aOnResponse);
  },

  /**
   * A cache of source scripts. Clients can observe the scriptsadded and
   * scriptscleared event to keep up to date on changes to this cache,
   * and can fill it using the fillScripts method.
   */
  get cachedScripts() { return this._scriptCache; },

  /**
   * Ensure that source scripts have been loaded in the
   * ThreadClient's source script cache. A scriptsadded event will be
   * sent when the source script cache is updated.
   *
   * @returns true if a scriptsadded notification should be expected.
   */
  fillScripts: function TC_fillScripts() {
    let self = this;
    this.getScripts(function(aResponse) {
      for each (let script in aResponse.scripts) {
        self._scriptCache[script.url] = script;
      }
      // If the cache was modified, notify listeners.
      if (aResponse.scripts && aResponse.scripts.length) {
        self.notify("scriptsadded");
      }
    });
    return true;
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
  getFrames: function TC_getFrames(aStart, aCount, aOnResponse) {
    this._assertPaused("frames");

    let packet = { to: this._actor, type: DebugProtocolTypes.frames,
                   start: aStart, count: aCount ? aCount : undefined };
    this._client.request(packet, aOnResponse);
  },

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
    if (!this._pauseGrips) {
      this._pauseGrips = {};
    }

    if (aGrip.actor in this._pauseGrips) {
      return this._pauseGrips[aGrip.actor];
    }

    let client = new GripClient(this._client, aGrip);
    this._pauseGrips[aGrip.actor] = client;
    return client;
  },

  /**
   * Invalidate pause-lifetime grip clients and clear the list of
   * current grip clients.
   */
  _clearPauseGrips: function TC_clearPauseGrips(aPacket) {
    for each (let grip in this._pauseGrips) {
      grip.valid = false;
    }
    this._pauseGrips = null;
  },

  /**
   * Handle thread state change by doing necessary cleanup and notifying all
   * registered listeners.
   */
  _onThreadState: function TC_onThreadState(aPacket) {
    this._state = ThreadStateTypes[aPacket.type];
    this._clearFrames();
    this._clearPauseGrips();
    this._client._eventsEnabled && this.notify(aPacket.type, aPacket);
  },
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
}

GripClient.prototype = {
  get actor() { return this._grip.actor },

  _valid: true,
  get valid() { return this._valid; },
  set valid(aValid) { this._valid = !!aValid; },

  /**
   * Request the name of the function and its formal parameters.
   *
   * @param aOnResponse function
   *        Called with the request's response.
   */
  getSignature: function GC_getSignature(aOnResponse) {
    if (this._grip["class"] !== "Function") {
      throw "getSignature is only valid for function grips.";
    }

    let packet = { to: this.actor, type: DebugProtocolTypes.nameAndParameters };
    this._client.request(packet, function (aResponse) {
                                   if (aOnResponse) {
                                     aOnResponse(aResponse);
                                   }
                                 });
  },

  /**
   * Request the names of the properties defined on the object and not its
   * prototype.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getOwnPropertyNames: function GC_getOwnPropertyNames(aOnResponse) {
    let packet = { to: this.actor, type: DebugProtocolTypes.ownPropertyNames };
    this._client.request(packet, function (aResponse) {
                                   if (aOnResponse) {
                                     aOnResponse(aResponse);
                                   }
                                 });
  },

  /**
   * Request the prototype and own properties of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getPrototypeAndProperties: function GC_getPrototypeAndProperties(aOnResponse) {
    let packet = { to: this.actor,
                   type: DebugProtocolTypes.prototypeAndProperties };
    this._client.request(packet, function (aResponse) {
                                   if (aOnResponse) {
                                     aOnResponse(aResponse);
                                   }
                                 });
  },

  /**
   * Request the property descriptor of the object's specified property.
   *
   * @param aName string The name of the requested property.
   * @param aOnResponse function Called with the request's response.
   */
  getProperty: function GC_getProperty(aName, aOnResponse) {
    let packet = { to: this.actor, type: DebugProtocolTypes.property,
                   name: aName };
    this._client.request(packet, function (aResponse) {
                                   if (aOnResponse) {
                                     aOnResponse(aResponse);
                                   }
                                 });
  },

  /**
   * Request the prototype of the object.
   *
   * @param aOnResponse function Called with the request's response.
   */
  getPrototype: function GC_getPrototype(aOnResponse) {
    let packet = { to: this.actor, type: DebugProtocolTypes.prototype };
    this._client.request(packet, function (aResponse) {
                                   if (aOnResponse) {
                                     aOnResponse(aResponse);
                                   }
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
 */
function BreakpointClient(aClient, aActor, aLocation) {
  this._client = aClient;
  this._actor = aActor;
  this.location = aLocation;
}

BreakpointClient.prototype = {

  _actor: null,
  get actor() { return this._actor; },

  /**
   * Remove the breakpoint from the server.
   */
  remove: function BC_remove(aOnResponse) {
    let packet = { to: this._actor, type: DebugProtocolTypes["delete"] };
    this._client.request(packet, function(aResponse) {
                                   if (aOnResponse) {
                                     aOnResponse(aResponse);
                                   }
                                 });
  }
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
function debuggerSocketConnect(aHost, aPort)
{
  let s = socketTransportService.createTransport(null, 0, aHost, aPort, null);
  let transport = new DebuggerTransport(s.openInputStream(0, 0, 0),
                                        s.openOutputStream(0, 0, 0));
  return transport;
}
