/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test Page.createIsolatedWorld

const WORLD_NAME_1 = "testWorld1";
const WORLD_NAME_2 = "testWorld2";

const DESTROYED = "Runtime.executionContextDestroyed";
const CREATED = "Runtime.executionContextCreated";
const CLEARED = "Runtime.executionContextsCleared";

add_task(async function frameIdMissing({ client }) {
  const { Page } = client;

  await Assert.rejects(
    Page.createIsolatedWorld({
      worldName: WORLD_NAME_1,
      grantUniversalAccess: true,
    }),
    /frameId: string value expected/,
    `Fails with missing frameId`
  );
});

add_task(async function frameIdInvalidTypes({ client }) {
  const { Page } = client;

  for (const frameId of [null, true, 1, [], {}]) {
    await Assert.rejects(
      Page.createIsolatedWorld({
        frameId,
      }),
      /frameId: string value expected/,
      `Fails with invalid type: ${frameId}`
    );
  }
});

add_task(async function worldNameInvalidTypes({ client }) {
  const { Page } = client;

  await Page.enable();
  info("Page notifications are enabled");

  const loadEvent = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: PAGE_URL });
  await loadEvent;

  for (const worldName of [null, true, 1, [], {}]) {
    await Assert.rejects(
      Page.createIsolatedWorld({
        frameId,
        worldName,
      }),
      /worldName: string value expected/,
      `Fails with invalid type: ${worldName}`
    );
  }
});

add_task(async function noEventsWhenRuntimeDomainDisabled({ client }) {
  const { Page, Runtime } = client;

  await Page.enable();
  info("Page notifications are enabled");

  const history = recordEvents(Runtime, 0);
  const loadEvent = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: PAGE_URL });
  await loadEvent;

  let errorThrown = "";
  try {
    await Page.createIsolatedWorld({
      frameId,
      worldName: WORLD_NAME_1,
      grantUniversalAccess: true,
    });
    await assertEvents({ history, expectedEvents: [] });
  } catch (e) {
    errorThrown = e.message;
  }
  todo(
    errorThrown === "",
    "No contexts tracked internally without Runtime enabled (Bug 1623482)"
  );
});

add_task(async function noEventsAfterRuntimeDomainDisabled({ client }) {
  const { Page, Runtime } = client;

  await Page.enable();
  info("Page notifications are enabled");

  await enableRuntime(client);
  await Runtime.disable();
  info("Runtime notifications are disabled");

  const history = recordEvents(Runtime, 0);
  const loadEvent = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: PAGE_URL });
  await loadEvent;

  await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME_2,
    grantUniversalAccess: true,
  });
  await assertEvents({ history, expectedEvents: [] });
});

add_task(async function contextCreatedAfterNavigation({ client }) {
  const { Page, Runtime } = client;

  await Page.enable();
  info("Page notifications are enabled");

  await enableRuntime(client);

  const history = recordEvents(Runtime, 3);
  const loadEvent = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: PAGE_URL });
  await loadEvent;

  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME_1,
    grantUniversalAccess: true,
  });
  await assertEvents({
    history,
    expectedEvents: [
      DESTROYED, // default, about:blank
      CREATED, // default, PAGE_URL
      CREATED, // isolated, PAGE_URL
    ],
  });

  const contexts = history
    .findEvents(CREATED)
    .map(event => event.payload.context);
  const defaultContext = contexts[0];
  const isolatedContext = contexts[1];
  is(defaultContext.auxData.isDefault, true, "Default context is default");
  is(
    defaultContext.auxData.type,
    "default",
    "Default context has type 'default'"
  );
  is(defaultContext.origin, BASE_ORIGIN, "Default context has expected origin");
  checkIsolated(isolatedContext, isolatedId, WORLD_NAME_1, frameId);
  compareContexts(isolatedContext, defaultContext);
});

add_task(async function contextDestroyedForNavigation({ client }) {
  const { Page, Runtime } = client;

  const defaultContext = await enableRuntime(client);
  const isolatedContext = await createIsolatedContext(client, defaultContext);

  await Page.enable();

  const history = recordEvents(Runtime, 4, true);
  const frameNavigated = Page.frameNavigated();
  await Page.navigate({ url: PAGE_URL });
  await frameNavigated;

  await assertEvents({
    history,
    expectedEvents: [
      DESTROYED, // default, about:blank
      DESTROYED, // isolated, about:blank
      CLEARED,
      CREATED, // default, PAGE_URL
    ],
  });

  const destroyed = history
    .findEvents(DESTROYED)
    .map(event => event.payload.executionContextId);
  ok(destroyed.includes(isolatedContext.id), "Isolated context destroyed");
  ok(destroyed.includes(defaultContext.id), "Default context destroyed");

  const { context: newContext } = history.findEvent(CREATED).payload;
  is(newContext.auxData.isDefault, true, "The new context is a default one");
  ok(!!newContext.id, "The new context has an id");
  ok(
    ![defaultContext.id, isolatedContext.id].includes(newContext.id),
    "The new context has a new id"
  );
});

