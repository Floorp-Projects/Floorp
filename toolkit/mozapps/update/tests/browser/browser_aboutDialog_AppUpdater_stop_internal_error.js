/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppUpdater: "resource://gre/modules/AppUpdater.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

add_setup(function setup_internalErrorTest() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(AppUpdater.prototype, "aus").get(() => {
    throw new Error("intentional test error");
  });
  sandbox.stub(AppUpdater.prototype, "checker").get(() => {
    throw new Error("intentional test error");
  });
  sandbox.stub(AppUpdater.prototype, "um").get(() => {
    throw new Error("intentional test error");
  });
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

// Test that while a download is in-progress, calling `AppUpdater.stop()` while
// in the "internal error" state doesn't cause a shift to any other state.
// This is less a test of the About dialog than of AppUpdater, but it's easier
// to test it via the About dialog just because there is already a testing
// framework for the About dialog.
add_task(async function aboutDialog_AppUpdater_stop_internal_error() {
  let params = {};
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "internalError",
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "internalError",
    },
  ]);
});
