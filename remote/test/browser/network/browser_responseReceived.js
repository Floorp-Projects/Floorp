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
  is(events.length, 2, "Expected number of Network.responseReceived events");

  const docResponse = events[0].payload;
  is(docResponse.response.url, PAGE_URL, "Got the doc response");
  is(
    docResponse.response.mimeType,
    "text/html",
    "Doc response has expected mimeType"
  );
  is(docResponse.type, "Document", "The doc response has 'Document' type");
  is(
    docResponse.requestId,
    docResponse.loaderId,
    "The doc request has requestId = loaderId"
  );
  is(
    docResponse.frameId,
    frameIdNav,
    "Doc response returns same frameId as Page.navigate"
  );
  ok(!!docResponse.response.headers.server, "Doc response has headers");
  is(docResponse.response.status, 200, "Doc response status is 200");
  is(docResponse.response.statusText, "OK", "Doc response status is OK");
  if (docResponse.response.fromDiskCache === false) {
    is(
      docResponse.response.remoteIPAddress,
      "127.0.0.1",
      "Doc response has an IP address"
    );
    ok(
      typeof docResponse.response.remotePort == "number",
      "Doc response has a remotePort"
    );
  }
  is(
    docResponse.response.protocol,
    "http/1.1",
    "Doc response has expected protocol"
  );

  const resourceResponse = events[1].payload;
  is(resourceResponse.response.url, JS_URL, "Got the resource response");
  is(
    resourceResponse.response.mimeType,
    "application/x-javascript",
    "Resource response has expected mimeType"
  );
  is(
    resourceResponse.type,
    "Script",
    "The resource response has 'Script' type"
  );
  ok(!!resourceResponse.frameId, "Resource response has a frame id");
  ok(
    !!resourceResponse.response.headers.server,
    "Resource response has headers"
  );
  is(resourceResponse.response.status, 200, "Resource response status is 200");
  is(resourceResponse.response.statusText, "OK", "Response status is OK");
  if (resourceResponse.response.fromDiskCache === false) {
    is(
      resourceResponse.response.remoteIPAddress,
      docResponse.response.remoteIPAddress,
      "Resource response has same IP address and doc response"
    );
    ok(
      typeof resourceResponse.response.remotePort == "number",
      "Resource response has a remotePort"
    );
  }
  is(
    resourceResponse.response.protocol,
    "http/1.1",
    "Resource response has expected protocol"
  );
});

function configureHistory(client, total) {
  const RESPONSE = "Network.responseReceived";

  const { Network } = client;
  const history = new RecordEvents(total);

  history.addRecorder({
    event: Network.responseReceived,
    eventName: RESPONSE,
    messageFn: payload => {
      return `Received ${RESPONSE} for ${payload.response?.url}`;
    },
  });
  return history;
}
