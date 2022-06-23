/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_PIN_PRIVATE_FIREFOX_TO_TASKBAR() {
  const sandbox = sinon.createSandbox();
  let shell = {
    async checkPinCurrentAppToTaskbarAsync() {},
    QueryInterface: () => shell,
    get macDockSupport() {
      return this;
    },
    get shellService() {
      return this;
    },

    ensureAppIsPinnedToDock: sandbox.stub(),
    isCurrentAppPinnedToTaskbarAsync: sandbox.stub(),
    pinCurrentAppToTaskbarAsync: sandbox.stub().resolves(undefined),
    isAppInDock: false,
  };

  // Prefer the mocked implementation and fall back to the original version,
  // which can call back into the mocked version (via this.shellService).
  shell = new Proxy(shell, {
    get(target, prop) {
      return (Object.hasOwn(target, prop) ? target : ShellService)[prop];
    },
  });

  const test = () =>
    SMATestUtils.executeAndValidateAction(
      {
        type: "PIN_FIREFOX_TO_TASKBAR",
        data: {
          privatePin: true,
        },
      },
      {
        ownerGlobal: {
          getShellService: () => shell,
        },
      }
    );

  await test();

  function check(count, message, arg) {
    Assert.equal(
      shell.pinCurrentAppToTaskbarAsync.callCount,
      count,
      `pinCurrentAppToTaskbarAsync was ${message} by the action for windows`
    );
    if (arg) {
      Assert.equal(
        shell.pinCurrentAppToTaskbarAsync.calledWith(arg),
        true,
        `pinCurrentAppToTaskbarAsync was ${message} with the arg: ${JSON.stringify(
          arg
        )}`
      );
    }
  }
  check(1, "called", true);

  // Pretend the app is already pinned.
  shell.isCurrentAppPinnedToTaskbarAsync.resolves(true);
  shell.isAppInDock = true;
  await test();
  check(1, "not called");

  // Pretend the app became unpinned.
  shell.isCurrentAppPinnedToTaskbarAsync.resolves(false);
  shell.isAppInDock = false;
  await test();
  check(2, "called again", true);
});
