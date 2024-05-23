/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PromptHandlerConfiguration,
  PromptHandlers,
  PromptTypes,
  UserPromptHandler,
} = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/UserPromptHandler.sys.mjs"
);

add_task(function test_PromptHandlerConfiguration_ctor() {
  const config = new PromptHandlerConfiguration("accept", true);
  equal(config.handler, "accept");
  equal(config.notify, true);
});

add_task(function test_PromptHandlerConfiguration_toString() {
  equal(
    new PromptHandlerConfiguration().toString(),
    "[object PromptHandlerConfiguration]"
  );
});

add_task(function test_PromptHandlerConfiguration_toJSON() {
  let configuration, serialized;

  for (const behavior of Object.values(PromptHandlers)) {
    console.log(`Test handler - ${behavior}`);
    configuration = new PromptHandlerConfiguration(behavior, false);
    serialized = configuration.toJSON();
    equal(serialized, behavior);

    configuration = new PromptHandlerConfiguration(behavior, true);
    serialized = configuration.toJSON();
    if ([PromptHandlers.Accept, PromptHandlers.Dismiss].includes(behavior)) {
      equal(serialized, `${behavior} and notify`);
    } else {
      equal(serialized, behavior);
    }
  }
});

add_task(function test_UserPromptHandler_ctor() {
  const handler = new UserPromptHandler();
  equal(handler.activePromptHandlers.size, 0);
});

add_task(function test_UserPromptHandler_toString() {
  equal(new UserPromptHandler().toString(), "[object UserPromptHandler]");
});

add_task(function test_UserPromptHandler_fromJSON() {
  let promptHandler;

  // Unhandled prompt behavior as string
  for (const behavior of Object.values(PromptHandlers)) {
    console.log(`Test as string for ${behavior}`);
    promptHandler = UserPromptHandler.fromJSON(behavior);
    equal(promptHandler.activePromptHandlers.size, 1);
    ok(promptHandler.activePromptHandlers.has("default"));
    const handler = promptHandler.activePromptHandlers.get("default");
    ok(behavior.startsWith(handler.handler));
    if (behavior == "ignore") {
      ok(handler.notify);
    } else {
      equal(handler.notify, /and notify/.test(behavior));
    }
  }

  // Unhandled prompt behavior as object
  for (const promptType of Object.values(PromptTypes)) {
    for (const behavior of Object.values(PromptHandlers)) {
      console.log(`Test as object for ${promptType} - ${behavior}`);
      promptHandler = UserPromptHandler.fromJSON({ [promptType]: behavior });
      equal(promptHandler.activePromptHandlers.size, 1);
      ok(promptHandler.activePromptHandlers.has(promptType));
      const handler = promptHandler.activePromptHandlers.get(promptType);
      ok(behavior.startsWith(handler.handler));
      if (behavior == "ignore") {
        ok(handler.notify);
      } else {
        equal(handler.notify, /and notify/.test(behavior));
      }
    }
  }
});

add_task(function test_UserPromptHandler_fromJSON_invalid() {
  for (const type of [
    undefined,
    null,
    true,
    42,
    [],
    "default",
    "beforeunload",
  ]) {
    Assert.throws(
      () => UserPromptHandler.fromJSON(type),
      /InvalidArgumentError/
    );
  }

  // Invalid types for prompt types and handlers
  for (const type of [undefined, null, true, 42, {}, []]) {
    Assert.throws(
      () => UserPromptHandler.fromJSON({ [type]: "accept" }),
      /InvalidArgumentError/
    );
    Assert.throws(
      () => UserPromptHandler.fromJSON({ alert: type }),
      /InvalidArgumentError/
    );
  }

  // Invalid values for prompt type and handlers
  Assert.throws(
    () => UserPromptHandler.fromJSON({ foo: "accept" }),
    /InvalidArgumentError/
  );
  Assert.throws(
    () => UserPromptHandler.fromJSON({ alert: "foo" }),
    /InvalidArgumentError/
  );
});

add_task(function test_UserPromptHandler_getPromptHandler() {
  let configuration, promptHandler;

  // Check the default value with no handlers defined
  promptHandler = new UserPromptHandler();
  equal(promptHandler.activePromptHandlers.size, 0);
  for (const promptType of Object.values(PromptTypes)) {
    console.log(`Test default behavior for ${promptType}`);
    const handler = promptHandler.getPromptHandler(promptType);
    if (promptType === PromptTypes.BeforeUnload) {
      equal(handler.handler, PromptHandlers.Accept);
      equal(handler.notify, false);
    } else {
      equal(handler.handler, PromptHandlers.Dismiss);
      equal(handler.notify, true);
    }
  }

  // Check custom default behavior for all prompt types
  promptHandler = new UserPromptHandler();
  configuration = new PromptHandlerConfiguration(PromptHandlers.Ignore, false);
  promptHandler.set(PromptTypes.Default, configuration);
  for (const promptType of Object.values(PromptTypes)) {
    console.log(`Test custom default behavior for ${promptType}`);
    equal(promptHandler.activePromptHandlers.size, 1);
    const handler = promptHandler.getPromptHandler(promptType);
    if (promptType === PromptTypes.BeforeUnload) {
      // For backward compatibility with WebDriver classic it is not overridden
      equal(handler.handler, PromptHandlers.Accept);
      equal(handler.notify, false);
    } else {
      equal(handler.handler, PromptHandlers.Ignore);
      equal(handler.notify, false);
    }
  }

  // Check custom behavior overrides default for all prompt types
  promptHandler = new UserPromptHandler();
  configuration = new PromptHandlerConfiguration(PromptHandlers.Ignore, false);
  promptHandler.set(PromptTypes.Default, configuration);
  for (const promptType of Object.values(PromptTypes)) {
    if (promptType === PromptTypes.Default) {
      continue;
    }

    console.log(`Test custom behavior overrides default for ${promptType}`);
    configuration = new PromptHandlerConfiguration(PromptHandlers.Accept, true);
    promptHandler.set(promptType, configuration);
    const handler = promptHandler.getPromptHandler(promptType);
    equal(handler.handler, PromptHandlers.Accept);
    equal(handler.notify, true);
  }
});

add_task(function test_UserPromptHandler_toJSON() {
  let configuration, promptHandler, serialized;

  // Default behavior
  promptHandler = new UserPromptHandler();
  serialized = promptHandler.toJSON();
  equal(serialized, "dismiss and notify");

  // Custom default behavior
  promptHandler = new UserPromptHandler();
  configuration = new PromptHandlerConfiguration(PromptHandlers.Accept, false);
  promptHandler.set(PromptTypes.Default, configuration);
  serialized = promptHandler.toJSON();
  equal(serialized, "accept");

  // Multiple handler definitions
  promptHandler = new UserPromptHandler();
  configuration = new PromptHandlerConfiguration(PromptHandlers.Ignore, false);
  promptHandler.set(PromptTypes.Default, configuration);
  configuration = new PromptHandlerConfiguration(PromptHandlers.Accept, true);
  promptHandler.set(PromptTypes.Alert, configuration);
  configuration = new PromptHandlerConfiguration(PromptHandlers.Dismiss, false);
  promptHandler.set(PromptTypes.Confirm, configuration);

  serialized = promptHandler.toJSON();
  deepEqual(serialized, {
    default: "ignore",
    alert: "accept and notify",
    confirm: "dismiss",
  });
});
