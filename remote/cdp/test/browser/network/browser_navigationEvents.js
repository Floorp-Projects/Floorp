/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test order and consistency of Network/Page events as a whole.
// Details of specific events are checked in event-specific test files.

// Bug 1734694: network request header mismatch when using HTTPS
const BASE_PATH = "http://example.com/browser/remote/cdp/test/browser/network";
const FRAMESET_URL = `${BASE_PATH}/doc_frameset.html`;
const FRAMESET_JS_URL = `${BASE_PATH}/file_framesetEvents.js`;
const PAGE_URL = `${BASE_PATH}/doc_networkEvents.html`;
const PAGE_JS_URL = `${BASE_PATH}/file_networkEvents.js`;

add_task(async function eventsForTopFrameNavigation({ client }) {
  const { history, frameId: frameIdNav } = await prepareTest(
    client,
    FRAMESET_URL,
    10
  );

  const documentEvents = filterEventsByType(history, "Document");
  const scriptEvents = filterEventsByType(history, "Script");
  const subdocumentEvents = filterEventsByType(history, "Subdocument");

  is(documentEvents.length, 2, "Expected number of Document events");
  is(subdocumentEvents.length, 2, "Expected number of Subdocument events");
  is(scriptEvents.length, 4, "Expected number of Script events");

  const navigatedEvents = history.findEvents("Page.navigate");
  is(navigatedEvents.length, 1, "Expected number of navigate done events");

  const frameAttachedEvents = history.findEvents("Page.frameAttached");
  is(frameAttachedEvents.length, 1, "Expected number of frame attached events");

  // network events for document and script
  assertEventOrder(documentEvents[0], documentEvents[1]);
  assertEventOrder(documentEvents[1], navigatedEvents[0], {
    ignoreTimestamps: true,
  });
  assertEventOrder(navigatedEvents[0], scriptEvents[0], {
    ignoreTimestamps: true,
  });
  assertEventOrder(scriptEvents[0], scriptEvents[1]);

  const docRequest = documentEvents[0].payload;
  is(docRequest.documentURL, FRAMESET_URL, "documenURL matches target url");
  is(docRequest.frameId, frameIdNav, "Got the expected frame id");
  is(docRequest.request.url, FRAMESET_URL, "Got the Document request");

  const docResponse = documentEvents[1].payload;
  is(docResponse.frameId, frameIdNav, "Got the expected frame id");
  is(docResponse.response.url, FRAMESET_URL, "Got the Document response");
  ok(!!docResponse.response.headers.server, "Document response has headers");
  // TODO? response reports extra request header "upgrade-insecure-requests":"1"
  // Assert.deepEqual(
  //   docResponse.response.requestHeaders,
  //   docRequest.request.headers,
  //   "Response event reports same request headers as request event"
  // );

  const scriptRequest = scriptEvents[0].payload;
  is(
    scriptRequest.documentURL,
    FRAMESET_URL,
    "documentURL is trigger document"
  );
  is(scriptRequest.frameId, frameIdNav, "Got the expected frame id");
  is(scriptRequest.request.url, FRAMESET_JS_URL, "Got the Script request");

  const scriptResponse = scriptEvents[1].payload;
  is(scriptResponse.frameId, frameIdNav, "Got the expected frame id");
  todo(
    scriptResponse.loaderId === docRequest.loaderId,
    "The same loaderId is used for dependent responses (Bug 1637838)"
  );
  is(scriptResponse.response.url, FRAMESET_JS_URL, "Got the Script response");
  Assert.deepEqual(
    scriptResponse.response.requestHeaders,
    scriptRequest.request.headers,
    "Response event reports same request headers as request event"
  );

  // frame is attached after all resources of the document have been loaded
  // and before sub document starts loading
  assertEventOrder(scriptEvents[1], frameAttachedEvents[0], {
    ignoreTimestamps: true,
  });
  assertEventOrder(frameAttachedEvents[0], subdocumentEvents[0], {
    ignoreTimestamps: true,
  });

  const { frameId: frameIdSubFrame, parentFrameId } =
    frameAttachedEvents[0].payload;
  is(parentFrameId, frameIdNav, "Got expected parent frame id");

  // network events for subdocument and script
  assertEventOrder(subdocumentEvents[0], subdocumentEvents[1]);
  assertEventOrder(subdocumentEvents[1], scriptEvents[2]);
  assertEventOrder(scriptEvents[2], scriptEvents[3]);

  const subdocRequest = subdocumentEvents[0].payload;
  is(
    subdocRequest.documentURL,
    FRAMESET_URL,
    "documentURL is trigger document"
  );
  is(subdocRequest.frameId, frameIdSubFrame, "Got the expected frame id");
  is(subdocRequest.request.url, PAGE_URL, "Got the Subdocument request");

  const subdocResponse = subdocumentEvents[1].payload;
  is(subdocResponse.frameId, frameIdSubFrame, "Got the expected frame id");
  is(subdocResponse.response.url, PAGE_URL, "Got the Subdocument response");

  const subscriptRequest = scriptEvents[2].payload;
  is(subscriptRequest.documentURL, PAGE_URL, "documentURL is trigger document");
  is(subscriptRequest.frameId, frameIdSubFrame, "Got the expected frame id");
  is(subscriptRequest.request.url, PAGE_JS_URL, "Got the Script request");

  const subscriptResponse = scriptEvents[3].payload;
  is(subscriptResponse.frameId, frameIdSubFrame, "Got the expected frame id");
  is(subscriptResponse.response.url, PAGE_JS_URL, "Got the Script response");
  todo(
    subscriptResponse.loaderId === subdocRequest.loaderId,
    "The same loaderId is used for dependent responses (Bug 1637838)"
  );
  Assert.deepEqual(
    subscriptResponse.response.requestHeaders,
    subscriptRequest.request.headers,
    "Response event reports same request headers as request event"
  );

  const lifeCycleEvents = history
    .findEvents("Page.lifecycleEvent")
    .map(event => event.payload);
  for (const { name, loaderId } of lifeCycleEvents) {
    is(
      loaderId,
      docRequest.loaderId,
      `${name} lifecycle event has same loaderId as Document request`
    );
  }
});

