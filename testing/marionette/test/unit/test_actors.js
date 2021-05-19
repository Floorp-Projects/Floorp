/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getMarionetteCommandsActorProxy,
  registerCommandsActor,
  unregisterCommandsActor,
} = ChromeUtils.import(
  "chrome://marionette/content/actors/MarionetteCommandsParent.jsm"
);
const {
  EventDispatcher,
  registerEventsActor,
  unregisterEventsActor,
} = ChromeUtils.import(
  "chrome://marionette/content/actors/MarionetteEventsParent.jsm"
);

registerCleanupFunction(function() {
  unregisterCommandsActor();
  unregisterEventsActor();
});

add_test(function test_commandsActor_register() {
  registerCommandsActor();
  unregisterCommandsActor();

  registerCommandsActor();
  registerCommandsActor();
  unregisterCommandsActor();

  run_next_test();
});

add_test(async function test_commandsActor_getActorProxy_noBrowsingContext() {
  registerCommandsActor();

  try {
    await getMarionetteCommandsActorProxy(() => null).sendQuery("foo", "bar");
    ok(false, "Expected NoBrowsingContext error not raised");
  } catch (e) {
    ok(
      e.message.includes("No BrowsingContext found"),
      "Expected default error message found"
    );
  }

  unregisterCommandsActor();

  run_next_test();
});

add_test(function test_eventsActor_register() {
  registerEventsActor();
  unregisterEventsActor();

  registerEventsActor();
  registerEventsActor();
  unregisterEventsActor();

  run_next_test();
});
