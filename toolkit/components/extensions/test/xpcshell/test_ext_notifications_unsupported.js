/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(
  Components.ID("{18f25bb4-ab12-4e24-b3b0-69215056160b}"),
  "unsupported alerts service",
  ALERTS_SERVICE_CONTRACT_ID,
  {} // This object lacks an implementation of nsIAlertsService.
);

add_task(async function test_notification_unsupported_backend() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["notifications"],
    },
    async background() {
      let closedPromise = new Promise(resolve => {
        browser.notifications.onClosed.addListener(resolve);
      });
      let createdId = await browser.notifications.create("notifid", {
        type: "basic",
        title: "titl",
        message: "msg",
      });
      let closedId = await closedPromise;
      browser.test.assertEq(createdId, closedId, "ID of closed notification");
      browser.test.assertEq(
        "{}",
        JSON.stringify(await browser.notifications.getAll()),
        "no notifications left"
      );
      browser.test.sendMessage("notification_closed");
    },
  });
  await extension.startup();
  await extension.awaitMessage("notification_closed");
  await extension.unload();
});
