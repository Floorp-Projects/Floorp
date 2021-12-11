/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  openToolboxAndLog,
  closeToolboxAndLog,
  isFissionEnabled,
  testSetup,
  testTeardown,
  COMPLICATED_URL,
} = require("damp-test/tests/head");
const {
  reloadConsoleAndLog,
} = require("damp-test/tests/webconsole/webconsole-helpers");
const { AppConstants } = require("resource://gre/modules/AppConstants.jsm");

const EXPECTED_MESSAGES = [
  {
    text: `This page uses the non standard property “zoom”`,
    count: isFissionEnabled() ? 1 : 2,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `Layout was forced before the page was fully loaded.`,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `Some cookies are misusing the “SameSite“ attribute, so it won’t work as expected`,
    visibleWhenFissionEnabled: true,
    nightlyOnly: true,
  },
  {
    text: `Uncaught DOMException: XMLHttpRequest.send: XMLHttpRequest state must be OPENED.`,
    visibleWhenFissionEnabled: true,
  },
  {
    text: `Uncaught SyntaxError: missing ) after argument list`,
    count: 2,
    visibleWhenFissionEnabled: false,
  },
  {
    text: `Uncaught ReferenceError: Bootloaddisableder is not defined`,
    count: 4,
    visibleWhenFissionEnabled: false,
  },
].filter(
  ({ visibleWhenFissionEnabled, nightlyOnly }) =>
    (!isFissionEnabled() || visibleWhenFissionEnabled) &&
    (!nightlyOnly || AppConstants.NIGHTLY_BUILD)
);

module.exports = async function() {
  // Disable overlay scrollbars to display the message "Layout was forced before the page was fully loaded"
  // consistently. See Bug 1684963.
  Services.prefs.setIntPref("ui.useOverlayScrollbars", 0);

  await testSetup(COMPLICATED_URL);

  let toolbox = await openToolboxAndLog("complicated.webconsole", "webconsole");
  await reloadConsoleAndLog("complicated", toolbox, EXPECTED_MESSAGES);
  await closeToolboxAndLog("complicated.webconsole", toolbox);

  // Restore (most likely delete) the overlay scrollbars preference.
  Services.prefs.clearUserPref("ui.useOverlayScrollbars");

  await testTeardown();
};
