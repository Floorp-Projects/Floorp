/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_PATH = "https://example.com/browser/remote/cdp/test/browser/network";
const FRAMESET_URL = `${BASE_PATH}/doc_frameset.html`;
const FRAMESET_URL_JS = `${BASE_PATH}/file_framesetEvents.js`;
const PAGE_EMPTY_URL = `${BASE_PATH}/doc_empty.html`;
const PAGE_URL = `${BASE_PATH}/doc_networkEvents.html`;
const PAGE_EMPTY_HASH = `#`;
const PAGE_HASH = `#foo`;
const PAGE_URL_WITH_HASH = `${PAGE_URL}${PAGE_HASH}`;
const PAGE_URL_WITH_EMPTY_HASH = `${PAGE_URL}${PAGE_EMPTY_HASH}`;
const PAGE_URL_JS = `${BASE_PATH}/file_networkEvents.js`;

add_task(async function noEventsWhenNetworkDomainDisabled({ client }) {
  const history = configureHistory(client, 0);
  await loadURL(PAGE_URL);

  const events = await history.record();
  is(events.length, 0, "Expected no Network.responseReceived events");
});

add_task(async function noEventsAfterNetworkDomainDisabled({ client }) {
  const { Network } = client;

  const history = configureHistory(client, 0);
  await Network.enable();
  await Network.disable();
  await loadURL(PAGE_URL);

  const events = await history.record();
  is(events.length, 0, "Expected no Network.responseReceived events");
});

add_task(async function documentNavigationWithResource({ client }) {
  const { Page, Network } = client;

  await Network.enable();
  await Page.enable();

  const history = configureHistory(client, 4);

  const frameAttached = Page.frameAttached();
  const { frameId: frameIdNav } = await Page.navigate({ url: FRAMESET_URL });
  const { frameId: frameIdSubFrame } = await frameAttached;
  ok(frameIdNav, "Page.navigate returned a frameId");

  info("Wait for Network events");
  const events = await history.record();
  is(events.length, 4, "Expected number of Network.requestWillBeSent events");

  // Check top-level document request
  const docRequest = events[0].payload;
  is(docRequest.type, "Document", "Document request has the expected type");
  is(docRequest.documentURL, FRAMESET_URL, "documentURL matches requested url");
  is(docRequest.frameId, frameIdNav, "Got the expected frame id");
  is(docRequest.request.url, FRAMESET_URL, "Got the Document request");
  is(docRequest.request.urlFragment, undefined, "Has no URL fragment set");
  is(docRequest.request.method, "GET", "Has the expected request method");
  is(
    docRequest.requestId,
    docRequest.loaderId,
    "The request id is equal to the loader id"
  );
  is(
    docRequest.request.headers.host,
    "example.com",
    "Document request has headers"
  );

  // Check top-level script request
  const scriptRequest = events[1].payload;
  is(scriptRequest.type, "Script", "Script request has the expected type");
  is(
    scriptRequest.documentURL,
    FRAMESET_URL,
    "documentURL is trigger document for the script request"
  );
  is(scriptRequest.frameId, frameIdNav, "Got the expected frame id");
  is(scriptRequest.request.url, FRAMESET_URL_JS, "Got the Script request");
  is(scriptRequest.request.method, "GET", "Has the expected request method");
  is(
    scriptRequest.request.headers.host,
    "example.com",
    "Script request has headers"
  );
  todo(
    scriptRequest.loaderId === docRequest.loaderId,
    "The same loaderId is used for dependent requests (Bug 1637838)"
  );
  assertEventOrder(events[0], events[1]);

  // Check subdocument request
  const subdocRequest = events[2].payload;
  is(
    subdocRequest.type,
    "Subdocument",
    "Subdocument request has the expected type"
  );
  is(subdocRequest.documentURL, FRAMESET_URL, "documenURL matches request url");
  is(subdocRequest.frameId, frameIdSubFrame, "Got the expected frame id");
  is(
    subdocRequest.requestId,
    subdocRequest.loaderId,
    "The request id is equal to the loader id"
  );
  is(subdocRequest.request.url, PAGE_URL, "Got the Subdocument request");
  is(subdocRequest.request.method, "GET", "Has the expected request method");
  is(
    subdocRequest.request.headers.host,
    "example.com",
    "Subdocument request has headers"
  );
  assertEventOrder(events[1], events[2]);

  // Check script request (frame)
  const subscriptRequest = events[3].payload;
  is(subscriptRequest.type, "Script", "Script request has the expected type");
  is(
    subscriptRequest.documentURL,
    PAGE_URL,
    "documentURL is trigger document for the script request"
  );
  is(subscriptRequest.frameId, frameIdSubFrame, "Got the expected frame id");
  todo(
    subscriptRequest.loaderId === docRequest.loaderId,
    "The same loaderId is used for dependent requests (Bug 1637838)"
  );
  is(subscriptRequest.request.url, PAGE_URL_JS, "Got the Script request");
  is(
    subscriptRequest.request.method,
    "GET",
    "Script request has the expected method"
  );
  is(
    subscriptRequest.request.headers.host,
    "example.com",
    "Script request has headers"
  );
  assertEventOrder(events[2], events[3]);
});

add_task(async function documentNavigationToURLWithHash({ client }) {
  const { Page, Network } = client;

  await loadURL(PAGE_EMPTY_URL);

  await Network.enable();
  await Page.enable();

  const history = configureHistory(client, 4);

  const frameNavigated = Page.frameNavigated();
  const { frameId: frameIdNav } = await Page.navigate({
    url: PAGE_URL_WITH_HASH,
  });
  await frameNavigated;
  ok(frameIdNav, "Page.navigate returned a frameId");

  info("Wait for Network events");
  const events = await history.record();
  is(events.length, 2, "Expected number of Network.requestWillBeSent events");

  // Check top-level document request only for fragment usage
  const docRequest = events[0].payload;
  is(docRequest.documentURL, PAGE_URL, "documentURL matches requested URL");
  is(docRequest.request.url, PAGE_URL, "Request url matches requested URL");
  is(
    docRequest.request.urlFragment,
    PAGE_HASH,
    "Request URL fragment is present"
  );
});

add_task(async function documentNavigationToURLWithEmptyHash({ client }) {
  const { Page, Network } = client;

  await loadURL(PAGE_EMPTY_URL);

  await Network.enable();
  await Page.enable();

  const history = configureHistory(client, 4);

  const frameNavigated = Page.frameNavigated();
  const { frameId: frameIdNav } = await Page.navigate({
    url: PAGE_URL_WITH_EMPTY_HASH,
  });
  await frameNavigated;
  ok(frameIdNav, "Page.navigate returned a frameId");

  info("Wait for Network events");
  const events = await history.record();
  is(events.length, 2, "Expected number of Network.requestWillBeSent events");

  // Check top-level document request only for fragment usage
  const docRequest = events[0].payload;
  is(docRequest.documentURL, PAGE_URL, "documentURL matches requested URL");
  is(docRequest.request.url, PAGE_URL, "Request url matches requested URL");
  is(
    docRequest.request.urlFragment,
    PAGE_EMPTY_HASH,
    "Request URL fragment is present"
  );
});

function configureHistory(client, total) {
  const REQUEST = "Network.requestWillBeSent";

  const { Network } = client;
  const history = new RecordEvents(total);

  history.addRecorder({
    event: Network.requestWillBeSent,
    eventName: REQUEST,
    messageFn: payload => {
      return `Received ${REQUEST} for ${payload.request.url}`;
    },
  });

  return history;
}
