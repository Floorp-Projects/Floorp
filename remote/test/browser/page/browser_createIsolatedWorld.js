/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test Page.createIsolatedWorld

const DOC = toDataURL("default-test-page");
const WORLD_NAME = "testWorld";

add_task(async function createContextNoRuntimeDomain({ Page }) {
  const { frameId } = await Page.navigate({ url: DOC });
  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME,
    grantUniversalAccess: true,
  });
  ok(typeof isolatedId == "number", "Page.createIsolatedWorld returns an id");
});

add_task(async function createContextRuntimeDisabled({ Runtime, Page }) {
  const { promises, resolutions } = recordEvents(Runtime, 0);
  await Runtime.disable();
  info("Runtime notifications are disabled");
  const { frameId } = await Page.navigate({ url: DOC });
  await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME,
    grantUniversalAccess: true,
  });
  await assertEventOrder(promises, resolutions, []);
});

add_task(async function contextCreatedAfterNavigation({ Page, Runtime }) {
  const { promises, resolutions } = recordEvents(Runtime, 4);
  await Runtime.enable();
  info("Runtime notifications are enabled");
  info("Navigating...");
  const { frameId } = await Page.navigate({ url: DOC });
  const { executionContextId: isolatedId } = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD_NAME,
    grantUniversalAccess: true,
  });
  await assertEventOrder(promises, resolutions, [
    "Runtime.executionContextCreated", // default, about:blank
    "Runtime.executionContextDestroyed", // default, about:blank
    "Runtime.executionContextCreated", // default, DOC
    "Runtime.executionContextCreated", // isolated, DOC
  ]);
  const defaultContext = resolutions[2].payload.context;
  const isolatedContext = resolutions[3].payload.context;
  is(defaultContext.auxData.isDefault, true, "Default context is default");
  is(
    defaultContext.auxData.type,
    "default",
    "Default context has type 'default'"
  );
  is(defaultContext.origin, DOC, "Default context has expected origin");
  checkIsolated(isolatedContext, isolatedId);
  compareContexts(isolatedContext, defaultContext);
});

add_task(async function contextDestroyedAfterNavigation({ Page, Runtime }) {
  const { isolatedId, defaultContext } = await setupContexts(Page, Runtime);
  is(defaultContext.auxData.isDefault, true, "Default context is default");
  const { promises, resolutions } = recordEvents(Runtime, 3);
  info("Navigating...");
  await Page.navigate({ url: DOC });

  await assertEventOrder(promises, resolutions, [
    "Runtime.executionContextDestroyed", // default, about:blank
    "Runtime.executionContextDestroyed", // isolated, about:blank
    "Runtime.executionContextCreated", // default, DOC
  ]);
  const destroyed = [
    resolutions[0].payload.executionContextId,
    resolutions[1].payload.executionContextId,
  ];
  ok(destroyed.includes(isolatedId), "Isolated context destroyed");
  ok(destroyed.includes(defaultContext.id), "Default context destroyed");

  const newContext = resolutions[2].payload.context;
  is(newContext.auxData.isDefault, true, "The new context is a default one");
  ok(!!newContext.id, "The new context has an id");
  ok(
    ![isolatedId, defaultContext.id].includes(newContext.id),
    "The new context has a new id"
  );
});

add_task(async function evaluateInIsolatedAndDefault({ Page, Runtime }) {
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
  let errorThrown;
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
    errorThrown.match(/Cannot find object with ID/),
    "Contexts do not share objects"
  );
});

add_task(async function contextEvaluationIsIsolated({ Page, Runtime }) {
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

function checkIsolated(context, expectedId) {
  ok(!!context.id, "Isolated context has an id");
  ok(!!context.origin, "Isolated context has an origin");
  ok(!!context.auxData.frameId, "Isolated context has a frameId");
  is(context.name, WORLD_NAME, "Isolated context is named as requested");
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
    worldName: WORLD_NAME,
    grantUniversalAccess: true,
  });
  const isolatedContext = (await isolatedContextCreated).context;
  info("Isolated world created");
  checkIsolated(isolatedContext, isolatedId);
  compareContexts(isolatedContext, defaultContext);
  return { isolatedContext, defaultContext, isolatedId };
}

function recordEvents(Runtime, total) {
  const resolutions = [];
  const promises = new Set();

  recordPromises(
    Runtime.executionContextDestroyed,
    "Runtime.executionContextDestroyed"
  );
  recordPromises(
    Runtime.executionContextCreated,
    "Runtime.executionContextCreated",
    "id"
  );

  function recordPromises(event, eventName, prop) {
    const promise = new Promise(resolve => {
      const unsubscribe = event(payload => {
        const propSuffix = prop ? ` for ${prop} ${payload.context[prop]}` : "";
        const message = `Received ${eventName} ${propSuffix}`;
        info(message);
        resolutions.push({ eventName, payload });

        if (resolutions.length > total) {
          unsubscribe();
          resolve();
        }
      });
    });
    promises.add(promise);
  }
  return { promises, resolutions };
}

async function assertEventOrder(promises, resolutions, expectedResolutions) {
  await Promise.race([Promise.all(promises), timeoutPromise(1000)]);
  Assert.deepEqual(
    resolutions.map(item => item.eventName),
    expectedResolutions,
    "Received Runtime context events in expected order"
  );
}
