/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Target domain

add_task(async function() {
  // Start the CDP server
  await RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

  // Retrieve the chrome-remote-interface library object
  const CDP = await getCDP();

  // Connect to the server
  const {webSocketDebuggerUrl} = await CDP.Version();
  const client = await CDP({"target": webSocketDebuggerUrl});
  ok(true, "CDP client has been instantiated");

  const {Target} = client;
  ok("Target" in client, "Target domain is available");

  // Wait for all Target.targetCreated event. One for each tab, plus the one
  // for the main process target.
  const targetsCreated = new Promise(resolve => {
    let targets = 0;
    const unsubscribe = Target.targetCreated(event => {
      if (++targets >= gBrowser.tabs.length + 1) {
        unsubscribe();
        resolve();
      }
    });
  });
  Target.setDiscoverTargets({ discover: true });
  await targetsCreated;

  const {browserContextId} = await Target.createBrowserContext();

  const targetCreated = Target.targetCreated();
  const {targetId} = await Target.createTarget({ browserContextId });
  ok(true, `Target created: ${targetId}`);
  ok(!!targetId, "createTarget returns a non-empty target id");

  const {targetInfo} = await targetCreated;
  is(targetId, targetInfo.targetId, "createTarget and targetCreated refers to the same target id");
  is(browserContextId, targetInfo.browserContextId, "the created target is reported to be of the same browser context");
  is(targetInfo.type, "page", "The target is a page");

  // Releasing the browser context is going to remove the tab opened when calling createTarget
  await Target.disposeBrowserContext({ browserContextId });

  await client.close();
  ok(true, "The client is closed");

  await RemoteAgent.close();
});
