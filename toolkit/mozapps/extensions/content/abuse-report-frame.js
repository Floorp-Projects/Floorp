/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals promiseHtmlBrowserLoaded */

// Helper method exported into the about:addons global, used to open the
// abuse report panel from outside of the about:addons page
// (e.g. triggered from the browserAction context menu).
window.openAbuseReport = ({ addonId, reportEntryPoint }) => {
  promiseHtmlBrowserLoaded().then(browser => {
    browser.contentWindow.openAbuseReport({
      addonId,
      reportEntryPoint,
    });
  });
};
