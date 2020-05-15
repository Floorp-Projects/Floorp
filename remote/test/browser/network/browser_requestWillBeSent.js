/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/network/doc_networkEvents.html";
const JS_URL =
  "http://example.com/browser/remote/test/browser/network/file_networkEvents.js";

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
  const history = configureHistory(client, 2);

  const { frameId: frameIdNav } = await Page.navigate({ url: PAGE_URL });
  ok(frameIdNav, "Page.navigate returned a frameId");

  info("Wait for Network events");
  const events = await history.record();
  is(events.length, 2, "Expected number of Network.requestWillBeSent events");

  const docRequest = events[0].payload;
  is(docRequest.request.url, PAGE_URL, "Got the doc request");
  is(docRequest.documentURL, PAGE_URL, "documenURL matches request url");
  is(docRequest.type, "Document", "The doc request has 'Document' type");
  is(docRequest.request.method, "GET", "The doc request has 'GET' method");
  is(
    docRequest.requestId,
    docRequest.loaderId,
    "The doc request has requestId = loaderId"
  );
  is(
    docRequest.frameId,
    frameIdNav,
    "Doc request returns same frameId as Page.navigate"
  );
  is(docRequest.request.headers.host, "example.com", "Doc request has headers");

  const resourceRequest = events[1].payload;
  is(resourceRequest.documentURL, PAGE_URL, "documentURL is trigger document");
  is(resourceRequest.request.url, JS_URL, "Got the JS request");
  is(
    resourceRequest.request.headers.host,
    "example.com",
    "Doc request has headers"
  );
  is(resourceRequest.type, "Script", "The page request has 'Script' type");
  is(resourceRequest.request.method, "GET", "The doc request has 'GET' method");
  is(
    docRequest.frameId,
    frameIdNav,
    "Resource request returns same frameId as Page.navigate"
  );
  todo(
    resourceRequest.loaderId === docRequest.loaderId,
    "The same loaderId is used for dependent requests (Bug 1637838)"
  );
  ok(
    docRequest.timestamp <= resourceRequest.timestamp,
    "Document request happens before resource request"
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
      return `Received ${REQUEST} for ${payload.request?.url}`;
    },
  });

  return history;
}
