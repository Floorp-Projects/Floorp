/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("devtools/shared/protocol");
const { dampTestSpec } = require("damp-test/tests/server/spec");

class DampTestActor extends Actor {
  constructor(conn) {
    super(conn, dampTestSpec);
  }

  testMethod(arg, { option }) {
    // Emit an event with second argument's option.
    this.emit("testEvent", option);

    // Returns back an array of repetition of first argument.
    return arg;
  }
}
exports.DampTestActor = DampTestActor;
