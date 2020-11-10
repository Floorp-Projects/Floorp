/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher:
    "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
  registerEventsActor:
    "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
  unregisterEventsActor:
    "chrome://marionette/content/actors/MarionetteEventsParent.jsm",
});

registerCleanupFunction(function() {
  unregisterEventsActor();
});

add_test(function test_registering_event_actor() {
  registerEventsActor();
  unregisterEventsActor();

  registerEventsActor();
  registerEventsActor();
  unregisterEventsActor();

  run_next_test();
});
