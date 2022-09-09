/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  testSetup,
  testTeardown,
  COMPLICATED_URL,
} = require("damp-test/tests/head");
const {
  reloadConsoleAndLog,
} = require("damp-test/tests/webconsole/webconsole-helpers");

// The virtualized list render all the messages that fit in the console output, and 20 more,
// so all the expected messages here should be rendered.
const EXPECTED_MESSAGES = [
  {
    text: `Uncaught SyntaxError: missing ) after argument list`,
    count: 2,
  },
  {
    text: `Uncaught ReferenceError: Bootloaddisableder is not defined`,
    count: 4,
    stacktrace: true,
  },
  {
    text: `Uncaught DOMException: XMLHttpRequest.send: XMLHttpRequest state must be OPENED`,
  },
];

module.exports = async function() {
  await testSetup(COMPLICATED_URL);

  // Disabling all filters but Errors, as they are more likely to be stable (unlike
  // warning messages which can be added more frequently as the platform evolves)
  const filtersToDisable = [
    "devtools.webconsole.filter.warn",
    "devtools.webconsole.filter.info",
    "devtools.webconsole.filter.log",
    "devtools.webconsole.filter.debug",
  ];
  for (const filter of filtersToDisable) {
    Services.prefs.setBoolPref(filter, false);
  }

  let toolbox = await openToolboxAndLog("complicated.webconsole", "webconsole");
  await reloadConsoleAndLog("complicated", toolbox, EXPECTED_MESSAGES);
  await closeToolboxAndLog("complicated.webconsole", toolbox);

  for (const filter of filtersToDisable) {
    Services.prefs.clearUserPref(filter);
  }

  await testTeardown();
};
