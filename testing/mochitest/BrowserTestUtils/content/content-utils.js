/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

addEventListener("DOMContentLoaded", function(event) {
  let subframe = event.target != content.document;
  // For error page, internalURL is 'about:neterror?...', and visibleURL
  // is the original URL.
  sendAsyncMessage("browser-test-utils:DOMContentLoadedEvent",
    {subframe, internalURL: event.target.documentURI,
     visibleURL: content.document.location.href});
}, true);

addEventListener("load", function(event) {
  let subframe = event.target != content.document;
  sendAsyncMessage("browser-test-utils:loadEvent",
    {subframe, internalURL: event.target.documentURI,
     visibleURL: content.document.location.href});
}, true);
