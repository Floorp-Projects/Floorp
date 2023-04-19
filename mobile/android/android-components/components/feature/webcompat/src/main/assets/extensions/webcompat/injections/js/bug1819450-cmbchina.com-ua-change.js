/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1819450 - cmbchina.com - Override UA
 *
 * The site is using UA detection to redirect to
 * m.cmbchina.com (mobile version of the site). Adding `SAMSUNG` allows
 * to bypass the detection of mobile browser.
 */

/* globals exportFunction */

console.info(
  "The user agent has been overridden for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1081239 for details."
);

const MODIFIED_UA = navigator.userAgent + " SAMSUNG";

Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
  get: exportFunction(function() {
    return MODIFIED_UA;
  }, window),

  set: exportFunction(function() {}, window),
});
