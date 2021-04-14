/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_PIN_FIREFOX_TO_TASKBAR() {
  const sandbox = sinon.createSandbox();
  let shell = {
    checkPinCurrentAppToTaskbar() {},
    QueryInterface: () => shell,
    get shellService() {
      return this;
    },

    isCurrentAppPinnedToTaskbarAsync: sandbox.stub(),
    pinCurrentAppToTaskbar: sandbox.stub(),
  };

  // Prefer the mocked implementation and fall back to the original version,
  // which can call back into the mocked version (via this.shellService).
  shell = new Proxy(shell, {
    get(target, prop) {
      return (prop in target ? target : ShellService)[prop];
    },
  });

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