async function prepareTest(client, url, totalCount) {
  const REQUEST = "Network.requestWillBeSent";
  const RESPONSE = "Network.responseReceived";
  const FRAMEATTACHED = "Page.frameAttached";
  const LIFECYCLE = "Page.livecycleEvent";

  const { Network, Page } = client;
  const history = new RecordEvents(totalCount);

  history.addRecorder({
    event: Network.requestWillBeSent,
    eventName: REQUEST,
    messageFn: payload => {
      return `Received ${REQUEST} for ${payload.request?.url}`;
    },
  });

  history.addRecorder({
    event: Network.responseReceived,
    eventName: RESPONSE,
    messageFn: payload => {
      return `Received ${RESPONSE} for ${payload.response?.url}`;
    },
  });

  history.addRecorder({
    event: Page.frameAttached,
    eventName: FRAMEATTACHED,
    messageFn: ({ frameId, parentFrameId: parentId }) => {
      return `Received ${FRAMEATTACHED} frame=${frameId} parent=${parentId}`;
    },
  });

  history.addRecorder({
    event: Page.lifecycleEvent,
    eventName: LIFECYCLE,
    messageFn: payload => {
      return `Received ${LIFECYCLE} ${payload.name}`;
    },
  });

  await Network.enable();
  await Page.enable();

  const navigateDone = history.addPromise("Page.navigate");
  const { frameId } = await Page.navigate({ url }).then(navigateDone);
  ok(frameId, "Page.navigate returned a frameId");

  info("Wait for events");
  const events = await history.record();

  info(`Received events: ${events.map(getDescriptionForEvent)}`);
  is(events.length, totalCount, "Received expected number of events");

  return { history, frameId };
}
