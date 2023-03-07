"use strict";

/**
 * Bug 1818818 - Neutralize FastClick
 *
 * The patch is applied on sites using older version of FastClick library.
 * This allows to disable FastClick and fix various breakage caused
 * by the library.
 **/

/* globals exportFunction */

const proto = CSS2Properties.prototype.wrappedJSObject;
Object.defineProperty(proto, "msTouchAction", {
  get: exportFunction(function() {
    return "none";
  }, window),

  set: exportFunction(function() {}, window),
});
