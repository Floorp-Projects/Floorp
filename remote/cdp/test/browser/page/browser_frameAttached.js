/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function noEventWhenPageDomainDisabled({ client }) {
  await runFrameAttachedTest(client, 0, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(FRAMESET_NESTED_URL);
  });
});

add_task(async function noEventAfterPageDomainDisabled({ client }) {
  const { Page } = client;

  await Page.enable();
  await Page.disable();

  await runFrameAttachedTest(client, 0, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(FRAMESET_NESTED_URL);
  });
});

add_task(async function noEventWhenNavigatingWithNoFrames({ client }) {
  const { Page } = client;

  info("Navigate to a page with iframes");
  await loadURL(FRAMESET_NESTED_URL);

  await Page.enable();

  await runFrameAttachedTest(client, 0, async () => {
    info("Navigate to a page with no iframes");
    await loadURL(PAGE_URL);
  });
});

add_task(async function eventWhenNavigatingWithFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameAttachedTest(client, 2, async () => {
    info("Navigate to a page with iframes");
    await loadURL(FRAMESET_MULTI_URL);
  });
});

add_task(async function eventWhenNavigatingWithNestedFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameAttachedTest(client, 3, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(FRAMESET_NESTED_URL);
  });
});

add_task(async function eventWhenAttachingFrame({ client }) {
  const { Page } = client;

  await loadURL(FRAMESET_NESTED_URL);

  await Page.enable();

  await runFrameAttachedTest(client, 1, async () => {
    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [PAGE_FRAME_URL],
      async frameURL => {
        const frame = content.document.createElement("iframe");
        frame.src = frameURL;
        const loaded = new Promise(resolve => (frame.onload = resolve));
        content.document.body.appendChild(frame);
        await loaded;
      }
    );
  });
});

async function runFrameAttachedTest(client, expectedEventCount, callback) {
  const { Page } = client;

  const ATTACHED = "Page.frameAttached";

  const history = new RecordEvents(expectedEventCount);
  history.addRecorder({
    event: Page.frameAttached,
    eventName: ATTACHED,
    messageFn: payload => {
      return `Received ${ATTACHED} for frame id ${payload.frameId}`;
    },
  });

  const framesBefore = await getFlattenedFrameTree(client);
  await callback();
  const framesAfter = await getFlattenedFrameTree(client);

  const frameAttachedEvents = await history.record();

  if (expectedEventCount == 0) {
    is(frameAttachedEvents.length, 0, "Got no frame attached event");
    return;
  }

  // check how many frames were attached or detached
  const count = Math.abs(framesBefore.size - framesAfter.size);

  is(count, expectedEventCount, "Expected amount of frames attached");
  is(
    frameAttachedEvents.length,
    count,
    "Received the expected amount of frameAttached events"
  );

  // extract the new or removed frames
  const framesAll = new Map([...framesBefore, ...framesAfter]);
  const expectedFrames = new Map(
    [...framesAll].filter(([key, _value]) => {
      return !framesBefore.has(key) && framesAfter.has(key);
    })
  );

  frameAttachedEvents.forEach(({ payload }) => {
    const { frameId, parentFrameId } = payload;

    info(`Check frame id ${frameId}`);
    const expectedFrame = expectedFrames.get(frameId);

    ok(expectedFrame, `Found expected frame with id ${frameId}`);
    is(
      frameId,
      expectedFrame.id,
      "Got expected frame id for frameAttached event"
    );
    is(
      parentFrameId,
      expectedFrame.parentId,
      "Got expected parent frame id for frameAttached event"
    );
  });
}
