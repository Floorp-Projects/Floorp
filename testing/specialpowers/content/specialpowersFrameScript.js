"use strict";

/* globals attachSpecialPowersToWindow */

let mm = this;

ChromeUtils.import("resource://specialpowers/specialpowersAPI.js", this);
ChromeUtils.import("resource://specialpowers/specialpowers.js", this);

// This is a frame script, so it may be running in a content process.
// In any event, it is targeted at a specific "tab", so we listen for
// the DOMWindowCreated event to be notified about content windows
// being created in this context.

function SpecialPowersManager() {
  addEventListener("DOMWindowCreated", this, false);
}

SpecialPowersManager.prototype = {
  handleEvent: function handleEvent(aEvent) {
    var window = aEvent.target.defaultView;
    attachSpecialPowersToWindow(window, mm);
  }
};

var specialpowersmanager = new SpecialPowersManager();
