/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "EventEmitter",
  "devtools/toolkit/event-emitter");

/**
 * A dummy front decorated with the provided methods.
 *
 * @param array blueprint
 *        A list of [funcName, retVal] describing the class.
 */
function MockFront (blueprint) {
  EventEmitter.decorate(this);

  for (let [funcName, retVal] of blueprint) {
    this[funcName] = (x => typeof x === "function" ? x() : x).bind(this, retVal);
  }
}

function MockTimelineFront () {
  MockFront.call(this, [
    ["destroy"],
    ["start", 0],
    ["stop", 0],
  ]);
}

/**
 * Takes a TabTarget, and checks existence of a TimelineActor on
 * the server, or if TEST_MOCK_TIMELINE_ACTOR is to be used.
 *
 * @param {TabTarget} target
 * @return {Boolean}
 */
function timelineActorSupported(target) {
  // This `target` property is used only in tests to test
  // instances where the timeline actor is not available.
  if (target.TEST_MOCK_TIMELINE_ACTOR) {
    return false;
  }

  return target.hasActor("timeline");
}

/**
 * Returns a function to be used as a method on an "Front" in ./actors.
 * Calls the underlying actor's method.
 */
function callFrontMethod (method) {
  return function () {
    // If there's no target or client on this actor facade,
    // abort silently -- this occurs in tests when polling occurs
    // after the test ends, when tests do not wait for toolbox destruction
    // (which will destroy the actor facade, turning off the polling).
    if (!this._target || !this._target.client) {
      return;
    }
    return this._front[method].apply(this._front, arguments);
  };
}

exports.MockTimelineFront = MockTimelineFront;
exports.timelineActorSupported = timelineActorSupported;
exports.callFrontMethod = callFrontMethod;
