/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");

const { dampTestSpec } = require("./spec");

exports.DampTestActor = protocol.ActorClassWithSpec(dampTestSpec, {
  initialize(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);
  },

  testMethod(arg, { option }, arraySize) {
    // Emit an event with second argument's option.
    this.emit("testEvent", option);

    // Returns back an array of repetition of first argument.
    return arg;
  },
});

