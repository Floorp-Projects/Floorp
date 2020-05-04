/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test Page.createIsolatedWorld

const DOC = toDataURL("default-test-page");
const WORLD_NAME_1 = "testWorld1";
const WORLD_NAME_2 = "testWorld2";
const WORLD_NAME_3 = "testWorld3";
const DESTROYED = "Runtime.executionContextDestroyed";
const CREATED = "Runtime.executionContextCreated";
const CLEARED = "Runtime.executionContextsCleared";

add_task(async function createContextNoRuntimeDomain({ client }) {
  const { Page } = client;
  const { frameId } = await Page.navigate({ url: DOC });
  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME_1,
    grantUniversalAccess: true,
  });
  ok(typeof isolatedId == "number", "Page.createIsolatedWorld returns an id");
});

add_task(async function createContextRuntimeDisabled({ client }) {
  const { Runtime, Page } = client;
  const history = recordEvents(Runtime, 0);
  await Runtime.disable();
  info("Runtime notifications are disabled");
  const { frameId } = await Page.navigate({ url: DOC });
  await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME_2,
    grantUniversalAccess: true,
  });
  await assertEventOrder({ history, expectedEvents: [] });
});

add_task(async function contextCreatedAfterNavigation({ client }) {
  const { Runtime, Page } = client;
  const history = recordEvents(Runtime, 4);
  await Runtime.enable();
  info("Runtime notifications are enabled");
  await Page.enable();
  const loadEvent = Page.loadEventFired();
  info("Page notifications are enabled");
  info("Navigating...");
  const { frameId } = await Page.navigate({ url: DOC });
  await loadEvent;

  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME_3,
    grantUniversalAccess: true,
  });
  await assertEventOrder({
    history,
    expectedEvents: [
      CREATED, // default, about:blank
      DESTROYED, // default, about:blank
      CREATED, // default, DOC
      CREATED, // isolated, DOC
    ],
    timeout: 2000,
  });
  const defaultContext = history.events[2].payload.context;
  const isolatedContext = history.events[3].payload.context;
  is(defaultContext.auxData.isDefault, true, "Default context is default");
  is(
    defaultContext.auxData.type,
    "default",
    "Default context has type 'default'"
  );
  is(defaultContext.origin, DOC, "Default context has expected origin");
  checkIsolated(isolatedContext, isolatedId, WORLD_NAME_3);
  compareContexts(isolatedContext, defaultContext);
});

add_task(async function contextDestroyedAfterNavigation({ client }) {
  const { Runtime, Page } = client;
  const { isolatedId, defaultContext } = await setupContexts(Page, Runtime);
  is(defaultContext.auxData.isDefault, true, "Default context is default");
  const history = recordEvents(Runtime, 4, true);
  await Page.enable();
  const frameNavigated = Page.frameNavigated();
  info("Page notifications are enabled");
  info("Navigating...");
  await Page.navigate({ url: DOC });
  await frameNavigated;

  await assertEventOrder({
    history,
    expectedEvents: [
      DESTROYED, // default, about:blank
      DESTROYED, // isolated, about:blank
      CLEARED,
      CREATED, // default, DOC
    ],
    timeout: 2000,
  });
  const destroyed = [
    history.events[0].payload.executionContextId,
    history.events[1].payload.executionContextId,
  ];
  ok(destroyed.includes(isolatedId), "Isolated context destroyed");
  ok(destroyed.includes(defaultContext.id), "Default context destroyed");

  const newContext = history.events[3].payload.context;
  is(newContext.auxData.isDefault, true, "The new context is a default one");
  ok(!!newContext.id, "The new context has an id");
  ok(
    ![isolatedId, defaultContext.id].includes(newContext.id),
    "The new context has a new id"
  );
});

