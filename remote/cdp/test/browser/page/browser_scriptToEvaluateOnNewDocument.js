/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test Page.addScriptToEvaluateOnNewDocument and Page.removeScriptToEvaluateOnNewDocument
//
// TODO Bug 1601695 - Schedule script evaluation and check for correct frame id

const WORLD = "testWorld";

add_task(async function uniqueIdForAddedScripts({ client }) {
  const { Page, Runtime } = client;

  await loadURL(PAGE_URL);

  const { identifier: id1 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
  });
  is(typeof id1, "string", "Script id should be a string");
  ok(id1.length, "Script id is non-empty");

  const { identifier: id2 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
  });
  ok(id2.length, "Script id is non-empty");
  isnot(id1, id2, "Two scripts should have different ids");

  await Runtime.enable();

  // flush event for PAGE_URL default context
  await Runtime.executionContextCreated();
  await checkIsolatedContextAfterLoad(client, PAGE_FRAME_URL, []);
});

add_task(async function addScriptAfterNavigation({ client }) {
  const { Page } = client;

  await loadURL(PAGE_URL);

  const { identifier: id1 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
  });
  is(typeof id1, "string", "Script id should be a string");
  ok(id1.length, "Script id is non-empty");

  await loadURL(PAGE_FRAME_URL);

  const { identifier: id2 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 2;",
  });
  ok(id2.length, "Script id is non-empty");
  isnot(id1, id2, "Two scripts should have different ids");
});

add_task(async function addWithIsolatedWorldAndNavigate({ client }) {
  const { Page, Runtime } = client;

  await Page.enable();
  await Runtime.enable();

  const contextsCreated = recordContextCreated(Runtime, 3);

  const loadEventFired = Page.loadEventFired();
  const { frameId } = await Page.navigate({ url: PAGE_URL });
  await loadEventFired;

  // flush context-created events for the steps above
  await contextsCreated;

  await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
    worldName: WORLD,
  });

  const isolatedId = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD,
    grantUniversalAccess: true,
  });

  const contexts = await checkIsolatedContextAfterLoad(client, PAGE_FRAME_URL);
  isnot(contexts[1].id, isolatedId, "The context has a new id");
});

add_task(async function addWithIsolatedWorldNavigateTwice({ client }) {
  const { Page, Runtime } = client;

  await Runtime.enable();

  await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
    worldName: WORLD,
  });

  await checkIsolatedContextAfterLoad(client, PAGE_URL);
  await checkIsolatedContextAfterLoad(client, PAGE_FRAME_URL);
});

add_task(async function addTwoScriptsWithIsolatedWorld({ client }) {
  const { Page, Runtime } = client;

  await Runtime.enable();

  const names = [WORLD, "A_whole_new_world"];
  await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
    worldName: names[0],
  });
  await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 8;",
    worldName: names[1],
  });

  await checkIsolatedContextAfterLoad(client, PAGE_URL, names);
});

function recordContextCreated(Runtime, expectedCount) {
  return new Promise(resolve => {
    const ctx = [];
    const unsubscribe = Runtime.executionContextCreated(payload => {
      ctx.push(payload.context);
      info(
        `Runtime.executionContextCreated: ${payload.context.auxData.type} ` +
          `(${payload.context.origin})`
      );
      if (ctx.length > expectedCount) {
        unsubscribe();
        resolve(ctx);
      }
    });
    timeoutPromise(1000).then(() => {
      unsubscribe();
      resolve(ctx);
    });
  });
}

async function checkIsolatedContextAfterLoad(client, url, names = [WORLD]) {
  const { Page, Runtime } = client;

  await Page.enable();

  // At least the default context will get created
  const expected = names.length + 1;

  const contextsCreated = recordContextCreated(Runtime, expected);
  const frameNavigated = Page.frameNavigated();
  const { frameId } = await Page.navigate({ url });
  await frameNavigated;
  const contexts = await contextsCreated;

  is(contexts.length, expected, "Expected number of contexts got created");
  is(contexts[0].auxData.frameId, frameId, "Expected frame id found");
  is(contexts[0].auxData.isDefault, true, "Got default context");
  is(contexts[0].auxData.type, "default", "Got default context");
  is(contexts[0].name, "", "Get context with empty name");

  names.forEach((name, index) => {
    is(contexts[index + 1].name, name, "Get context with expected name");
    is(contexts[index + 1].auxData.frameId, frameId, "Expected frame id found");
    is(contexts[index + 1].auxData.isDefault, false, "Got isolated context");
    is(contexts[index + 1].auxData.type, "isolated", "Got isolated context");
  });

  return contexts;
}
