/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const path = require("path")

module.exports = function(results) {
  for (let file of results) {
    let filePath = path.relative(".", file.filePath);
    for (let message of file.messages) {
      let status = message.message;

      if ("ruleId" in message) {
        status = `${status} (${message.ruleId})`;
      }

      let severity = message.severity == 1 ? "TEST-UNEXPECTED-WARNING"
                                           : "TEST-UNEXPECTED-ERROR";
      console.log(`${severity} | ${filePath}:${message.line}:${message.column} | ${status}`);
    }
  }
};
