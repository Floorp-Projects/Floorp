/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test Page.addScriptToEvaluateOnNewDocument and Page.removeScriptToEvaluateOnNewDocument

const DOC = toDataURL("default-test-page");
const WORLD = "testWorld";

// TODO Bug 1601695 - Add support for `source` parameter
add_task(async function addScript({ client }) {
  const { Page, Runtime } = client;
  await loadURL(DOC);
  const { identifier: id1 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
  });
  is(typeof id1, "string", "Script id should be a string");
  ok(id1.length > 0, "Script id is non-empty");
  const { identifier: id2 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
  });
  ok(id2.length > 0, "Script id is non-empty");
  isnot(id1, id2, "Two scripts should have different ids");
  await Runtime.enable();
  // flush event for DOC default context
  await Runtime.executionContextCreated();
  await checkIsolatedContextAfterLoad(Runtime, toDataURL("<p>Hello"), []);
});

add_task(async function addScriptAfterNavigation({ client }) {
  const { Page } = client;
  await loadURL(DOC);
  const { identifier: id1 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
  });
  is(typeof id1, "string", "Script id should be a string");
  ok(id1.length > 0, "Script id is non-empty");
  await loadURL(toDataURL("<p>Hello"));
  const { identifier: id2 } = await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 2;",
  });
  ok(id2.length > 0, "Script id is non-empty");
  isnot(id1, id2, "Two scripts should have different ids");
});

add_task(async function addWithIsolatedWorldAndNavigate({ client }) {
  const { Page, Runtime } = client;
  Page.enable();
  const loadEventFired = Page.loadEventFired();
  const contextsCreated = recordContextCreated(Runtime, 3);
  await Runtime.enable();
  info(`Navigating to ${DOC}`);
  const { frameId } = await Page.navigate({ url: DOC });
  await loadEventFired;
  await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
    worldName: WORLD,
  });
  const isolatedId = await Page.createIsolatedWorld({
    frameId,
    worldName: WORLD,
    grantUniversalAccess: true,
  });
  // flush context-created events for the steps above
  await contextsCreated;
  const contexts = await checkIsolatedContextAfterLoad(
    Runtime,
    toDataURL("<p>Next")
  );
  isnot(contexts[1].id, isolatedId, "The context has a new id");
});

add_task(async function addWithIsolatedWorldNavigateTwice({ client }) {
  const { Page, Runtime } = client;
  await Runtime.enable();
  await Page.addScriptToEvaluateOnNewDocument({
    source: "1 + 1;",
    worldName: WORLD,
  });
  await checkIsolatedContextAfterLoad(Runtime, DOC);
  await checkIsolatedContextAfterLoad(Runtime, toDataURL("<p>Hello"));
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
  await checkIsolatedContextAfterLoad(Runtime, DOC, names);
});

function recordContextCreated(Runtime, expectedCount) {
  return new Promise(resolve => {
    const ctx = [];
    const unsubscribe = Runtime.executionContextCreated(payload => {
      ctx.push(payload.context);
      info(
        `Runtime.executionContextCreated: ${payload.context.auxData.type}` +
          `\n\turl ${payload.context.origin}`
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

async function checkIsolatedContextAfterLoad(Runtime, url, names = [WORLD]) {
  // At least the default context will get created
  const expected = names.length + 1;
  const contextsCreated = recordContextCreated(Runtime, expected);
  info(`Navigating to ${url}`);
  await loadURL(url);
  const contexts = await contextsCreated;
  is(contexts.length, expected, "Expected number of contexts got created");
  is(contexts[0].auxData.isDefault, true, "Got default context");
  is(contexts[0].auxData.type, "default", "Got default context");
  is(contexts[0].name, "", "Get context with empty name");
  names.forEach((name, index) => {
    is(contexts[index + 1].name, name, "Get context with expected name");
    is(contexts[index + 1].auxData.isDefault, false, "Got isolated context");
    is(contexts[index + 1].auxData.type, "isolated", "Got isolated context");
  });
  return contexts;
}
