/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global getCDP */

const {RemoteAgent} = ChromeUtils.import("chrome://remote/content/RemoteAgent.jsm");
const {RemoteAgentError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

// Test the Page navigation events

const TEST_URI = "data:text/html;charset=utf-8,default-test-page";

add_task(async function() {
  try {
    await testCDP();
  } catch (e) {
    // Display better error message with the server side stacktrace
    // if an error happened on the server side:
    if (e.response) {
      throw RemoteAgentError.fromJSON(e.response);
    } else {
      throw e;
    }
  }
});

async function testCDP() {
  // Open a test page, to prevent debugging the random default page
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  // Start the CDP server
  RemoteAgent.init();
  RemoteAgent.tabs.start();
  RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

  // Retrieve the chrome-remote-interface library object
  const CDP = await getCDP();

  // Connect to the server
  const client = await CDP({
    target(list) {
      // Ensure debugging the right target, i.e. the one for our test tab.
      return list.find(target => target.url == TEST_URI);
    },
  });
  ok(true, "CDP client has been instantiated");

  const {Page} = client;

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Page.enable();
  ok(true, "Page domain has been enabled");

  const promises = [];
  const resolutions = new Map();
  function recordPromise(name, promise) {
    promise.then(event => {
      ok(true, `Received Page.${name}`);
      resolutions.set(name, event);
    });
    promises.push(promise);
  }
  recordPromise("frameStoppedLoading", Page.frameStoppedLoading());
  recordPromise("navigatedWithinDocument", Page.navigatedWithinDocument());
  recordPromise("domContentEventFired", Page.domContentEventFired());
  recordPromise("loadEventFired", Page.loadEventFired());
  recordPromise("frameNavigated", Page.frameNavigated());
  const url = "data:text/html;charset=utf-8,test-page";
  const { frameId } = await Page.navigate({ url });
  ok(true, "A new page has been loaded");
  ok(frameId, "Page.navigate returned a frameId");

  // Wait for all the promises to resolve
  await Promise.all(promises);

  // Assert the order in which they resolved
  const expectedResolutions = [
    "frameNavigated",
    "domContentEventFired",
    "loadEventFired",
    "navigatedWithinDocument",
    "frameStoppedLoading",
  ];
  Assert.deepEqual([...resolutions.keys()],
     expectedResolutions,
     "Received various Page navigation events in the expected order");

  // Now assert the data exposed by each of these events
  const frameNavigated = resolutions.get("frameNavigated");
  ok(!frameNavigated.frame.parentId, "frameNavigated is for the top level document and" +
    " has a null parentId");
  is(frameNavigated.frame.id, frameId, "frameNavigated id is the same than the one " +
    "returned by Page.navigate");
  is(frameNavigated.frame.name, undefined, "frameNavigated name isn't implemented yet");
  is(frameNavigated.frame.url, url, "frameNavigated url is the same being given to " +
    "Page.navigate");

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
}
