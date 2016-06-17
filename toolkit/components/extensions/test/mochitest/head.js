"use strict";

Components.utils.import("resource://gre/modules/Task.jsm");

/* exported waitForLoad, promiseConsoleOutput */

function waitForLoad(win) {
  return new Promise(resolve => {
    win.addEventListener("load", function listener() {
      win.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}

var promiseConsoleOutput = Task.async(function* (task) {
  const DONE = "=== extension test console listener done ===";

  let listener;
  let messages = [];
  let awaitListener = new Promise(resolve => {
    listener = msg => {
      if (msg == DONE) {
        resolve();
      } else if (msg instanceof Ci.nsIConsoleMessage) {
        messages.push(msg.message);
      }
    };
  });

  Services.console.registerListener(listener);
  try {
    let result = yield task();

    Services.console.logStringMessage(DONE);
    yield awaitListener;

    return {messages, result};
  } finally {
    Services.console.unregisterListener(listener);
  }
});
