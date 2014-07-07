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
  handle.addGlobalActor(FramerateActor, "framerateActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(FramerateActor);
  handle.removeGlobalActor(FramerateActor);
};

/**
 * A very simple utility for monitoring framerate.
 */
let FramerateActor = exports.FramerateActor = protocol.ActorClass({
  typeName: "framerate",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    this._chromeWin = getChromeWin(tabActor.window);
    this._onRefreshDriverTick = this._onRefreshDriverTick.bind(this);
  },
  destroy: function(conn) {
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

    this._startTime = this._chromeWin.performance.now();
    this._rafID = this._chromeWin.requestAnimationFrame(this._onRefreshDriverTick);
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
    this._chromeWin.cancelAnimationFrame(this._rafID);
    this._recording = false;
    this._ticks = null;
    this._rafID = -1;
  }, {
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
    this._rafID = this._chromeWin.requestAnimationFrame(this._onRefreshDriverTick);

    // Store the amount of time passed since the recording started.
    let currentTime = this._chromeWin.performance.now();
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

/**
 * Plots the frames per second on a timeline.
 *
 * @param array ticks
 *        The raw data received from the framerate actor, which represents
 *        the elapsed time on each refresh driver tick.
 * @param number interval
 *        The maximum amount of time to wait between calculations.
 * @param number clamp
 *        The maximum allowed framerate value.
 * @return array
 *         A collection of { delta, value } objects representing the
 *         framerate value at every delta time.
 */
FramerateFront.plotFPS = function(ticks, interval = 100, clamp = 60) {
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

    let framerate = Math.min(1000 / (elapsedTime / frameCount), clamp);
    timeline.push({ delta: prevTime, value: framerate });
    timeline.push({ delta: currTime, value: framerate });

    frameCount = 0;
    prevTime = currTime;
  }

  return timeline;
};

/**
 * Gets the top level browser window from a content window.
 *
 * @param nsIDOMWindow innerWin
 *        The content window to query.
 * @return nsIDOMWindow
 *         The top level browser window.
 */
function getChromeWin(innerWin) {
  return innerWin
    .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
}
