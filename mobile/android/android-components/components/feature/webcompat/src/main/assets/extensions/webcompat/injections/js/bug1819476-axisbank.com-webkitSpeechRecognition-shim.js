/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * axisbank.com - Shim webkitSpeechRecognition
 * WebCompat issue #117770 - https://webcompat.com/issues/117770
 *
 * The page with bank offerings is not loading options due to the
 * site relying on webkitSpeechRecognition, which is undefined in Firefox.
 * Shimming it to `class {}` makes the pages work.
 */

/* globals exportFunction */

console.info(
  "webkitSpeechRecognition was shimmed for compatibility reasons. See https://webcompat.com/issues/117770 for details."
);

Object.defineProperty(window.wrappedJSObject, "webkitSpeechRecognition", {
  value: exportFunction(function() {
    return class {};
  }, window),
});