add_task(async function evaluateInIsolatedAndDefault({ client }) {
  const { Runtime, Page } = client;
  const { isolatedContext, defaultContext } = await setupContexts(
    Page,
    Runtime
  );

  const { result: objDefault } = await Runtime.evaluate({
    contextId: defaultContext.id,
    expression: "({ foo: 1 })",
  });
  const { result: objIsolated } = await Runtime.evaluate({
    contextId: isolatedContext.id,
    expression: "({ foo: 10 })",
  });
  const { result: result1 } = await Runtime.callFunctionOn({
    executionContextId: isolatedContext.id,
    functionDeclaration: "arg => ++arg.foo",
    arguments: [{ objectId: objIsolated.objectId }],
  });
  is(result1.value, 11, "Isolated context incremented the expected value");
  let errorThrown = "";
  try {
    await Runtime.callFunctionOn({
      executionContextId: isolatedContext.id,
      functionDeclaration: "arg => ++arg.foo",
      arguments: [{ objectId: objDefault.objectId }],
    });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(
    errorThrown.match(/Could not find object with given id/),
    "Contexts do not share objects"
  );
});

add_task(async function contextEvaluationIsIsolated({ client }) {
  const { Runtime, Page } = client;
  // If a document makes changes to standard global object, an isolated
  // world should not be affected
  await loadURL(toDataURL("<script>window.Node = null</script>"));
  const { isolatedContext, defaultContext } = await setupContexts(
    Page,
    Runtime
  );
  const { result: result1 } = await Runtime.callFunctionOn({
    executionContextId: defaultContext.id,
    functionDeclaration: "arg => window.Node",
  });
  const { result: result2 } = await Runtime.callFunctionOn({
    executionContextId: isolatedContext.id,
    functionDeclaration: "arg => window.Node",
  });
  is(result1.value, null, "Default context sees content changes to global");
  todo_isnot(
    result2.value,
    null,
    "Isolated context is not affected by changes to global, Bug 1601421"
  );
});

function checkIsolated(context, expectedId, expectedName) {
  ok(!!context.id, "Isolated context has an id");
  ok(!!context.origin, "Isolated context has an origin");
  ok(!!context.auxData.frameId, "Isolated context has a frameId");
  is(context.name, expectedName, "Isolated context is named as requested");
  is(
    expectedId,
    context.id,
    "createIsolatedWorld returns id of isolated context"
  );
  is(context.auxData.isDefault, false, "Isolated context is not default");
  is(context.auxData.type, "isolated", "Isolated context has type 'isolated'");
}

function compareContexts(isolatedContext, defaultContext) {
  isnot(
    defaultContext.name,
    isolatedContext.name,
    "The contexts have different names"
  );
  isnot(
    defaultContext.id,
    isolatedContext.id,
    "The contexts have different ids"
  );
  is(
    defaultContext.origin,
    isolatedContext.origin,
    "The contexts have same origin"
  );
  is(
    defaultContext.auxData.frameId,
    isolatedContext.auxData.frameId,
    "The contexts have same frameId"
  );
}

async function setupContexts(Page, Runtime) {
  const defaultContextCreated = Runtime.executionContextCreated();
  await Runtime.enable();
  info("Runtime notifications are enabled");
  const defaultContext = (await defaultContextCreated).context;
  const isolatedContextCreated = Runtime.executionContextCreated();
  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId: defaultContext.auxData.frameId,
    worldName: WORLD_NAME_1,
    grantUniversalAccess: true,
  });
  const isolatedContext = (await isolatedContextCreated).context;
  info("Isolated world created");
  checkIsolated(isolatedContext, isolatedId, WORLD_NAME_1);
  compareContexts(isolatedContext, defaultContext);
  return { isolatedContext, defaultContext, isolatedId };
}

function recordEvents(Runtime, total, cleared = false) {
  const history = new RecordEvents(total);

  history.addRecorder({
    event: Runtime.executionContextDestroyed,
    eventName: DESTROYED,
    messageFn: payload => {
      return `Received ${DESTROYED} for id ${payload.executionContextId}`;
    },
  });
  history.addRecorder({
    event: Runtime.executionContextCreated,
    eventName: CREATED,
    messageFn: payload => {
      return (
        `Received ${CREATED} for id ${payload.context.id}` +
        ` type: ${payload.context.auxData.type}` +
        ` name: ${payload.context.name}` +
        ` origin: ${payload.context.origin}`
      );
    },
  });
  if (cleared) {
    history.addRecorder({
      event: Runtime.executionContextsCleared,
      eventName: CLEARED,
    });
  }

  return history;
}

async function assertEventOrder(options = {}) {
  const { history, expectedEvents, timeout = 1000 } = options;
  const events = await history.record(timeout);
  const eventNames = events.map(item => item.eventName);
  info(`Expected events: ${expectedEvents}`);
  info(`Received events: ${eventNames}`);
  is(
    expectedEvents.length,
    events.length,
    "Received expected number of Runtime context events"
  );
  Assert.deepEqual(
    eventNames,
    expectedEvents,
    "Received Runtime context events in expected order"
  );
}
