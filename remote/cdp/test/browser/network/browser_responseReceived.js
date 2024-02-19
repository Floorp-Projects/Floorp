/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE_PATH = "https://example.com/browser/remote/cdp/test/browser/network";
const FRAMESET_URL = `${BASE_PATH}/doc_frameset.html`;
const FRAMESET_JS_URL = `${BASE_PATH}/file_framesetEvents.js`;
const PAGE_URL = `${BASE_PATH}/doc_networkEvents.html`;
const PAGE_JS_URL = `${BASE_PATH}/file_networkEvents.js`;

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
  const { frameId: frameIdSubframe } = await frameAttached;
  ok(frameIdNav, "Page.navigate returned a frameId");

  info("Wait for Network events");
  const events = await history.record();
  is(events.length, 4, "Expected number of Network.responseReceived events");

  // Check top-level document response
  const docResponse = events[0].payload;
  is(docResponse.type, "Document", "Document response has expected type");
  is(docResponse.frameId, frameIdNav, "Got the expected frame id");
  is(
    docResponse.requestId,
    docResponse.loaderId,
    "The response id is equal to the loader id"
  );
  is(docResponse.response.url, FRAMESET_URL, "Got the Document response");
  is(
    docResponse.response.mimeType,
    "text/html",
    "Document response has expected mimeType"
  );
  ok(!!docResponse.response.headers.server, "Document response has headers");
  is(docResponse.response.status, 200, "Document response has expected status");
  is(
    docResponse.response.statusText,
    "OK",
    "Document response has expected status text"
  );
  if (docResponse.response.fromDiskCache === false) {
    is(
      docResponse.response.remoteIPAddress,
      "127.0.0.1",
      "Document response has the expected IP address"
    );
    Assert.equal(
      typeof docResponse.response.remotePort,
      "number",
      "Document response has a remotePort"
    );
  }
  is(
    docResponse.response.protocol,
    "http/1.1",
    "Document response has expected protocol"
  );

  // Check top-level script response
  const scriptResponse = events[1].payload;
  is(scriptResponse.type, "Script", "Script response has expected type");
  is(scriptResponse.frameId, frameIdNav, "Got the expected frame id");
  is(scriptResponse.response.url, FRAMESET_JS_URL, "Got the Script response");
  is(
    scriptResponse.response.mimeType,
    "application/x-javascript",
    "Script response has expected mimeType"
  );
  ok(!!scriptResponse.response.headers.server, "Script response has headers");
  is(
    scriptResponse.response.status,
    200,
    "Script response has the expected status"
  );
  is(
    scriptResponse.response.statusText,
    "OK",
    "Script response has the expected status text"
  );
  if (scriptResponse.response.fromDiskCache === false) {
    is(
      scriptResponse.response.remoteIPAddress,
      docResponse.response.remoteIPAddress,
      "Script response has same IP address as document response"
    );
    Assert.equal(
      typeof scriptResponse.response.remotePort,
      "number",
      "Script response has a remotePort"
    );
  }
  is(
    scriptResponse.response.protocol,
    "http/1.1",
    "Script response has the expected protocol"
  );

  // Check subdocument response
  const frameDocResponse = events[2].payload;
  is(
    frameDocResponse.type,
    "Subdocument",
    "Subdocument response has expected type"
  );
  is(frameDocResponse.frameId, frameIdSubframe, "Got the expected frame id");
  is(
    frameDocResponse.requestId,
    frameDocResponse.loaderId,
    "The response id is equal to the loader id"
  );
  is(
    frameDocResponse.response.url,
    PAGE_URL,
    "Got the expected Document response"
  );
  is(
    frameDocResponse.response.mimeType,
    "text/html",
    "Document response has expected mimeType"
  );
  ok(
    !!frameDocResponse.response.headers.server,
    "Subdocument response has headers"
  );
  is(
    frameDocResponse.response.status,
    200,
    "Subdocument response has expected status"
  );
  is(
    frameDocResponse.response.statusText,
    "OK",
    "Subdocument response has expected status text"
  );
  if (frameDocResponse.response.fromDiskCache === false) {
    is(
      frameDocResponse.response.remoteIPAddress,
      "127.0.0.1",
      "Subdocument response has the expected IP address"
    );
    Assert.equal(
      typeof frameDocResponse.response.remotePort,
      "number",
      "Subdocument response has a remotePort"
    );
  }
  is(
    frameDocResponse.response.protocol,
    "http/1.1",
    "Subdocument response has expected protocol"
  );

  // Check frame script response
  const frameScriptResponse = events[3].payload;
  is(frameScriptResponse.type, "Script", "Script response has expected type");
  is(frameScriptResponse.frameId, frameIdSubframe, "Got the expected frame id");
  is(frameScriptResponse.response.url, PAGE_JS_URL, "Got the Script response");
  is(
    frameScriptResponse.response.mimeType,
    "application/x-javascript",
    "Script response has expected mimeType"
  );
  ok(
    !!frameScriptResponse.response.headers.server,
    "Script response has headers"
  );
  is(
    frameScriptResponse.response.status,
    200,
    "Script response has the expected status"
  );
  is(
    frameScriptResponse.response.statusText,
    "OK",
    "Script response has the expected status text"
  );
  if (frameScriptResponse.response.fromDiskCache === false) {
    is(
      frameScriptResponse.response.remoteIPAddress,
      docResponse.response.remoteIPAddress,
      "Script response has same IP address as document response"
    );
    Assert.equal(
      typeof frameScriptResponse.response.remotePort,
      "number",
      "Script response has a remotePort"
    );
  }
  is(
    frameScriptResponse.response.protocol,
    "http/1.1",
    "Script response has the expected protocol"
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
      return `Received ${RESPONSE} for ${payload.response.url}`;
    },
  });
  return history;
}
