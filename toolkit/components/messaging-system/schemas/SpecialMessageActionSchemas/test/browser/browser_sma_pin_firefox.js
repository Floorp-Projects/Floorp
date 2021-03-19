/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_PIN_FIREFOX_TO_TASKBAR() {
  const sandbox = sinon.createSandbox();
  const shell = {
    QueryInterface: () => shell,
    isCurrentAppPinnedToTaskbarAsync: sandbox.stub(),
    pinCurrentAppToTaskbar: sandbox.stub(),
  };

  const test = () =>
    SMATestUtils.executeAndValidateAction(
      { type: "PIN_FIREFOX_TO_TASKBAR" },
      {
        ownerGlobal: {
          getShellService: () => shell,
        },
      }
    );

  await test();
  Assert.equal(
    shell.pinCurrentAppToTaskbar.callCount,
    1,
    "pinCurrentAppToTaskbar was called by the action"
  );

  // Pretend the app is already pinned.
  shell.isCurrentAppPinnedToTaskbarAsync.resolves(true);
  await test();
  Assert.equal(
    shell.pinCurrentAppToTaskbar.callCount,
    1,
    "pinCurrentAppToTaskbar was not called by the action"
  );

  // Pretend the app became unpinned.
  shell.isCurrentAppPinnedToTaskbarAsync.resolves(false);
  await test();
  Assert.equal(
    shell.pinCurrentAppToTaskbar.callCount,
    2,
    "pinCurrentAppToTaskbar was called again by the action"
  );
});
