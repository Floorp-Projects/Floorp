/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Many Gecko operations (painting, reflows, restyle, ...) can be tracked
 * in real time. A marker is a representation of one operation. A marker
 * has a name, and start and end timestamps. Markers are stored within
 * a docshell.
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
const {method, Arg, RetVal} = protocol;
const events = require("sdk/event/core");
const {setTimeout, clearTimeout} = require("sdk/timers");

const TIMELINE_DATA_PULL_TIMEOUT = 300;

exports.register = function(handle) {
  handle.addGlobalActor(TimelineActor, "timelineActor");
  handle.addTabActor(TimelineActor, "timelineActor");
};

exports.unregister = function(handle) {
  handle.removeGlobalActor(TimelineActor);
  handle.removeTabActor(TimelineActor);
};

/**
 * The timeline actor pops and forwards timeline markers registered in
 * a docshell.
 */
let TimelineActor = protocol.ActorClass({
  typeName: "timeline",

  events: {
    /**
     * "markers" events are emitted at regular intervals when profile markers
     * are found. A marker has the following properties:
     * - start {Number}
     * - end {Number}
     * - name {String}
     */
    "markers" : {
      type: "markers",
      markers: Arg(0, "array:json")
    }
  },

  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.docshell = tabActor.docShell;
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
    this.docshell = null;
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * At regular intervals, pop the markers from the docshell, and forward
   * markers if any.
   */
  _pullTimelineData: function() {
    let markers = this.docshell.popProfileTimelineMarkers();
    if (markers.length > 0) {
      events.emit(this, "markers", markers);
    }
    this._dataPullTimeout = setTimeout(() => this._pullTimelineData(),
                                       TIMELINE_DATA_PULL_TIMEOUT);
  },

  /**
   * Are we recording profile markers for the current docshell (window)?
   */
  isRecording: method(function() {
    return this.docshell.recordProfileTimelineMarkers;
  }, {
    request: {},
    response: {
      value: RetVal("boolean")
    }
  }),

  /**
   * Start/stop recording profile markers.
   */
  start: method(function() {
    if (!this.docshell.recordProfileTimelineMarkers) {
      this.docshell.recordProfileTimelineMarkers = true;
      this._pullTimelineData();
    }
  }, {oneway: true}),

  stop: method(function() {
    if (this.docshell.recordProfileTimelineMarkers) {
      this.docshell.recordProfileTimelineMarkers = false;
      clearTimeout(this._dataPullTimeout);
    }
  }, {oneway: true}),
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
