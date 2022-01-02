/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";

const createdAlerts = [];

const mockAlertsService = {
  showPersistentNotification(persistentData, alert, alertListener) {
    this.showAlert(alert, alertListener);
  },

  showAlert(alert, listener) {
    createdAlerts.push(alert);
    listener.observe(null, "alertfinished", alert.cookie);
  },

  showAlertNotification(
    imageUrl,
    title,
    text,
    textClickable,
    cookie,
    alertListener,
    name,
    dir,
    lang,
    data,
    principal,
    privateBrowsing
  ) {
    this.showAlert({ cookie, title, text, privateBrowsing }, alertListener);
  },

  closeAlert(name) {
    // This mock immediately close the alert on show, so this is empty.
  },

  QueryInterface: ChromeUtils.generateQI(["nsIAlertsService"]),

  createInstance(outer, iid) {
    if (outer != null) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(iid);
  },
};

const registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
registrar.registerFactory(
  Components.ID("{173a036a-d678-4415-9cff-0baff6bfe554}"),
  "alerts service",
  ALERTS_SERVICE_CONTRACT_ID,
  mockAlertsService
);

add_task(async function test_notification_privateBrowsing_flag() {
  let extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["notifications"],
    },
    files: {
      "page.html": `<meta charset="utf-8"><script src="page.js"></script>`,
      async "page.js"() {
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
    },
  });
  await extension.startup();

  async function checkPrivateBrowsingFlag(privateBrowsing) {
    let contentPage = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}/page.html`,
      { extension, remote: extension.extension.remote, privateBrowsing }
    );
    await extension.awaitMessage("notification_closed");
    await contentPage.close();

    Assert.equal(createdAlerts.length, 1, "expected one alert");
    let notification = createdAlerts.shift();
    Assert.equal(notification.cookie, "notifid", "notification id");
    Assert.equal(notification.title, "titl", "notification title");
    Assert.equal(notification.text, "msg", "notification text");
    Assert.equal(notification.privateBrowsing, privateBrowsing, "pbm flag");
  }

  await checkPrivateBrowsingFlag(false);
  await checkPrivateBrowsingFlag(true);

  await extension.unload();
});
