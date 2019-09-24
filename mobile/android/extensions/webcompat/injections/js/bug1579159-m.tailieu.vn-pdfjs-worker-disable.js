"use strict";

/**
 * m.tailieu.vn - Override PDFJS.disableWorker to be true
 * WebCompat issue #39057 - https://webcompat.com/issues/39057
 *
 * Custom viewer built with PDF.js is not working in Firefox for Android
 * Disabling worker to match Chrome behavior fixes the issue
 */

/* globals exportFunction */

console.info(
  "window.PDFJS.disableWorker has been set to true for compatibility reasons. See https://webcompat.com/issues/39057 for details."
);

let globals = {};

Object.defineProperty(window.wrappedJSObject, "PDFJS", {
  get: exportFunction(function() {
    return globals;
  }, window),

  set: exportFunction(function(value = {}) {
    globals = value;
    globals.disableWorker = true;
  }, window),
});