add_task(async function contextsForFramesetNavigation({ client }) {
  const { Page, Runtime } = client;

  await Page.enable();
  info("Page notifications are enabled");

  await enableRuntime(client);

  // check creation when navigating to a frameset
  const historyTo = recordEvents(Runtime, 5);
  const loadEventTo = Page.loadEventFired();
  const { frameId: frameIdTo } = await Page.navigate({
    url: FRAMESET_SINGLE_URL,
  });
  await loadEventTo;

  const { frameTree } = await Page.getFrameTree();
  const subFrame = frameTree.childFrames[0].frame;

  const { executionContextId: contextIdParent } =
    await Page.createIsolatedWorld({
      frameId: frameIdTo,
      worldName: WORLD_NAME_1,
      grantUniversalAccess: true,
    });
  const { executionContextId: contextIdSubFrame } =
    await Page.createIsolatedWorld({
      frameId: subFrame.id,
      worldName: WORLD_NAME_2,
      grantUniversalAccess: true,
    });

  await assertEvents({
    history: historyTo,
    expectedEvents: [
      DESTROYED, // default, about:blank
      CREATED, // default, FRAMESET_SINGLE_URL
      CREATED, // default, PAGE_URL
      CREATED, // isolated, FRAMESET_SINGLE_URL
      CREATED, // isolated, PAGE_URL
    ],
  });

  const contextsCreated = historyTo
    .findEvents(CREATED)
    .map(event => event.payload.context);
  const parentDefaultContextCreated = contextsCreated[0];
  const frameDefaultContextCreated = contextsCreated[1];
  const parentIsolatedContextCreated = contextsCreated[2];
  const frameIsolatedContextCreated = contextsCreated[3];

  checkIsolated(
    parentIsolatedContextCreated,
    contextIdParent,
    WORLD_NAME_1,
    frameIdTo
  );
  compareContexts(parentIsolatedContextCreated, parentDefaultContextCreated);

  checkIsolated(
    frameIsolatedContextCreated,
    contextIdSubFrame,
    WORLD_NAME_2,
    subFrame.id
  );
  compareContexts(frameIsolatedContextCreated, frameDefaultContextCreated);

  // check destroying when navigating away from a frameset
  const historyFrom = recordEvents(Runtime, 6);
  const loadEventFrom = Page.loadEventFired();
  await Page.navigate({ url: PAGE_URL });
  await loadEventFrom;

  await assertEvents({
    history: historyFrom,
    expectedEvents: [
      DESTROYED, // default, PAGE_URL
      DESTROYED, // isolated, PAGE_URL
      DESTROYED, // default, FRAMESET_SINGLE_URL
      DESTROYED, // isolated, FRAMESET_SINGLE_URL
      CREATED, // default, PAGE_URL
    ],
  });

  const contextsDestroyed = historyFrom
    .findEvents(DESTROYED)
    .map(event => event.payload.executionContextId);
  contextsCreated.forEach(context => {
    ok(
      contextsDestroyed.includes(context.id),
      `Context with id ${context.id} destroyed`
    );
  });

  const { context: newContext } = historyFrom.findEvent(CREATED).payload;
  is(newContext.auxData.isDefault, true, "The new context is a default one");
  ok(!!newContext.id, "The new context has an id");
  ok(
    ![parentDefaultContextCreated.id, frameDefaultContextCreated.id].includes(
      newContext.id
    ),
    "The new context has a new id"
  );
});

add_task(async function evaluateInIsolatedAndDefault({ client }) {
  const { Runtime } = client;

  const defaultContext = await enableRuntime(client);
  const isolatedContext = await createIsolatedContext(client, defaultContext);

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

  await Assert.rejects(
    Runtime.callFunctionOn({
      executionContextId: isolatedContext.id,
      functionDeclaration: "arg => ++arg.foo",
      arguments: [{ objectId: objDefault.objectId }],
    }),
    /Could not find object with given id/,
    "Contexts do not share objects"
  );
});

add_task(async function contextEvaluationIsIsolated({ client }) {
  const { Runtime } = client;

  // If a document makes changes to standard global object, an isolated
  // world should not be affected
  await loadURL(toDataURL("<script>window.Node = null</script>"));

  const defaultContext = await enableRuntime(client);
  const isolatedContext = await createIsolatedContext(client, defaultContext);

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

function checkIsolated(context, expectedId, expectedName, expectedFrameId) {
  is(
    expectedId,
    context.id,
    "createIsolatedWorld returns id of isolated context"
  );
  is(
    context.auxData.frameId,
    expectedFrameId,
    "Isolated context has expected frameId"
  );
  is(context.auxData.isDefault, false, "Isolated context is not default");
  is(context.auxData.type, "isolated", "Isolated context has type 'isolated'");
  is(context.name, expectedName, "Isolated context is named as requested");
  ok(!!context.origin, "Isolated context has an origin");
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

async function createIsolatedContext(
  client,
  defaultContext,
  worldName = WORLD_NAME_1
) {
  const { Page, Runtime } = client;

  const frameId = defaultContext.auxData.frameId;

  const isolatedContextCreated = Runtime.executionContextCreated();
  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId,
    worldName,
    grantUniversalAccess: true,
  });
  const { context: isolatedContext } = await isolatedContextCreated;
  info("Isolated world created");

  checkIsolated(isolatedContext, isolatedId, worldName, frameId);
  compareContexts(isolatedContext, defaultContext);

  return isolatedContext;
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

async function assertEvents(options = {}) {
  const { history, expectedEvents, timeout = 1000 } = options;
  const events = await history.record(timeout);
  const eventNames = events.map(item => item.eventName);
  info(`Expected events: ${expectedEvents}`);
  info(`Received events: ${eventNames}`);
  is(
    events.length,
    expectedEvents.length,
    "Received expected number of Runtime context events"
  );
  Assert.deepEqual(
    eventNames.sort(),
    expectedEvents.sort(),
    "Received expected Runtime context events"
  );
}
