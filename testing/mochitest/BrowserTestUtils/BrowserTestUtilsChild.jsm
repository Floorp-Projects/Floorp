/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTestUtilsChild"];

class BrowserTestUtilsChild extends JSWindowActorChild {
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "BrowserTestUtils:CrashFrame": {
        // This is to intentionally crash the frame.
        // We crash by using js-ctypes and dereferencing
        // a bad pointer. The crash should happen immediately
        // upon loading this frame script.

        const { ctypes } = ChromeUtils.import(
          "resource://gre/modules/ctypes.jsm"
        );

        let dies = function() {
          ChromeUtils.privateNoteIntentionalCrash();
          let zero = new ctypes.intptr_t(8);
          let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
          badptr.contents;
        };

        dump("\nEt tu, Brute?\n");
        dies();
      }
    }
  }
}
