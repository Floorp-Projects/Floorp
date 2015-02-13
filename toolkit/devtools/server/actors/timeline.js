/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Many Gecko operations (painting, reflows, restyle, ...) can be tracked
 * in real time. A marker is a representation of one operation. A marker
 * has a name, start and end timestamps. Markers are stored in docShells.
 *
 * This actor exposes this tracking mechanism to the devtools protocol.
 *
 * To start/stop recording markers:
 *   TimelineFront.start()
 *   TimelineFront.stop()
 *   TimelineFront.isRecording()
 *
 * When markers are available, an event is emitted:
 *   TimelineFront.on("markers", function(markers) {...})
 */

const {Ci, Cu} = require("chrome");
const protocol = require("devtools/server/protocol");
const {method, Arg, RetVal, Option} = protocol;
const events = require("sdk/event/core");
const {setTimeout, clearTimeout} = require("sdk/timers");

const {MemoryActor} = require("devtools/server/actors/memory");
const {FramerateActor} = require("devtools/server/actors/framerate");
const {StackFrameCache} = require("devtools/server/actors/utils/stack");

// How often do we pull markers from the docShells, and therefore, how often do
// we send events to the front (knowing that when there are no markers in the
// docShell, no event is sent).
const DEFAULT_TIMELINE_DATA_PULL_TIMEOUT = 200; // ms

/**
 * Type representing an array of numbers as strings, serialized fast(er).
 * http://jsperf.com/json-stringify-parse-vs-array-join-split/3
 *
 * XXX: It would be nice if on local connections (only), we could just *give*
 * the array directly to the front, instead of going through all this
 * serialization redundancy.
 */
