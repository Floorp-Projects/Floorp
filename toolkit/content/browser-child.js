/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

// This message is used to measure content process startup performance in Talos
// tests.
sendAsyncMessage("Content:BrowserChildReady", {
  time: Services.telemetry.msSystemNow(),
});
