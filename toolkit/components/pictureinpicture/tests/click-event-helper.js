/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This helper script is used to record mouse button events for
 * Picture-in-Picture toggle click tests. Anytime the toggle is
 * clicked, we expect none of the events to be fired. Otherwise,
 * all events should be fired when clicking.
 */

let eventTypes = ["pointerdown", "mousedown", "pointerup", "mouseup", "click"];

for (let event of eventTypes) {
  addEventListener(event, recordEvent, { capture: true });
}

let recordedEvents = [];
function recordEvent(event) {
  recordedEvents.push(event.type);
}

function getRecordedEvents() {
  let result = recordedEvents.concat();
  recordedEvents = [];
  return result;
}
