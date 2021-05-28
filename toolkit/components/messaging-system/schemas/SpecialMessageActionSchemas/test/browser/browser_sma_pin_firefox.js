/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const isWin = AppConstants.platform == "win";

add_task(async function test_PIN_FIREFOX_TO_TASKBAR() {
  const sandbox = sinon.createSandbox();
  let shell = {
    checkPinCurrentAppToTaskbar() {},
    QueryInterface: () => shell,
    get macDockSupport() {
      return this;
    },
    get shellService() {
      return this;
    },

    ensureAppIsPinnedToDock: sandbox.stub(),
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

  function check(count, message) {
    Assert.equal(
      shell.pinCurrentAppToTaskbar.callCount,
      count * isWin,
      `pinCurrentAppToTaskbar was ${message} by the action for windows`
    );
    Assert.equal(
      shell.ensureAppIsPinnedToDock.callCount,
      count * !isWin,
      `ensureAppIsPinnedToDock was ${message} by the action for not windows`
    );
  }
  check(1, "called");

  // Pretend the app is already pinned.
  shell.isCurrentAppPinnedToTaskbarAsync.resolves(true);
  await test();
  check(1, "not called");

  // Pretend the app became unpinned.
  shell.isCurrentAppPinnedToTaskbarAsync.resolves(false);
  await test();
  check(2, "called again");
});
