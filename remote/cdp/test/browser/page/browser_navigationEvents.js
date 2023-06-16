/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Page navigation events

const RANDOM_ID_PAGE_URL = toDataURL(
  `<script>window.randomId = Math.random() + "-" + Date.now();</script>`
);

const promises = new Set();
const resolutions = new Map();

add_task(async function pageWithoutFrame({ client }) {
  await loadURL(PAGE_URL);

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
    recordPromise("frameStoppedLoading", Page.frameStoppedLoading());
  }

  info("Test Page.navigate");
  recordPromises();

  let navigatedWithinDocumentResolved = false;
  Page.navigatedWithinDocument().finally(
    () => (navigatedWithinDocumentResolved = true)
  );

  const url = RANDOM_ID_PAGE_URL;
  const { frameId } = await Page.navigate({ url });
  info("A new page has been requested");

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

  ok(
    !navigatedWithinDocumentResolved,
    "navigatedWithinDocument never resolved during the test"
  );
});

add_task(async function pageWithSingleFrame({ client }) {
  const { Page } = client;

  await Page.enable();

  // Store all frameNavigated events in an array
  const frameNavigatedEvents = [];
  Page.frameNavigated(e => frameNavigatedEvents.push(e));

  info("Navigate to a page containing an iframe");
  const onStoppedLoading = Page.frameStoppedLoading();
  const { frameId } = await Page.navigate({ url: FRAMESET_SINGLE_URL });
  await onStoppedLoading;

  is(frameNavigatedEvents.length, 2, "Received 2 frameNavigated events");
  is(
    frameNavigatedEvents[0].frame.id,
    frameId,
    "Received the correct frameId for the frameNavigated event"
  );
});

add_task(async function sameDocumentNavigation({ client }) {
  await loadURL(PAGE_URL);

  const { Page } = client;

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Page.enable();
  info("Page domain has been enabled");

  const { frameTree } = await Page.getFrameTree();

  info("Test Page.navigate for a same document navigation");
  const onNavigatedWithinDocument = Page.navigatedWithinDocument();

  let unexpectedEventResolved = false;
  Promise.race([
    Page.frameStartedLoading(),
    Page.frameNavigated(),
    Page.domContentEventFired(),
    Page.loadEventFired(),
    Page.frameStoppedLoading(),
  ]).then(() => (unexpectedEventResolved = true));

  const url = `${PAGE_URL}#some-hash`;
  const { frameId } = await Page.navigate({ url });
  ok(frameId, "Page.navigate returned a frameId");
  is(
    frameId,
    frameTree.frame.id,
    "The Page.navigate's frameId is the same than getFrameTree's one"
  );

  const event = await onNavigatedWithinDocument;
  is(
    event.frameId,
    frameId,
    "The navigatedWithinDocument frameId is the same as in Page.navigate"
  );
  is(event.url, url, "The navigatedWithinDocument url is the expected url");
  ok(!unexpectedEventResolved, "No unexpected navigation event resolved.");
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
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.randomId;
  });
}
