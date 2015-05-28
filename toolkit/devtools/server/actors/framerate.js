/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const Services = require("Services");
const events = require("sdk/event/core");
const protocol = require("devtools/server/protocol");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils.js");

const {on, once, off, emit} = events;
const {method, custom, Arg, Option, RetVal} = protocol;

/**
 * A very simple utility for monitoring framerate.
 */
let FramerateActor = exports.FramerateActor = protocol.ActorClass({
  typeName: "framerate",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._contentWin = tabActor.window;
    this._onRefreshDriverTick = this._onRefreshDriverTick.bind(this);
    this._onGlobalCreated = this._onGlobalCreated.bind(this);
    on(this.tabActor, "window-ready", this._onGlobalCreated);
  },
  destroy: function(conn) {
    off(this.tabActor, "window-ready", this._onGlobalCreated);
    protocol.Actor.prototype.destroy.call(this, conn);
    this.stopRecording();
  },

  /**
   * Starts monitoring framerate, storing the frames per second.
   */
  startRecording: method(function() {
    if (this._recording) {
      return;
    }
    this._recording = true;
    this._ticks = [];
    this._startTime = this.tabActor.docShell.now();
    this._rafID = this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
  }, {
  }),

  /**
   * Stops monitoring framerate, returning the recorded values.
   */
  stopRecording: method(function(beginAt = 0, endAt = Number.MAX_SAFE_INTEGER) {
    if (!this._recording) {
      return [];
    }
    let ticks = this.getPendingTicks(beginAt, endAt);
    this.cancelRecording();
    return ticks;
  }, {
    request: {
      beginAt: Arg(0, "nullable:number"),
      endAt: Arg(1, "nullable:number")
    },
    response: { ticks: RetVal("array:number") }
  }),

  /**
   * Stops monitoring framerate, without returning the recorded values.
   */
  cancelRecording: method(function() {
    this._contentWin.cancelAnimationFrame(this._rafID);
    this._recording = false;
    this._ticks = null;
    this._rafID = -1;
  }, {
  }),

  /**
   * Returns whether this actor is currently active.
   */
  isRecording: method(function() {
    return !!this._recording;
  }, {
    response: { recording: RetVal("boolean") }
  }),

  /**
   * Gets the refresh driver ticks recorded so far.
   */
  getPendingTicks: method(function(beginAt = 0, endAt = Number.MAX_SAFE_INTEGER) {
    if (!this._ticks) {
      return [];
    }
    return this._ticks.filter(e => e >= beginAt && e <= endAt);
  }, {
    request: {
      beginAt: Arg(0, "nullable:number"),
      endAt: Arg(1, "nullable:number")
    },
    response: { ticks: RetVal("array:number") }
  }),

  /**
   * Function invoked along with the refresh driver.
   */
  _onRefreshDriverTick: function() {
    if (!this._recording) {
      return;
    }
    this._rafID = this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
    this._ticks.push(this.tabActor.docShell.now() - this._startTime);
  },

  /**
   * When the content window for the tab actor is created.
   */
  _onGlobalCreated: function (win) {
    if (this._recording) {
      this._rafID = this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
    }
  }
});

/**
 * The corresponding Front object for the FramerateActor.
 */
let FramerateFront = exports.FramerateFront = protocol.FrontClass(FramerateActor, {
  initialize: function(client, { framerateActor }) {
    protocol.Front.prototype.initialize.call(this, client, { actor: framerateActor });
    this.manage(this);
  }
});
