/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Network.requestWillBeSent event

const PAGE_URL =
  "http://example.com/browser/remote/test/browser/doc_network_requestWillBeSent.html";
const JS_URL =
  "http://example.com/browser/remote/test/browser/file_network_requestWillBeSent.js";

add_task(async function() {
  const { client } = await setupForURL(toDataURL("default-test-page"));

  const { Page, Network } = client;

  await Network.enable();
  ok(true, "Network domain has been enabled");

  let requests = 0;
  const onRequests = new Promise(resolve => {
    Network.requestWillBeSent(event => {
      ok(true, "Received a request");
      switch (++requests) {
        case 1:
          is(event.request.url, PAGE_URL, "Got the page request");
          is(event.type, "Document", "The page request has 'Document' type");
          is(
            event.requestId,
            event.loaderId,
            "The page request has requestId = loaderId (puppeteer assumes that to detect the page start request)"
          );
          break;
        case 2:
          is(event.request.url, JS_URL, "Got the JS request");
          resolve();
          break;
        case 3:
          ok(false, "Expect only two requests");
      }
    });
  });

  const { frameId } = await Page.navigate({ url: PAGE_URL });
  ok(frameId, "Page.navigate returned a frameId");

  info("Wait for Network.requestWillBeSent events");
  await onRequests;

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
});
