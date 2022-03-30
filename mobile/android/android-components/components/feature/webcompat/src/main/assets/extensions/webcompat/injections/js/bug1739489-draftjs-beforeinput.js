"use strict";

/**
 * Bug 1739489 - Entering an emoji using the MacOS IME "crashes" Draft.js editors.
 */

/* globals exportFunction */

console.info(
  "textInput event has been remapped to beforeinput for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1739489 for details."
);

window.wrappedJSObject.TextEvent = window.wrappedJSObject.InputEvent;

const { CustomEvent, Event, EventTarget } = window.wrappedJSObject;
var Remapped = [
  [CustomEvent, "constructor"],
  [Event, "constructor"],
  [Event, "initEvent"],
  [EventTarget, "addEventListener"],
  [EventTarget, "removeEventListener"],
];

for (const [obj, name] of Remapped) {
  const { prototype } = obj;
  const orig = prototype[name];
  Object.defineProperty(prototype, name, {
    value: exportFunction(function(type, b, c, d) {
      if (type?.toLowerCase() === "textinput") {
        type = "beforeinput";
      }
      return orig.call(this, type, b, c, d);
    }, window),
  });
}
