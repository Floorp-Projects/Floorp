/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Target closeTarget method and the targetDestroyed event.
add_task(async function(client) {
  info("Setup Target domain");
  const { Target } = client;

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

  info("Create a new tab and wait for the target to be created");
  const otherTargetCreated = Target.targetCreated();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    toDataURL("")
  );
  const { targetInfo } = await otherTargetCreated;
  is(targetInfo.type, "page");

  const onTabClose = BrowserTestUtils.waitForEvent(tab, "TabClose");
  const targetDestroyed = Target.targetDestroyed();

  info("Close the target");
  Target.closeTarget({ targetId: targetInfo.targetId });

  await onTabClose;
  ok(true, "Tab was closed");

  await targetDestroyed;
  ok(true, "Received the expected Target.targetDestroyed event");
});
