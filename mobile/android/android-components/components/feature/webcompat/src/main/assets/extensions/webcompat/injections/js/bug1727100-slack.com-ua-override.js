"use strict";

/* globals exportFunction, UAHelpers */

UAHelpers.overrideWithDeviceAppropriateChromeUA("95.0.4638.54");

Object.defineProperty(window.navigator.wrappedJSObject, "vendor", {
  get: exportFunction(function() {
    return "Google Inc.";
  }, window),

  set: exportFunction(function() {}, window),
});
