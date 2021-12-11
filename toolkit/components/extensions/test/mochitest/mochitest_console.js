"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { addMessageListener, sendAsyncMessage } = this;

// Much of the console monitoring code is copied from TestUtils but simplified
// to our needs.
function monitorConsole(msgs) {
  function msgMatches(msg, pat) {
    for (let k in pat) {
      if (!(k in msg)) {
        return false;
      }
      if (pat[k] instanceof RegExp && typeof msg[k] === "string") {
        if (!pat[k].test(msg[k])) {
          return false;
        }
      } else if (msg[k] !== pat[k]) {
        return false;
      }
    }
    return true;
  }

  let counter = 0;
  function listener(msg) {
    if (msgMatches(msg, msgs[counter])) {
      counter++;
    }
  }
  addMessageListener("waitForConsole", () => {
    sendAsyncMessage("consoleDone", {
      ok: counter >= msgs.length,
      message: `monitorConsole | messages left expected at least ${msgs.length} got ${counter}`,
    });
    Services.console.unregisterListener(listener);
  });

  Services.console.registerListener(listener);
}

addMessageListener("consoleStart", messages => {
  for (let msg of messages) {
    // Message might be a RegExp object from a different compartment, but
    // instanceof RegExp will fail.  If we have an object, lets just make
    // sure.
    let message = msg.message;
    if (typeof message == "object" && !(message instanceof RegExp)) {
      msg.message = new RegExp(message);
    }
  }
  monitorConsole(messages);
});
