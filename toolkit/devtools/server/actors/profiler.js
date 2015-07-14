/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");
const protocol = require("devtools/server/protocol");
const { custom, method, RetVal, Arg, Option, types } = protocol;
const { Profiler } = require("devtools/toolkit/shared/profiler");
const { actorBridge } = require("devtools/server/actors/common");

loader.lazyRequireGetter(this, "events", "sdk/event/core");
loader.lazyRequireGetter(this, "extend", "sdk/util/object", true);

types.addType("profiler-data", {
  // On Fx42+, the profile is only deserialized on the front; older
  // servers will get the profiler data as an object from nsIProfiler,
  // causing one parse/stringify cycle, then again implicitly in a packet.
  read: (v) => {
    if (typeof v.profile === "string") {
      // Create a new response object since `profile` is read only.
      let newValue = Object.create(null);
      newValue.profile = JSON.parse(v.profile);
      newValue.currentTime = v.currentTime;
      return newValue;
    }
    return v;
  }
});

/**
 * This actor wraps the Profiler module at toolkit/devtools/shared/profiler.js
 * and provides RDP definitions.
 *
 * @see toolkit/devtools/shared/profiler.js for documentation.
 */
let ProfilerActor = exports.ProfilerActor = protocol.ActorClass({
  typeName: "profiler",

  /**
   * The set of events the ProfilerActor emits over RDP.
   */
  events: {
    "console-api-profiler": {
      data: Arg(0, "json"),
    },
    "profiler-started": {
      data: Arg(0, "json"),
    },
    "profiler-stopped": {
      data: Arg(0, "json"),
    },

    // Only for older geckos, pre-protocol.js ProfilerActor (<Fx42).
    // Emitted on other events as a transition from older profiler events
    // to newer ones.
    "eventNotification": {
      subject: Option(0, "json"),
      topic: Option(0, "string"),
      details: Option(0, "json")
    }
  },

  initialize: function (conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this._onProfilerEvent = this._onProfilerEvent.bind(this);

    this.bridge = new Profiler();
    events.on(this.bridge, "*", this._onProfilerEvent);
  },

  /**
   * `disconnect` method required to call destroy, since this
   * actor is not managed by a parent actor.
   */
  disconnect: function() {
    this.destroy();
  },

  destroy: function() {
    events.off(this.bridge, "*", this._onProfilerEvent);
    this.bridge.destroy();
    protocol.Actor.prototype.destroy.call(this);
  },

  startProfiler: actorBridge("start", {
    // Write out every property in the request, since we want all these options to be
    // on the packet's top-level for backwards compatibility, when the profiler actor
    // was not using protocol.js (<Fx42)
    request: {
      entries: Option(0, "nullable:number"),
      interval: Option(0, "nullable:number"),
      features: Option(0, "nullable:array:string"),
      threadFilters: Option(0, "nullable:array:string"),
    },
    response: RetVal("json"),
  }),

  stopProfiler: actorBridge("stop", {
    response: RetVal("json"),
  }),

  getProfile: actorBridge("getProfile", {
    request: {
      startTime: Option(0, "nullable:number"),
      stringify: Option(0, "nullable:boolean")
    },
    response: RetVal("profiler-data")
  }),

  getFeatures: actorBridge("getFeatures", {
    response: RetVal("json")
  }),

  getBufferInfo: actorBridge("getBufferInfo", {
    response: RetVal("json")
  }),

  getStartOptions: actorBridge("getStartOptions", {
    response: RetVal("json")
  }),

  isActive: actorBridge("isActive", {
    response: RetVal("json")
  }),

  getSharedLibraryInformation: actorBridge("getSharedLibraryInformation", {
    response: RetVal("json")
  }),

  registerEventNotifications: actorBridge("registerEventNotifications", {
    // Explicitly enumerate the arguments
    // @see ProfilerActor#startProfiler
    request: {
      events: Option(0, "nullable:array:string"),
    },
    response: RetVal("json")
  }),

  unregisterEventNotifications: actorBridge("unregisterEventNotifications", {
    // Explicitly enumerate the arguments
    // @see ProfilerActor#startProfiler
    request: {
      events: Option(0, "nullable:array:string"),
    },
    response: RetVal("json")
  }),

  /**
   * Pipe events from Profiler module to this actor.
   */
  _onProfilerEvent: function (eventName, ...data) {
    events.emit(this, eventName, ...data);
  },
});

/**
 * This can be used on older Profiler implementations, but the methods cannot
 * be changed -- you must introduce a new method, and detect the server.
 */
exports.ProfilerFront = protocol.FrontClass(ProfilerActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this.actorID = form.profilerActor;
    this.manage(this);

    this._onProfilerEvent = this._onProfilerEvent.bind(this);
    events.on(this, "*", this._onProfilerEvent);
  },

  destroy: function () {
    events.off(this, "*", this._onProfilerEvent);
    protocol.Front.prototype.destroy.call(this);
  },

  /**
   * If using the protocol.js Fronts, then make stringify default,
   * since the read/write mechanisms will expose it as an object anyway, but
   * this lets other consumers who connect directly (xpcshell tests, Gecko Profiler) to
   * have unchanged behaviour.
   */
  getProfile: custom(function (options) {
    return this._getProfile(extend({ stringify: true }, options));
  }, {
    impl: "_getProfile"
  }),

  /**
   * Also emit an old `eventNotification` for older consumers of the profiler.
   */
  _onProfilerEvent: function (eventName, data) {
    // If this event already passed through once, don't repropagate
    if (data.relayed) {
      return;
    }
    data.relayed = true;

    // If this is `eventNotification`, this is coming from an older Gecko (<Fx42)
    // that doesn't use protocol.js style events. Massage it to emit a protocol.js
    // style event as well.
    if (eventName === "eventNotification") {
      events.emit(this, data.topic, data);
    }
    // Otherwise if a modern protocol.js event, emit it also as `eventNotification`
    // for compatibility reasons on the client (like for any add-ons/Gecko Profiler using this
    // event) and log a deprecation message if there is a listener.
    else {
      this.conn.emit("eventNotification", {
        subject: data.subject,
        topic: data.topic,
        data: data.data,
        details: data.details
      });
      if (this.conn._getListeners("eventNotification").length) {
        Cu.reportError(`
          ProfilerActor's "eventNotification" on the DebuggerClient has been deprecated.
          Use the ProfilerFront found in "devtools/server/actors/profiler".`);
      }
    }
  },
});
