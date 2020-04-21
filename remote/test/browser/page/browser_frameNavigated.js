/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Page navigation events

const INITIAL_DOC = toDataURL("default-test-page");
const RANDOM_ID_DOC = toDataURL(
  `<script>window.randomId = Math.random() + "-" + Date.now();</script>`
);

const promises = new Set();
const resolutions = new Map();

add_task(async function({ client }) {
  await loadURL(INITIAL_DOC);

  const { Page } = client;

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Page.enable();
  info("Page domain has been enabled");

  const { frameTree } = await Page.getFrameTree();

  // Save the given `promise` resolution into the `promises` global Set
  function recordPromise(name, promise) {
    promise.then(event => {
      info(`Received Page.${name}`);
      resolutions.set(name, event);
    });
    promises.add(promise);
  }
  // Record all Page events that we assert in this test
  function recordPromises() {
    recordPromise("frameStartedLoading", Page.frameStartedLoading());
    recordPromise("frameNavigated", Page.frameNavigated());
    recordPromise("domContentEventFired", Page.domContentEventFired());
    recordPromise("loadEventFired", Page.loadEventFired());
    recordPromise("navigatedWithinDocument", Page.navigatedWithinDocument());
    recordPromise("frameStoppedLoading", Page.frameStoppedLoading());
  }

  info("Test Page.navigate");
  recordPromises();

  const url = RANDOM_ID_DOC;
  const { frameId } = await Page.navigate({ url });
  info("A new page has been loaded");

  ok(frameId, "Page.navigate returned a frameId");
  is(
    frameId,
    frameTree.frame.id,
    "The Page.navigate's frameId is the same than getFrameTree's one"
  );

  await assertNavigationEvents({ url, frameId });

  const randomId1 = await getTestTabRandomId();
  ok(!!randomId1, "Test tab has a valid randomId");

  info("Test Page.reload");
  recordPromises();

  await Page.reload();
  info("The page has been reloaded");

  await assertNavigationEvents({ url, frameId });

  const randomId2 = await getTestTabRandomId();
  ok(!!randomId2, "Test tab has a valid randomId");
  isnot(
    randomId2,
    randomId1,
    "Test tab randomId has been updated after reload"
  );

  info("Test Page.navigate with the same URL still reloads the current page");
  recordPromises();

  await Page.navigate({ url });
  info("The page has been reloaded");

  await assertNavigationEvents({ url, frameId });

  const randomId3 = await getTestTabRandomId();
  ok(!!randomId3, "Test tab has a valid randomId");
  isnot(
    randomId3,
    randomId2,
    "Test tab randomId has been updated after reload"
  );
});

async function assertNavigationEvents({ url, frameId }) {
  // Wait for all the promises to resolve
  await Promise.all(promises);

  // Assert the order in which they resolved
  const expectedResolutions = [
    "frameStartedLoading",
    "frameNavigated",
    "domContentEventFired",
    "loadEventFired",
    "navigatedWithinDocument",
    "frameStoppedLoading",
  ];
  Assert.deepEqual(
    [...resolutions.keys()],
    expectedResolutions,
    "Received various Page navigation events in the expected order"
  );

  // Now assert the data exposed by each of these events
  const frameStartedLoading = resolutions.get("frameStartedLoading");
  is(
    frameStartedLoading.frameId,
    frameId,
    "frameStartedLoading frameId is the same one"
  );

  const frameNavigated = resolutions.get("frameNavigated");
  ok(
    !frameNavigated.frame.parentId,
    "frameNavigated is for the top level document and has a null parentId"
  );
  is(frameNavigated.frame.id, frameId, "frameNavigated id is the right one");
  is(
    frameNavigated.frame.name,
    undefined,
    "frameNavigated name isn't implemented yet"
  );
  is(frameNavigated.frame.url, url, "frameNavigated url is the right one");

  const navigatedWithinDocument = resolutions.get("navigatedWithinDocument");
  is(
    navigatedWithinDocument.frameId,
    frameId,
    "navigatedWithinDocument frameId is the same one"
  );
  is(
    navigatedWithinDocument.url,
    url,
    "navigatedWithinDocument url is the same one"
  );

  const frameStoppedLoading = resolutions.get("frameStoppedLoading");
  is(
    frameStoppedLoading.frameId,
    frameId,
    "frameStoppedLoading frameId is the same one"
  );

  promises.clear();
  resolutions.clear();
}

async function getTestTabRandomId() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    return content.wrappedJSObject.randomId;
  });
}
