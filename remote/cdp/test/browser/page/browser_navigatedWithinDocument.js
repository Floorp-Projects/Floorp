/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function noEventWhenPageDomainDisabled({ client }) {
  await loadURL(PAGE_URL);
  await runNavigatedWithinDocumentTest(client, 0, async () => {
    info("Navigate to the '#hash' anchor in the page");
    await navigateToAnchor(PAGE_URL, "hash");
  });
});

add_task(async function noEventAfterPageDomainDisabled({ client }) {
  const { Page } = client;

  await Page.enable();
  await Page.disable();

  await loadURL(PAGE_URL);

  await runNavigatedWithinDocumentTest(client, 0, async () => {
    info("Navigate to the '#hash' anchor in the page");
    await navigateToAnchor(PAGE_URL, "hash");
  });
});

add_task(async function eventWhenNavigatingToHash({ client }) {
  const { Page } = client;

  await Page.enable();

  await loadURL(PAGE_URL);

  await runNavigatedWithinDocumentTest(client, 1, async () => {
    info("Navigate to the '#hash' anchor in the page");
    await navigateToAnchor(PAGE_URL, "hash");
  });
});

add_task(async function eventWhenNavigatingToDifferentHash({ client }) {
  const { Page } = client;

  await Page.enable();

  await navigateToAnchor(PAGE_URL, "hash");

  await runNavigatedWithinDocumentTest(client, 1, async () => {
    info("Navigate to the '#hash' anchor in the page");
    await navigateToAnchor(PAGE_URL, "other-hash");
  });
});

add_task(async function eventWhenNavigatingToHashInFrames({ client }) {
  const { Page } = client;

  await Page.enable();

  await loadURL(FRAMESET_NESTED_URL);

  await runNavigatedWithinDocumentTest(client, 1, async () => {
    info("Navigate to the '#hash' anchor in the first iframe");
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      const iframe = content.frames[0];
      const baseUrl = iframe.location.href;
      iframe.location.href = baseUrl + "#hash-first-frame";
    });
  });

  await runNavigatedWithinDocumentTest(client, 2, async () => {
    info("Navigate to the '#hash' anchor in the nested iframes");
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      const nestedFrame1 = content.frames[0].frames[0];
      const baseUrl1 = nestedFrame1.location.href;
      nestedFrame1.location.href = baseUrl1 + "#hash-nested-frame-1";
      const nestedFrame2 = content.frames[0].frames[0];
      const baseUrl2 = nestedFrame2.location.href;
      nestedFrame2.location.href = baseUrl2 + "#hash-nested-frame-2";
    });
  });
});

async function runNavigatedWithinDocumentTest(
  client,
  expectedEventCount,
  callback
) {
  const { Page } = client;

  const NAVIGATED = "Page.navigatedWithinDocument";

  const history = new RecordEvents(expectedEventCount);
  history.addRecorder({
    event: Page.navigatedWithinDocument,
    eventName: NAVIGATED,
    messageFn: payload => {
      return `Received ${NAVIGATED} for frame id ${payload.frameId}`;
    },
  });

  await callback();

  const navigatedWithinDocumentEvents = await history.record();

  is(
    navigatedWithinDocumentEvents.length,
    expectedEventCount,
    "Got expected amount of navigatedWithinDocument events"
  );
  if (expectedEventCount == 0) {
    return;
  }

  const frames = await getFlattenedFrameTree(client);

  navigatedWithinDocumentEvents.forEach(({ payload }) => {
    const { frameId, url } = payload;

    const frame = frames.get(frameId);
    ok(frame, "Returned a valid frame id");
    is(url, frame.url, "Returned the expectedUrl");
  });
}

function navigateToAnchor(baseUrl, hash) {
  const url = `${baseUrl}#${hash}`;
  const onLocationChange = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    url
  );
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
  return onLocationChange;
}
