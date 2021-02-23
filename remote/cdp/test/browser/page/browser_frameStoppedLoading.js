/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL("<div>foo</div>");
const DOC_IFRAME_MULTI = toDataURL(`
  <iframe src='data:text/html,foo'></iframe>
  <iframe src='data:text/html,bar'></iframe>
`);
const DOC_IFRAME_NESTED = toDataURL(`
  <iframe src="${DOC_IFRAME_MULTI}"></iframe>
`);

add_task(async function noEventWhenPageDomainDisabled({ client }) {
  await runFrameStoppedLoadingTest(client, 0, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(DOC_IFRAME_NESTED);
  });
});

add_task(async function noEventAfterPageDomainDisabled({ client }) {
  const { Page } = client;

  await Page.enable();
  await Page.disable();

  await runFrameStoppedLoadingTest(client, 0, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(DOC_IFRAME_NESTED);
  });
});

add_task(async function eventWhenNavigatingWithNoFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameStoppedLoadingTest(client, 1, async () => {
    info("Navigate to a page with no iframes");
    await loadURL(DOC);
  });
});

add_task(async function eventsWhenNavigatingWithFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameStoppedLoadingTest(client, 3, async () => {
    info("Navigate to a page with iframes");
    await loadURL(DOC_IFRAME_MULTI);
  });
});

add_task(async function eventsWhenNavigatingWithNestedFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await runFrameStoppedLoadingTest(client, 4, async () => {
    info("Navigate to a page with nested iframes");
    await loadURL(DOC_IFRAME_NESTED);
  });
});

async function runFrameStoppedLoadingTest(
  client,
  expectedEventCount,
  callback
) {
  const { Page } = client;

  const STOPPED_LOADING = "Page.frameStoppedLoading";

  const history = new RecordEvents(expectedEventCount);
  history.addRecorder({
    event: Page.frameStoppedLoading,
    eventName: STOPPED_LOADING,
    messageFn: payload => {
      return `Received ${STOPPED_LOADING} for frame id ${payload.frameId}`;
    },
  });

  await callback();

  const frameStoppedLoadingEvents = await history.record();

  is(
    frameStoppedLoadingEvents.length,
    expectedEventCount,
    "Got expected amount of frameStoppedLoading events"
  );
  if (expectedEventCount == 0) {
    return;
  }

  const frames = await getFlattenedFrameTree(client);

  frameStoppedLoadingEvents.forEach(({ payload }) => {
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
