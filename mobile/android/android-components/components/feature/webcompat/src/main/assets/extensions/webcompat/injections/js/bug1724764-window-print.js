/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Generic window.print shim
 *
 * Issues related to an error caused by missing window.print() method on Android.
 * Adding print to the window object allows to unbreak the sites.
 */

/* globals exportFunction */

if (typeof window.print === "undefined") {
  console.info(
    "window.print has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1659818 for details."
  );

  Object.defineProperty(window.wrappedJSObject, "print", {
    get: exportFunction(function() {
      return true;
    }, window),

    set: exportFunction(function() {}, window),
  });
}
