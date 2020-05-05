"use strict";

/* exported MockAlertsService */

function mockServicesChromeScript() {
  const MOCK_ALERTS_CID = Components.ID(
    "{48068bc2-40ab-4904-8afd-4cdfb3a385f3}"
  );
  const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";

  const { setTimeout } = ChromeUtils.import(
    "resource://gre/modules/Timer.jsm",
    {}
  );
  const registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

  let activeNotifications = Object.create(null);

  const mockAlertsService = {
    showPersistentNotification: function(persistentData, alert, alertListener) {
      this.showAlert(alert, alertListener);
    },

    showAlert: function(alert, listener) {
      activeNotifications[alert.name] = {
        listener: listener,
        cookie: alert.cookie,
        title: alert.title,
      };

      // fake async alert show event
      if (listener) {
        setTimeout(function() {
          listener.observe(null, "alertshow", alert.cookie);
        }, 100);
      }
    },

    showAlertNotification: function(
      imageUrl,
      title,
      text,
      textClickable,
      cookie,
      alertListener,
      name
    ) {
      this.showAlert(
        {
          name: name,
          cookie: cookie,
          title: title,
        },
        alertListener
      );
    },

    closeAlert: function(name) {
      let alertNotification = activeNotifications[name];
      if (alertNotification) {
        if (alertNotification.listener) {
          alertNotification.listener.observe(
            null,
            "alertfinished",
            alertNotification.cookie
          );
        }
        delete activeNotifications[name];
      }
    },

    QueryInterface: ChromeUtils.generateQI(["nsIAlertsService"]),

    createInstance: function(outer, iid) {
      if (outer != null) {
        throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
      }
      return this.QueryInterface(iid);
    },
  };

  registrar.registerFactory(
    MOCK_ALERTS_CID,
    "alerts service",
    ALERTS_SERVICE_CONTRACT_ID,
    mockAlertsService
  );

  function clickNotifications(doClose) {
    // Until we need to close a specific notification, just click them all.
    for (let [name, notification] of Object.entries(activeNotifications)) {
      let { listener, cookie } = notification;
      listener.observe(null, "alertclickcallback", cookie);
      if (doClose) {
        mockAlertsService.closeAlert(name);
      }
    }
  }

  function closeAllNotifications() {
    for (let alertName of Object.keys(activeNotifications)) {
      mockAlertsService.closeAlert(alertName);
    }
  }

  const { addMessageListener, sendAsyncMessage } = this;

  addMessageListener("mock-alert-service:unregister", () => {
    closeAllNotifications();
    activeNotifications = null;
    registrar.unregisterFactory(MOCK_ALERTS_CID, mockAlertsService);
    sendAsyncMessage("mock-alert-service:unregistered");
  });

  addMessageListener(
    "mock-alert-service:click-notifications",
    clickNotifications
  );

  addMessageListener(
    "mock-alert-service:close-notifications",
    closeAllNotifications
  );

  sendAsyncMessage("mock-alert-service:registered");
}

const MockAlertsService = {
  async register() {
    if (this._chromeScript) {
      throw new Error("MockAlertsService already registered");
    }
    this._chromeScript = SpecialPowers.loadChromeScript(
      mockServicesChromeScript
    );
    await this._chromeScript.promiseOneMessage("mock-alert-service:registered");
  },
  async unregister() {
    if (!this._chromeScript) {
      throw new Error("MockAlertsService not registered");
    }
    this._chromeScript.sendAsyncMessage("mock-alert-service:unregister");
    return this._chromeScript
      .promiseOneMessage("mock-alert-service:unregistered")
      .then(() => {
        this._chromeScript.destroy();
        this._chromeScript = null;
      });
  },
  async clickNotifications() {
    // Most implementations of the nsIAlertsService automatically close upon click.
    await this._chromeScript.sendAsyncMessage(
      "mock-alert-service:click-notifications",
      true
    );
  },
  async clickNotificationsWithoutClose() {
    // The implementation on macOS does not automatically close the notification.
    await this._chromeScript.sendAsyncMessage(
      "mock-alert-service:click-notifications",
      false
    );
  },
  async closeNotifications() {
    await this._chromeScript.sendAsyncMessage(
      "mock-alert-service:close-notifications"
    );
  },
};