protocol.types.addType("array-of-numbers-as-strings", {
  write: (v) => v.join(","),
  // In Gecko <= 37, `v` is an array; do not transform in this case.
  read: (v) => typeof v === "string" ? v.split(",") : v
});

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
let TimelineActor = exports.TimelineActor = protocol.ActorClass({
  typeName: "timeline",

  events: {
    /**
     * The "markers" events emitted every DEFAULT_TIMELINE_DATA_PULL_TIMEOUT ms
     * at most, when profile markers are found. The timestamps on each marker
     * are relative to when recording was started.
     */
    "markers" : {
      type: "markers",
      markers: Arg(0, "json"),
      endTime: Arg(1, "number")
    },

    /**
     * The "memory" events emitted in tandem with "markers", if this was enabled
     * when the recording started. The `delta` timestamp on this measurement is
     * relative to when recording was started.
     */
    "memory" : {
      type: "memory",
      delta: Arg(0, "number"),
      measurement: Arg(1, "json")
    },

    /**
     * The "ticks" events (from the refresh driver) emitted in tandem with
     * "markers", if this was enabled when the recording started. All ticks
     * are timestamps with a zero epoch.
     */
    "ticks" : {
      type: "ticks",
      delta: Arg(0, "number"),
      timestamps: Arg(1, "array-of-numbers-as-strings")
    },

    /**
     * The "frames" events emitted in tandem with "markers", containing
     * JS stack frames. The `delta` timestamp on this frames packet is
     * relative to when recording was started.
     */
    "frames" : {
      type: "frames",
      delta: Arg(0, "number"),
      frames: Arg(1, "json")
    }
  },

  /**
   * Initializes this actor with the provided connection and tab actor.
   */
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;

    this._isRecording = false;
    this._startTime = 0;
    this._stackFrames = null;

    // Make sure to get markers from new windows as they become available
    this._onWindowReady = this._onWindowReady.bind(this);
    events.on(this.tabActor, "window-ready", this._onWindowReady);
  },

  /**
   * The timeline actor is the first (and last) in its hierarchy to use protocol.js
   * so it doesn't have a parent protocol actor that takes care of its lifetime.
   * So it needs a disconnect method to cleanup.
   */
  disconnect: function() {
    this.destroy();
  },

  /**
   * Destroys this actor, stopping recording first.
   */
  destroy: function() {
    this.stop();

    events.off(this.tabActor, "window-ready", this._onWindowReady);
    this.tabActor = null;

    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Get the list of docShells in the currently attached tabActor. Note that we
   * always list the docShells included in the real root docShell, even if the
   * tabActor was switched to a child frame. This is because for now, paint
   * markers are only recorded at parent frame level so switching the timeline
   * to a child frame would hide all paint markers.
   * See https://bugzilla.mozilla.org/show_bug.cgi?id=1050773#c14
   * @return {Array}
   */
  get docShells() {
    let originalDocShell;

    if (this.tabActor.isRootActor) {
      originalDocShell = this.tabActor.docShell;
    } else {
      originalDocShell = this.tabActor.originalDocShell;
    }

    let docShellsEnum = originalDocShell.getDocShellEnumerator(
      Ci.nsIDocShellTreeItem.typeAll,
      Ci.nsIDocShell.ENUMERATE_FORWARDS
    );

    let docShells = [];
    while (docShellsEnum.hasMoreElements()) {
      let docShell = docShellsEnum.getNext();
      docShells.push(docShell.QueryInterface(Ci.nsIDocShell));
    }

    return docShells;
  },

  /**
   * At regular intervals, pop the markers from the docshell, and forward
   * markers, memory, tick and frames events, if any.
   */
  _pullTimelineData: function() {
    if (!this._isRecording) {
      return;
    }
    if (!this.docShells.length) {
      return;
    }

    let endTime = this.docShells[0].now();
    let markers = [];

    for (let docShell of this.docShells) {
      markers = [...markers, ...docShell.popProfileTimelineMarkers()];
    }

    // The docshell may return markers with stack traces attached.
    // Here we transform the stack traces via the stack frame cache,
    // which lets us preserve tail sharing when transferring the
    // frames to the client.  We must waive xrays here because Firefox
    // doesn't understand that the Debugger.Frame object is safe to
    // use from chrome.  See Tutorial-Alloc-Log-Tree.md.
    for (let marker of markers) {
      if (marker.stack) {
        marker.stack = this._stackFrames.addFrame(Cu.waiveXrays(marker.stack));
      }
      if (marker.endStack) {
        marker.endStack = this._stackFrames.addFrame(Cu.waiveXrays(marker.endStack));
      }
    }

    let frames = this._stackFrames.makeEvent();
    if (frames) {
      events.emit(this, "frames", endTime, frames);
    }
    if (markers.length > 0) {
      events.emit(this, "markers", markers, endTime);
    }
    if (this._memoryActor) {
      events.emit(this, "memory", endTime, this._memoryActor.measure());
    }
    if (this._framerateActor) {
      events.emit(this, "ticks", endTime, this._framerateActor.getPendingTicks());
    }

    this._dataPullTimeout = setTimeout(() => {
      this._pullTimelineData();
    }, DEFAULT_TIMELINE_DATA_PULL_TIMEOUT);
  },

  /**
   * Are we recording profile markers currently?
   */
  isRecording: method(function() {
    return this._isRecording;
  }, {
    request: {},
    response: {
      value: RetVal("boolean")
    }
  }),

  /**
   * Start recording profile markers.
   */
  start: method(function({ withMemory, withTicks }) {
    if (this._isRecording) {
      return;
    }
    this._isRecording = true;
    this._startTime = this.docShells[0].now();
    this._stackFrames = new StackFrameCache();
    this._stackFrames.initFrames();

    for (let docShell of this.docShells) {
      docShell.recordProfileTimelineMarkers = true;
    }

    if (withMemory) {
      this._memoryActor = new MemoryActor(this.conn, this.tabActor, this._stackFrames);
    }

    if (withTicks) {
      this._framerateActor = new FramerateActor(this.conn, this.tabActor);
      this._framerateActor.startRecording();
    }

    this._pullTimelineData();
    return this._startTime;
  }, {
    request: {
      withMemory: Option(0, "boolean"),
      withTicks: Option(0, "boolean")
    },
    response: {
      value: RetVal("number")
    }
  }),

  /**
   * Stop recording profile markers.
   */
  stop: method(function() {
    if (!this._isRecording) {
      return;
    }
    this._isRecording = false;
    this._stackFrames = null;

    if (this._memoryActor) {
      this._memoryActor = null;
    }

    if (this._framerateActor) {
      this._framerateActor.stopRecording();
      this._framerateActor = null;
    }

    for (let docShell of this.docShells) {
      docShell.recordProfileTimelineMarkers = false;
    }

    clearTimeout(this._dataPullTimeout);
    return this.docShells[0].now();
  }, {
    response: {
      // Set as possibly nullable due to the end time possibly being
      // undefined during destruction
      value: RetVal("nullable:number")
    }
  }),

  /**
   * When a new window becomes available in the tabActor, start recording its
   * markers if we were recording.
   */
  _onWindowReady: function({window}) {
    if (this._isRecording) {
      let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell);
      docShell.recordProfileTimelineMarkers = true;
    }
  }
});

exports.TimelineFront = protocol.FrontClass(TimelineActor, {
  initialize: function(client, {timelineActor}) {
    protocol.Front.prototype.initialize.call(this, client, {actor: timelineActor});
    this.manage(this);
  },
  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },
});
