/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UserContextManagerClass } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/UserContextManager.sys.mjs"
);

add_task(async function test_invalid() {
  const userContextManager = new UserContextManagerClass();

  // Check invalid types for hasUserContextId/getInternalIdById which expects
  // a string.
  for (const value of [null, undefined, 1, [], {}]) {
    is(userContextManager.hasUserContextId(value), false);
    is(userContextManager.getInternalIdById(value), null);
  }

  // Check an invalid value for hasUserContextId/getInternalIdById which expects
  // either "default" or a UUID from Services.uuid.generateUUID.
  is(userContextManager.hasUserContextId("foo"), false);
  is(userContextManager.getInternalIdById("foo"), null);

  // Check invalid types for getIdByInternalId which expects a number.
  for (const value of [null, undefined, "foo", [], {}]) {
    is(userContextManager.getIdByInternalId(value), null);
  }

  userContextManager.destroy();
});

add_task(async function test_default_context() {
  const userContextManager = new UserContextManagerClass();
  ok(
    userContextManager.hasUserContextId("default"),
    `Context id default is known by the manager`
  );
  ok(
    userContextManager.getUserContextIds().includes("default"),
    `Context id default is listed by the manager`
  );
  is(
    userContextManager.getInternalIdById("default"),
    0,
    "Default user context has the expected internal id"
  );

  userContextManager.destroy();
});

add_task(async function test_new_internal_contexts() {
  info("Create a new user context with ContextualIdentityService");
  const beforeInternalId =
    ContextualIdentityService.create("before").userContextId;

  info("Create the UserContextManager");
  const userContextManager = new UserContextManagerClass();

  const beforeContextId =
    userContextManager.getIdByInternalId(beforeInternalId);
  assertContextAvailable(userContextManager, beforeContextId, beforeInternalId);

  info("Create another user context with ContextualIdentityService");
  const afterInternalId =
    ContextualIdentityService.create("after").userContextId;
  const afterContextId = userContextManager.getIdByInternalId(afterInternalId);
  assertContextAvailable(userContextManager, afterContextId, afterInternalId);

  info("Delete both user contexts");
  ContextualIdentityService.remove(beforeInternalId);
  ContextualIdentityService.remove(afterInternalId);
  assertContextRemoved(userContextManager, afterContextId, afterInternalId);
  assertContextRemoved(userContextManager, beforeContextId, beforeInternalId);

  userContextManager.destroy();
});

add_task(async function test_create_remove_context() {
  const userContextManager = new UserContextManagerClass();

  for (const closeContextTabs of [true, false]) {
    info("Create two contexts via createContext");
    const userContextId1 = userContextManager.createContext();
    const internalId1 = userContextManager.getInternalIdById(userContextId1);
    assertContextAvailable(userContextManager, userContextId1);

    const userContextId2 = userContextManager.createContext();
    const internalId2 = userContextManager.getInternalIdById(userContextId2);
    assertContextAvailable(userContextManager, userContextId2);

    info("Create tabs in various user contexts");
    const url = "https://example.com/document-builder.sjs?html=tab";
    const tabDefault = await addTab(gBrowser, url);
    const tabContext1 = await addTab(gBrowser, url, {
      userContextId: internalId1,
    });
    const tabContext2 = await addTab(gBrowser, url, {
      userContextId: internalId2,
    });

    info("Remove the user context 1 via removeUserContext");
    userContextManager.removeUserContext(userContextId1, { closeContextTabs });

    assertContextRemoved(userContextManager, userContextId1, internalId1);
    if (closeContextTabs) {
      ok(!gBrowser.tabs.includes(tabContext1), "Tab context 1 is closed");
    } else {
      ok(gBrowser.tabs.includes(tabContext1), "Tab context 1 is not closed");
    }
    ok(gBrowser.tabs.includes(tabDefault), "Tab default is not closed");
    ok(gBrowser.tabs.includes(tabContext2), "Tab context 2 is not closed");

    info("Remove the user context 2 via removeUserContext");
    userContextManager.removeUserContext(userContextId2, { closeContextTabs });
    assertContextRemoved(userContextManager, userContextId2, internalId2);
    if (closeContextTabs) {
      ok(!gBrowser.tabs.includes(tabContext2), "Tab context 2 is closed");
    } else {
      ok(gBrowser.tabs.includes(tabContext2), "Tab context 2 is not closed");
    }
    ok(gBrowser.tabs.includes(tabDefault), "Tab default is not closed");
  }

  userContextManager.destroy();
});

