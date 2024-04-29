/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint max-len: ["error", 80] */
/* import-globals-from aboutaddonsCommon.js */
/* exported AbuseReporter, openAbuseReport */
/* global windowRoot */

/**
 * This script is part of the HTML about:addons page and it provides some
 * helpers used for abuse reports.
 */

const { AbuseReporter } = ChromeUtils.importESModule(
  "resource://gre/modules/AbuseReporter.sys.mjs"
);

async function openAbuseReport({ addonId }) {
  // TODO: `reportEntryPoint` is also passed to this function but we aren't
  // using it currently. Maybe we should?

  const amoUrl = AbuseReporter.getAMOFormURL({ addonId });
  windowRoot.ownerGlobal.openTrustedLinkIn(amoUrl, "tab", {
    // Make sure the newly open tab is going to be focused, independently
    // from general user prefs.
    forceForeground: true,
  });
}

window.openAbuseReport = openAbuseReport;
