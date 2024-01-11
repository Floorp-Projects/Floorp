/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetworkListener } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/listeners/NetworkListener.sys.mjs"
);
const { TabManager } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/TabManager.sys.mjs"
);

add_task(async function test_beforeRequestSent() {
  const listener = new NetworkListener();
  const events = [];
  const onEvent = (name, data) => events.push(data);
  listener.on("before-request-sent", onEvent);

  const tab1 = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);
  const contextId1 = TabManager.getIdForBrowser(tab1.linkedBrowser);

  const tab2 = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab2"
  );
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  const contextId2 = TabManager.getIdForBrowser(tab2.linkedBrowser);

  listener.startListening();

  await fetch(tab1.linkedBrowser, "https://example.com/?1");
  ok(events.length == 1, "One event was received");
  assertNetworkEvent(events[0], contextId1, "https://example.com/?1");

  info("Check that events are no longer emitted after calling stopListening");
  listener.stopListening();
  await fetch(tab1.linkedBrowser, "https://example.com/?2");
  ok(events.length == 1, "No new event was received");

  listener.startListening();
  await fetch(tab1.linkedBrowser, "https://example.com/?3");
  ok(events.length == 2, "A new event was received");
  assertNetworkEvent(events[1], contextId1, "https://example.com/?3");

  info("Check network event from the new tab");
  await fetch(tab2.linkedBrowser, "https://example.com/?4");
  ok(events.length == 3, "A new event was received");
  assertNetworkEvent(events[2], contextId2, "https://example.com/?4");

  gBrowser.removeTab(tab1);
  gBrowser.removeTab(tab2);
  listener.off("before-request-sent", onEvent);
  listener.destroy();
});

add_task(async function test_beforeRequestSent_newTab() {
  const listener = new NetworkListener();
  const onBeforeRequestSent = listener.once("before-request-sent");
  listener.startListening();

  info("Check network event related to loading a new tab");
  const tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.com/document-builder.sjs?html=tab"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const contextId = TabManager.getIdForBrowser(tab.linkedBrowser);
  const event = await onBeforeRequestSent;

  assertNetworkEvent(
    event,
    contextId,
    "https://example.com/document-builder.sjs?html=tab"
  );
  gBrowser.removeTab(tab);
});

add_task(async function test_fetchError() {
  const listener = new NetworkListener();
  const onFetchError = listener.once("fetch-error");
  listener.startListening();

  info("Check fetchError event when loading a new tab");
  const tab = BrowserTestUtils.addTab(gBrowser, "https://not_a_valid_url/");
  BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  const contextId = TabManager.getIdForBrowser(tab.linkedBrowser);
  const event = await onFetchError;

  assertNetworkEvent(event, contextId, "https://not_a_valid_url/");
  is(event.errorText, "NS_ERROR_UNKNOWN_HOST");
  gBrowser.removeTab(tab);
});

function assertNetworkEvent(event, expectedContextId, expectedUrl) {
  is(event.contextId, expectedContextId, "Event has the expected context id");
  is(event.requestData.url, expectedUrl, "Event has the expected url");
}
