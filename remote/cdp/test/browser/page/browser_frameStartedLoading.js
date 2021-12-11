/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function noEventWhenPageDomainDisabled({ client }) {
  await runFrameStartedLoadingTest(client, 0, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(FRAMESET_NESTED_URL);
  });
});

add_task(async function noEventAfterPageDomainDisabled({ client }) {
  const { Page } = client;

  await Page.enable();
  await Page.disable();

  await runFrameStartedLoadingTest(client, 0, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(FRAMESET_NESTED_URL);
  });
});

add_task(async function eventWhenNavigatingWithNoFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameStartedLoadingTest(client, 1, async () => {
    info("Navigate to a page with no iframes");
    await loadURL(PAGE_URL);
  });
});

add_task(async function eventsWhenNavigatingWithFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameStartedLoadingTest(client, 3, async () => {
    info("Navigate to a page with iframes");
    await loadURL(FRAMESET_MULTI_URL);
  });
});

add_task(async function eventsWhenNavigatingWithNestedFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameStartedLoadingTest(client, 4, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(FRAMESET_NESTED_URL);
  });
});

async function runFrameStartedLoadingTest(
  client,
  expectedEventCount,
  callback
) {
  const { Page } = client;

  const STARTED_LOADING = "Page.frameStartedLoading";

  const history = new RecordEvents(expectedEventCount);
  history.addRecorder({
    event: Page.frameStartedLoading,
    eventName: STARTED_LOADING,
    messageFn: payload => {
      return `Received ${STARTED_LOADING} for frame id ${payload.frameId}`;
    },
  });

  await callback();

  const frameStartedLoadingEvents = await history.record();

  is(
    frameStartedLoadingEvents.length,
    expectedEventCount,
    "Got expected amount of frameStartedLoading events"
  );
  if (expectedEventCount == 0) {
    return;
  }

  const frames = await getFlattenedFrameTree(client);

  frameStartedLoadingEvents.forEach(({ payload }) => {
    const { frameId } = payload;

    info(`Check frame id ${frameId}`);
    const expectedFrame = frames.get(frameId);

    ok(expectedFrame, `Found expected frame with id ${frameId}`);
    is(
      frameId,
      expectedFrame.id,
      "Got expected frame id for frameStartedLoading event"
    );
  });
}
