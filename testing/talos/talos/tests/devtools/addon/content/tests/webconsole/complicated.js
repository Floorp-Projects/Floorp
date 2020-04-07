/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  openToolboxAndLog,
  closeToolboxAndLog,
  isFissionEnabled,
  testSetup,
  testTeardown,
  COMPLICATED_URL,
} = require("../head");
const { reloadConsoleAndLog } = require("./webconsole-helpers");

const EXPECTED_MESSAGES = [
  {
    text: `This page uses the non standard property “zoom”`,
    count: isFissionEnabled() ? 2 : 4,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `Layout was forced before the page was fully loaded.`,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `Some cookies are misusing the “sameSite“ attribute, so it won’t work as expected`,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `InvalidStateError: XMLHttpRequest state must be OPENED.`,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `SyntaxError: missing ) after argument list`,
    count: 2,
    visibleWhenFissionEnabled: false,
  },
  {
    text: `ReferenceError: Bootloaddisableder is not defined`,
    count: 4,
    visibleWhenFissionEnabled: false,
  },
].filter(
  ({ visibleWhenFissionEnabled }) =>
    !isFissionEnabled() || visibleWhenFissionEnabled
);

module.exports = async function() {
  await testSetup(COMPLICATED_URL);

  let toolbox = await openToolboxAndLog("complicated.webconsole", "webconsole");
  await reloadConsoleAndLog("complicated", toolbox, EXPECTED_MESSAGES);
  await closeToolboxAndLog("complicated.webconsole", toolbox);

  await testTeardown();
};
