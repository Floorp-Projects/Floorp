/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://testing-common/MockRegistrar.jsm");

let idleService = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIIdleService]),
  idleTime: 19999,
};

add_task(function* setup() {
  let fakeIdleService = MockRegistrar.register("@mozilla.org/widget/idleservice;1", idleService);
  do_register_cleanup(() => {
    MockRegistrar.unregister(fakeIdleService);
  });
});

add_task(function* testIdleActive() {
  function background() {
    browser.idle.queryState(20).then(status => {
      browser.test.assertEq("active", status, "Idle status is active");
      browser.test.notifyPass("idle");
    },
    err => {
      browser.test.fail(`Error: ${err} :: ${err.stack}`);
      browser.test.notifyFail("idle");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("idle");
  yield extension.unload();
});

add_task(function* testIdleIdle() {
  function background() {
    browser.idle.queryState(15).then(status => {
      browser.test.assertEq("idle", status, "Idle status is idle");
      browser.test.notifyPass("idle");
    },
    err => {
      browser.test.fail(`Error: ${err} :: ${err.stack}`);
      browser.test.notifyFail("idle");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["idle"],
    },
  });

  yield extension.startup();
  yield extension.awaitFinish("idle");
  yield extension.unload();
});
