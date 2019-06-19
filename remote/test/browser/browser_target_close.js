/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,default-test-page";

// Test the Target closeTarget method and the targetDestroyed event.
add_task(async function() {
  info("Start the CDP server");
  RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));
  const CDP = await getCDP();
  const client = await CDP({});

  info("Setup Target domain");
  const { Target } = client;
  const targetCreated = Target.targetCreated();
  Target.setDiscoverTargets({ discover: true });
  await targetCreated;

  info("Create a new tab and wait for the target to be created");
  const otherTargetCreated = Target.targetCreated();
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);
  const {targetInfo} = await otherTargetCreated;

  const onTabClose = BrowserTestUtils.waitForEvent(tab, "TabClose");
  const targetDestroyed = Target.targetDestroyed();

  info("Close the target");
  Target.closeTarget({ targetId: targetInfo.targetId });

  await onTabClose;
  ok(true, "Tab was closed");

  await targetDestroyed;
  ok(true, "Received the expected Target.targetDestroyed event");

  await client.close();
  ok(true, "The client is closed");

  await RemoteAgent.close();
});
