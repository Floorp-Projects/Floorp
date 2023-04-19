/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1448747 - Neutralize FastClick
 *
 * The patch is applied on sites using FastClick library
 * to make sure `FastClick.notNeeded` returns `true`.
 * This allows to disable FastClick and fix various breakage caused
 * by the library (mainly non-functioning drop-down lists).
 */

/* globals exportFunction */

(function() {
  const proto = CSS2Properties.prototype.wrappedJSObject;
  const descriptor = Object.getOwnPropertyDescriptor(proto, "touchAction");
  const { get } = descriptor;

  descriptor.get = exportFunction(function() {
    try {
      throw Error();
    } catch (e) {
      if (e.stack?.includes("notNeeded")) {
        return "none";
      }
    }
    return get.call(this);
  }, window);

  Object.defineProperty(proto, "touchAction", descriptor);
})();
