"use strict";

/* eslint-env mozilla/frame-script */

var {interfaces: Ci} = Components;

Components.utils.import("resource://gre/modules/Services.jsm");

Services.console.registerListener(function listener(message) {
  if (/WebExt Privilege Escalation/.test(message.message)) {
    Services.console.unregisterListener(listener);
    sendAsyncMessage("console-message", {message: message.message});
  }
});
