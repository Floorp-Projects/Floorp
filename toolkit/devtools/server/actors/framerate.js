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

exports.register = function(handle) {
  handle.addTabActor(FramerateActor, "framerateActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(FramerateActor);
};

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
  },
  destroy: function(conn) {
    protocol.Actor.prototype.destroy.call(this, conn);
    this.finalize();
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

    this._startTime = this._contentWin.performance.now();
    this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);
  }, {
  }),

  /**
   * Stops monitoring framerate, returning the recorded values at an
   * interval defined by the specified resolution, in milliseconds.
   */
  stopRecording: method(function(resolution = 100) {
    if (!this._recording) {
      return {};
    }
    this._recording = false;

    let timeline = {};
    let ticks = this._ticks;
    let totalTicks = ticks.length;

    // We don't need to store the ticks array for future use, release it.
    this._ticks = null;

    // If the refresh driver didn't get a chance to tick before the
    // recording was stopped, assume framerate was 0.
    if (totalTicks == 0) {
      timeline[resolution] = 0;
      return timeline;
    }

    let pivotTick = 0;
    let lastTick = ticks[totalTicks - 1];

    for (let bucketTime = resolution; bucketTime < lastTick; bucketTime += resolution) {
      let frameCount = 0;
      while (ticks[pivotTick++] < bucketTime) frameCount++;

      let framerate = 1000 / (bucketTime / frameCount);
      timeline[bucketTime] = framerate;
    }

    return timeline;
  }, {
    request: { resolution: Arg(0, "nullable:number") },
    response: { timeline: RetVal("json") }
  }),

  /**
   * Function invoked along with the refresh driver.
   */
  _onRefreshDriverTick: function() {
    if (!this._recording) {
      return;
    }
    this._contentWin.requestAnimationFrame(this._onRefreshDriverTick);

    // Store the amount of time passed since the recording started.
    let currentTime = this._contentWin.performance.now();
    let elapsedTime = currentTime - this._startTime;
    this._ticks.push(elapsedTime);
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
