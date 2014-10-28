/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Many Gecko operations (painting, reflows, restyle, ...) can be tracked
 * in real time. A marker is a representation of one operation. A marker
 * has a name, and start and end timestamps. Markers are stored in docShells.
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
 *
 */

const {Ci, Cu} = require("chrome");
const protocol = require("devtools/server/protocol");
const {method, Arg, RetVal, Option} = protocol;
const events = require("sdk/event/core");
const {setTimeout, clearTimeout} = require("sdk/timers");
const {MemoryActor} = require("devtools/server/actors/memory");
const {FramerateActor} = require("devtools/server/actors/framerate");

// How often do we pull markers from the docShells, and therefore, how often do
// we send events to the front (knowing that when there are no markers in the
// docShell, no event is sent).
const DEFAULT_TIMELINE_DATA_PULL_TIMEOUT = 200; // ms

/**
 * The timeline actor pops and forwards timeline markers registered in docshells.
 */
let TimelineActor = exports.TimelineActor = protocol.ActorClass({
  typeName: "timeline",

  events: {
    /**
     * "markers" events are emitted every DEFAULT_TIMELINE_DATA_PULL_TIMEOUT ms
     * at most, when profile markers are found. A marker has the following
     * properties:
     * - start {Number} ms
     * - end {Number} ms
     * - name {String}
     */
    "markers" : {
      type: "markers",
      markers: Arg(0, "array:json"),
      endTime: Arg(1, "number")
    },

    /**
     * "memory" events emitted in tandem with "markers", if this was enabled
     * when the recording started.
     */
    "memory" : {
      type: "memory",
      delta: Arg(0, "number"),
      measurement: Arg(1, "json")
    },

    /**
     * "ticks" events (from the refresh driver) emitted in tandem with "markers",
     * if this was enabled when the recording started.
     */
    "ticks" : {
      type: "ticks",
      delta: Arg(0, "number"),
      timestamps: Arg(1, "array:number")
    }
  },

  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;

    this._isRecording = false;
    this._startTime = 0;

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
   * markers if any.
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

    if (markers.length > 0) {
      this._postProcessMarkers(markers);
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
   * Some markers need post processing.
   * We will eventually do that platform side: bug 1069661
   */
  _postProcessMarkers: function(m) {
    m.forEach(m => {
      // A marker named "ConsoleTime:foobar" needs
      // to be renamed "ConsoleTime".
      let split = m.name.match(/ConsoleTime:(.*)/);
      if (split && split.length > 0) {
        if (!m.detail) {
          m.detail = {}
        }
        m.detail.causeName = split[1];
        m.name = "ConsoleTime";
      }
    });
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

    for (let docShell of this.docShells) {
      docShell.recordProfileTimelineMarkers = true;
    }

    if (withMemory) {
      this._memoryActor = new MemoryActor(this.conn, this.tabActor);
      events.emit(this, "memory", this._startTime, this._memoryActor.measure());
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
  }, {}),

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
