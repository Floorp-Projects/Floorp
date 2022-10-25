/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  AppUpdater: "resource:///modules/AppUpdater.jsm",
  sinon: "resource://testing-common/Sinon.jsm",
});

add_setup(function setup_internalErrorTest() {
  const sandbox = sinon.createSandbox();
  sandbox.stub(AppUpdater.prototype, "um").get(() => {
    throw new Error("intentional test error");
  });
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

// Test for about:preferences internal error handling.
add_task(async function aboutPrefs_internalError() {
  let params = {};
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "internalError",
    },
  ]);
});
