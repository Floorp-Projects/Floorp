/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;

function Quitter() {
}

Quitter.prototype = {
  toString: function() { return "[Quitter]"; },
  quit: function() {
    sendSyncMessage('Quitter.Quit', {});
  },
  __exposedProps__: {
    'toString': 'r',
    'quit': 'r'
  }
};

// This is a frame script, so it may be running in a content process.
// In any event, it is targeted at a specific "tab", so we listen for
// the DOMWindowCreated event to be notified about content windows
// being created in this context.

function QuitterManager() {
  addEventListener("DOMWindowCreated", this, false);
}

QuitterManager.prototype = {
  handleEvent: function handleEvent(aEvent) {
    var window = aEvent.target.defaultView;
    window.wrappedJSObject.Quitter = new Quitter(window);
  }
};

var quittermanager = new QuitterManager();
