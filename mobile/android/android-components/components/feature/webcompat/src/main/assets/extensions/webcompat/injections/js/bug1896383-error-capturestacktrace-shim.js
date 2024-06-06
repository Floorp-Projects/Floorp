/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1896383 - Shim "captureStackTrace" in Error
 * Webcompat issue #136865 - https://github.com/webcompat/web-bugs/issues/136865
 *
 * The site is using Error.captureStackTrace and completely breaks in Firefox
 * since it's not supported. Depends on https://bugzilla.mozilla.org/show_bug.cgi?id=1886820
 */

/* globals exportFunction */

console.info(
  "Shimming captureStackTrace in Error for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1896383 for details."
);

Object.defineProperty(window.Error.wrappedJSObject, "captureStackTrace", {
  value: exportFunction(function () {
    return {};
  }, window),
});