add_task(async function test_create_context_prefix() {
  const userContextManager = new UserContextManagerClass();

  info("Create a context with a custom prefix via createContext");
  const userContextId = userContextManager.createContext("test_prefix");
  const internalId = userContextManager.getInternalIdById(userContextId);
  const identity =
    ContextualIdentityService.getPublicIdentityFromId(internalId);
  ok(
    identity.name.startsWith("test_prefix"),
    "The new identity used the provided prefix"
  );

  userContextManager.removeUserContext(userContextId);
  userContextManager.destroy();
});

add_task(async function test_several_managers() {
  const manager1 = new UserContextManagerClass();
  const manager2 = new UserContextManagerClass();

  info("Create a context via manager1");
  const contextId1 = manager1.createContext();
  const internalId = manager1.getInternalIdById(contextId1);
  assertContextUnknown(manager2, contextId1);

  info("Retrieve the corresponding user context id in manager2");
  const contextId2 = manager2.getIdByInternalId(internalId);
  is(
    typeof contextId2,
    "string",
    "manager2 has a valid id for the user context created by manager 1"
  );

  ok(
    contextId1 != contextId2,
    "manager1 and manager2 have different ids for the same internal context id"
  );

  info("Remove the user context via manager2");
  manager2.removeUserContext(contextId2);

  info("Check that the user context is removed from both managers");
  assertContextRemoved(manager1, contextId1, internalId);
  assertContextRemoved(manager2, contextId2, internalId);

  manager1.destroy();
  manager2.destroy();
});

function assertContextAvailable(manager, contextId, expectedInternalId = null) {
  ok(
    manager.getUserContextIds().includes(contextId),
    `Context id ${contextId} is listed by the manager`
  );
  ok(
    manager.hasUserContextId(contextId),
    `Context id ${contextId} is known by the manager`
  );

  const internalId = manager.getInternalIdById(contextId);
  if (expectedInternalId != null) {
    is(internalId, expectedInternalId, "Internal id has the expected value");
  }

  is(
    typeof internalId,
    "number",
    `Context id ${contextId} corresponds to a valid internal id (${internalId})`
  );
  is(
    manager.getIdByInternalId(internalId),
    contextId,
    `Context id ${contextId} is returned for internal id ${internalId}`
  );
  ok(
    ContextualIdentityService.getPublicUserContextIds().includes(internalId),
    `User context for context id ${contextId} is found by ContextualIdentityService`
  );
}

function assertContextUnknown(manager, contextId) {
  ok(
    !manager.getUserContextIds().includes(contextId),
    `Context id ${contextId} is not listed by the manager`
  );
  ok(
    !manager.hasUserContextId(contextId),
    `Context id ${contextId} is not known by the manager`
  );
  is(
    manager.getInternalIdById(contextId),
    null,
    `Context id ${contextId} does not match any internal id`
  );
}

function assertContextRemoved(manager, contextId, internalId) {
  assertContextUnknown(manager, contextId);
  is(
    manager.getIdByInternalId(internalId),
    null,
    `Internal id ${internalId} cannot be converted to user context id`
  );
  ok(
    !ContextualIdentityService.getPublicUserContextIds().includes(internalId),
    `Internal id ${internalId} is not found in ContextualIdentityService`
  );
}
