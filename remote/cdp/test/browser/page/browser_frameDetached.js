/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Disable bfcache to force documents to be destroyed on navigation
Services.prefs.setIntPref("browser.sessionhistory.max_total_viewers", 0);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.sessionhistory.max_total_viewers");
});

add_task(async function noEventWhenPageDomainDisabled({ client }) {
  info("Navigate to a page with nested iframes");
  await loadURL(FRAMESET_NESTED_URL);

  await runFrameDetachedTest(client, 0, async () => {
    info("Navigate away from a page with an iframe");
    await loadURL(PAGE_URL);
  });
});

add_task(async function noEventAfterPageDomainDisabled({ client }) {
  const { Page } = client;

  info("Navigate to a page with nested iframes");
  await loadURL(FRAMESET_NESTED_URL);

  await Page.enable();
  await Page.disable();

  await runFrameDetachedTest(client, 0, async () => {
    info("Navigate away to a page with no iframes");
    await loadURL(PAGE_URL);
  });
});

add_task(async function noEventWhenNavigatingWithNoFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  info("Navigate to a page with no iframes");
  await loadURL(PAGE_URL);

  await runFrameDetachedTest(client, 0, async () => {
    info("Navigate away to a page with no iframes");
    await loadURL(PAGE_URL);
  });
});

add_task(async function eventWhenNavigatingWithFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with iframes");
  await loadURL(FRAMESET_MULTI_URL);

  await Page.enable();

  await runFrameDetachedTest(client, 2, async () => {
    info("Navigate away to a page with no iframes");
    await loadURL(PAGE_URL);
  });
});

add_task(async function eventWhenNavigatingWithNestedFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with nested iframes");
  await loadURL(FRAMESET_NESTED_URL);

  await Page.enable();

  await runFrameDetachedTest(client, 3, async () => {
    info("Navigate away to a page with no iframes");
    await loadURL(PAGE_URL);
  });
});

add_task(async function eventWhenDetachingFrame({ client }) {
  const { Page } = client;

  info("Navigate to a page with iframes");
  await loadURL(FRAMESET_MULTI_URL);

  await Page.enable();

  await runFrameDetachedTest(client, 1, async () => {
    // Remove the single frame from the page
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      const frame = content.document.getElementsByTagName("iframe")[0];
      frame.remove();
    });
  });
});

add_task(async function eventWhenDetachingNestedFrames({ client }) {
  const { Page, Runtime } = client;

  info("Navigate to a page with nested iframes");
  await loadURL(FRAMESET_NESTED_URL);

  await Page.enable();
  await Runtime.enable();

  const { context } = await Runtime.executionContextCreated();

  await runFrameDetachedTest(client, 3, async () => {
    // Remove top-frame, which also removes any nested frames
    await evaluate(client, context.id, async () => {
      const frame = document.getElementsByTagName("iframe")[0];
      frame.remove();
    });
  });
});

async function runFrameDetachedTest(client, expectedEventCount, callback) {
  const { Page } = client;

  const DETACHED = "Page.frameDetached";

  const history = new RecordEvents(expectedEventCount);
  history.addRecorder({
    event: Page.frameDetached,
    eventName: DETACHED,
    messageFn: payload => {
      return `Received ${DETACHED} for frame id ${payload.frameId}`;
    },
  });

  const framesBefore = await getFlattenedFrameTree(client);
  await callback();
  const framesAfter = await getFlattenedFrameTree(client);

  const frameDetachedEvents = await history.record();

  if (expectedEventCount == 0) {
    is(frameDetachedEvents.length, 0, "Got no frame detached event");
    return;
  }

  // check how many frames were attached or detached
  const count = Math.abs(framesBefore.size - framesAfter.size);

  is(count, expectedEventCount, "Expected amount of frames detached");
  is(
    frameDetachedEvents.length,
    count,
    "Received the expected amount of frameDetached events"
  );

  // extract the new or removed frames
  const framesAll = new Map([...framesBefore, ...framesAfter]);
  const expectedFrames = new Map(
    [...framesAll].filter(([key, _value]) => {
      return framesBefore.has(key) && !framesAfter.has(key);
    })
  );

  frameDetachedEvents.forEach(({ payload }) => {
    const { frameId } = payload;

    info(`Check frame id ${frameId}`);
    const expectedFrame = expectedFrames.get(frameId);

    is(
      frameId,
      expectedFrame.id,
      "Got expected frame id for frameDetached event"
    );
  });
}
