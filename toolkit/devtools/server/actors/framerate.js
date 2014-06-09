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
   * Stops monitoring framerate, returning the recorded values.
   */
  stopRecording: method(function() {
    if (!this._recording) {
      return [];
    }
    this._recording = false;

    // We don't need to store the ticks array for future use, release it.
    let ticks = this._ticks;
    this._ticks = null;
    return ticks;
  }, {
    response: { timeline: RetVal("array:number") }
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
  },

  /**
   * Plots the frames per second on a timeline.
   *
   * @param array ticks
   *        The raw data received from the framerate actor, which represents
   *        the elapsed time on each refresh driver tick.
   * @param number interval
   *        The maximum amount of time to wait between calculations.
   * @return array
   *         A collection of { delta, value } objects representing the
   *         framerate value at every delta time.
   */
  plotFPS: function(ticks, interval = 100) {
    let timeline = [];
    let totalTicks = ticks.length;

    // If the refresh driver didn't get a chance to tick before the
    // recording was stopped, assume framerate was 0.
    if (totalTicks == 0) {
      timeline.push({ delta: 0, value: 0 });
      timeline.push({ delta: interval, value: 0 });
      return timeline;
    }

    let frameCount = 0;
    let prevTime = ticks[0];

    for (let i = 1; i < totalTicks; i++) {
      let currTime = ticks[i];
      frameCount++;

      let elapsedTime = currTime - prevTime;
      if (elapsedTime < interval) {
        continue;
      }

      let framerate = 1000 / (elapsedTime / frameCount);
      timeline.push({ delta: prevTime, value: framerate });
      timeline.push({ delta: currTime, value: framerate });

      frameCount = 0;
      prevTime = currTime;
    }

    return timeline;
  }
});
