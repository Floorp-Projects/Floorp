/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test order and consistency of Network/Page events as a whole.
// Details of specific events are checked in event-specific test files.

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/network/doc_networkEvents.html";
const JS_URL =
  "http://example.com/browser/remote/test/browser/network/file_networkEvents.js";

add_task(async function documentNavigationWithScriptResource({ client }) {
  const { Page, Network } = client;
  await Network.enable();
  await Page.enable();
  await Page.setLifecycleEventsEnabled({ enabled: true });
  const { history, urlToEvents } = configureHistory(client);
  const navigateDone = history.addPromise("Page.navigate");

  const { frameId } = await Page.navigate({ url: PAGE_URL }).then(navigateDone);
  ok(frameId, "Page.navigate returned a frameId");

  info("Wait for events");
  const events = await history.record();
  is(events.length, 8, "Expected number of events");
  const eventNames = events.map(
    item => `${item.eventName}(${item.payload.type || item.payload.name})`
  );
  info(`Received events: ${eventNames}`);
  const documentEvents = urlToEvents.get(PAGE_URL);
  const resourceEvents = urlToEvents.get(JS_URL);
  is(
    2,
    documentEvents.length,
    "Expected number of Network events for document"
  );
  is(
    2,
    resourceEvents.length,
    "Expected number of Network events for resource"
  );

  const docRequest = documentEvents[0].event;
  is(docRequest.request.url, PAGE_URL, "Got the doc request");
  is(docRequest.documentURL, PAGE_URL, "documenURL matches request url");
  const lifeCycleEvents = history.findEvents("Page.lifecycleEvent");
  const firstLifecycle = history.indexOf("Page.lifecycleEvent");
  ok(
    firstLifecycle > documentEvents[1].index,
    "First lifecycle event is after document response"
  );
  for (const e of lifeCycleEvents) {
    is(
      e.loaderId,
      docRequest.loaderId,
      `${e.name} lifecycle event has same loaderId as document request`
    );
  }

  const resourceRequest = resourceEvents[0].event;
  is(resourceRequest.documentURL, PAGE_URL, "documentURL is trigger document");
  is(resourceRequest.request.url, JS_URL, "Got the JS request");
  ok(
    documentEvents[0].index < resourceEvents[0].index,
    "Document request received before resource request"
  );
  const navigateStep = history.indexOf("Page.navigate");
  ok(
    navigateStep > documentEvents[1].index,
    "Page.navigate returns after document response"
  );
  ok(
    navigateStep < resourceEvents[0].index,
    "Page.navigate returns before resource request"
  );
  ok(
    navigateStep < firstLifecycle,
    "Page.navigate returns before first lifecycle event"
  );

  const docResponse = documentEvents[1].event;
  is(docResponse.response.url, PAGE_URL, "Got the doc response");
  is(
    docRequest.frameId,
    docResponse.frameId,
    "Doc response frame id matches that of doc request"
  );
  ok(!!docResponse.response.headers.server, "Doc response has headers");
  // TODO? response reports extra request header "upgrade-insecure-requests":"1"
  // Assert.deepEqual(
  //   docResponse.response.requestHeaders,
  //   docRequest.request.headers,
  //   "Response event reports same request headers as request event"
  // );

  ok(
    docRequest.timestamp <= docResponse.timestamp,
    "Document request happens before document response"
  );
  const resourceResponse = resourceEvents[1].event;
  is(resourceResponse.response.url, JS_URL, "Got the resource response");
  todo(
    resourceResponse.loaderId === docRequest.loaderId,
    "The same loaderId is used for dependent responses (Bug 1637838)"
  );
  ok(!!resourceResponse.frameId, "Resource response has a frame id");
  is(
    docRequest.frameId,
    resourceResponse.frameId,
    "Resource response frame id matches that of doc request"
  );
  Assert.deepEqual(
    resourceResponse.response.requestHeaders,
    resourceRequest.request.headers,
    "Response event reports same request headers as request event"
  );
  ok(
    resourceRequest.timestamp <= resourceResponse.timestamp,
    "Document request happens before document response"
  );
});

function configureHistory(client) {
  const REQUEST = "Network.requestWillBeSent";
  const RESPONSE = "Network.responseReceived";
  const LIFECYCLE = "Page.lifecycleEvent";

  const { Network, Page } = client;
  const history = new RecordEvents(8);
  const urlToEvents = new Map();
  function updateUrlToEvents(kind) {
    return ({ payload, index, eventName }) => {
      const url = payload[kind]?.url;
      if (!url) {
        return;
      }
      if (!urlToEvents.get(url)) {
        urlToEvents.set(url, [{ index, event: payload, eventName }]);
      } else {
        urlToEvents.get(url).push({ index, event: payload, eventName });
      }
    };
  }

  history.addRecorder({
    event: Network.requestWillBeSent,
    eventName: REQUEST,
    messageFn: payload => {
      return `Received ${REQUEST} for ${payload.request?.url}`;
    },
    callback: updateUrlToEvents("request"),
  });

  history.addRecorder({
    event: Network.responseReceived,
    eventName: RESPONSE,
    messageFn: payload => {
      return `Received ${RESPONSE} for ${payload.response?.url}`;
    },
    callback: updateUrlToEvents("response"),
  });

  history.addRecorder({
    event: Page.lifecycleEvent,
    eventName: LIFECYCLE,
    messageFn: payload => {
      return `Received ${LIFECYCLE} ${payload.name}`;
    },
  });

  return { history, urlToEvents };
}
