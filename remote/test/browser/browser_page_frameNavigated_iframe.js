/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that frameNavigated is not fired for iframes embedded in the page.

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/doc_page_frameNavigated_iframe.html";

const promises = new Set();
const resolutions = new Map();

add_task(async function() {
  const { client } = await setupForURL(toDataURL("default-test-page"));

  const { Page } = client;
  await Page.enable();

  // Store all frameNavigated events in an array
  const frameNavigatedEvents = [];
  Page.frameNavigated(e => frameNavigatedEvents.push(e));

  info("Navigate to a page containing an iframe");
  const onStoppedLoading = Page.frameStoppedLoading();
  const { frameId } = await Page.navigate({ url: PAGE_URL });
  await onStoppedLoading;

  is(frameNavigatedEvents.length, 1, "Received only 1 frameNavigated event");
  is(
    frameNavigatedEvents[0].frame.id,
    frameId,
    "Received the correct frameId for the frameNavigated event"
  );

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await RemoteAgent.close();
});
