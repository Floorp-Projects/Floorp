/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

var Cu = Components.utils;

// Ideally we would use CrashTestUtils.jsm, but that's only available for
// xpcshell tests - so we just copy a ctypes crasher from it.
Cu.import("resource://gre/modules/ctypes.jsm");
var crash = function() { // this will crash when called.
  let zero = new ctypes.intptr_t(8);
  let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
  badptr.contents
};


var TestHelper = {
  init() {
    addMessageListener("thumbnails-test:crash", this);
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "thumbnails-test:crash":
        privateNoteIntentionalCrash();
        crash();
      break;
    }
  },
}

TestHelper.init();
