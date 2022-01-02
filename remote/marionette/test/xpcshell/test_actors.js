/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getMarionetteCommandsActorProxy,
  registerCommandsActor,
  unregisterCommandsActor,
} = ChromeUtils.import(
  "chrome://remote/content/marionette/actors/MarionetteCommandsParent.jsm"
);
const { enableEventsActor, disableEventsActor } = ChromeUtils.import(
  "chrome://remote/content/marionette/actors/MarionetteEventsParent.jsm"
);

registerCleanupFunction(function() {
  unregisterCommandsActor();
  disableEventsActor();
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

add_test(function test_eventsActor_enable_disable() {
  enableEventsActor();
  disableEventsActor();

  enableEventsActor();
  enableEventsActor();
  disableEventsActor();

  run_next_test();
});
