// This is the content script for mochitest-e10s-utils

// We hook up some events and forward them back to the parent for the tests
// This is only a partial solution to tests using these events - tests which
// check, eg, event.target is the content window are still likely to be
// confused.
// But it's a good start...
["load", "DOMContentLoaded", "pageshow"].forEach(eventName => {
  addEventListener(eventName, function eventHandler(event) {
    // Some tests also rely on load events from, eg, iframes, so we should see
    // if we can do something sane to support that too.
    if (event.target == content.document) {
      sendAsyncMessage("Test:Event", {name: event.type});
    }
  }, true);
});
