/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

("use strict");

const EXPORTED_SYMBOLS = ["DampLoadParent", "EventDispatcher"];

const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);

// Global event emitter to avoid listening to individual JSWindowActor instances.
// For a single load, several pairs of JSWindowActors might be created in quick
// succession, so listening to events on individual actor instances is usually
// not enough to monitor a load.
const EventDispatcher = {};
EventEmitter.decorate(EventDispatcher);

// Simple JSWindow actor pair to listen to page show events.
class DampLoadParent extends JSWindowActorParent {
  async receiveMessage(msg) {
    const { name, data } = msg;
    if (name === "DampLoadChild:PageShow") {
      EventDispatcher.emit("DampLoadParent:PageShow", data);
    }
  }
}
