/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const {Arg, Option, RetVal, types} = protocol;

types.addDictType("test.option", {
  "attribute-1": "string",
  "attribute-2": "string",
  "attribute-3": "string",
  "attribute-4": "string",
  "attribute-5": "string",
  "attribute-6": "string",
  "attribute-7": "string",
  "attribute-9": "string",
  "attribute-10": "string",
});
const dampTestSpec = protocol.generateActorSpec({
  typeName: "dampTest",

  events: {
    "testEvent": { arg: Arg(0, "array:json") },
  },

  methods: {
    testMethod: {
      request: {
        arg: Arg(0, "array:json"),
        option: Option(1, "array:test.option"),
        arraySize: Arg(2, "number"),
      },
      response: { value: RetVal("array:json") },
    },
  }
});
exports.dampTestSpec = dampTestSpec;
