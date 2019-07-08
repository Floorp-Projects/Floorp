"use strict";

/**
 * Bug 1472075 - Build UA override for Bank of America for OSX & Linux
 * WebCompat issue #2787 - https://webcompat.com/issues/2787
 *
 * BoA is showing a red warning to Linux and macOS users, while accepting
 * Windows users without warning. From our side, there is no difference here
 * and we receive a lot of user complains about the warnings, so we spoof
 * as Firefox on Windows in those cases.
 */

/* globals exportFunction */

if (!navigator.platform.includes("Win")) {
  console.info(
    "The user agent has been overridden for compatibility reasons. See https://webcompat.com/issues/2787 for details."
  );

  const WINDOWS_UA = navigator.userAgent.replace(
    /\(.*; rv:/i,
    "(Windows NT 10.0; Win64; x64; rv:"
  );

  Object.defineProperty(window.navigator.wrappedJSObject, "userAgent", {
    get: exportFunction(function() {
      return WINDOWS_UA;
    }, window),

    set: exportFunction(function() {}, window),
  });

  Object.defineProperty(window.navigator.wrappedJSObject, "appVersion", {
    get: exportFunction(function() {
      return "appVersion";
    }, window),

    set: exportFunction(function() {}, window),
  });

  Object.defineProperty(window.navigator.wrappedJSObject, "platform", {
    get: exportFunction(function() {
      return "Win64";
    }, window),

    set: exportFunction(function() {}, window),
  });
}
