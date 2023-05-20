/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getMarionetteCommandsActorProxy,
  registerCommandsActor,
  unregisterCommandsActor,
} = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs"
);
const { enableEventsActor, disableEventsActor } = ChromeUtils.importESModule(
  "chrome://remote/content/marionette/actors/MarionetteEventsParent.sys.mjs"
);

registerCleanupFunction(function () {
  unregisterCommandsActor();
  disableEventsActor();
});

add_task(function test_commandsActor_register() {
  registerCommandsActor();
  unregisterCommandsActor();

  registerCommandsActor();
  registerCommandsActor();
  unregisterCommandsActor();
});

add_task(async function test_commandsActor_getActorProxy_noBrowsingContext() {
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
});

add_task(function test_eventsActor_enable_disable() {
  enableEventsActor();
  disableEventsActor();

  enableEventsActor();
  enableEventsActor();
  disableEventsActor();
});
