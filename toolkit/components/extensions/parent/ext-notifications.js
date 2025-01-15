/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ToolkitModules = {};

ChromeUtils.defineESModuleGetters(ToolkitModules, {
  EventEmitter: "resource://gre/modules/EventEmitter.sys.mjs",
});

var { ignoreEvent } = ExtensionCommon;

// Manages a notification popup (notifications API) created by the extension.
function Notification(context, notificationsMap, id, options) {
  this.notificationsMap = notificationsMap;
  this.id = id;
  this.options = options;

  let imageURL;
  if (options.iconUrl) {
    imageURL = context.extension.baseURI.resolve(options.iconUrl);
  }

  // Set before calling into nsIAlertsService, because the notification may be
  // closed during the call.
  notificationsMap.set(id, this);

  try {
    let svc = Cc["@mozilla.org/alerts-service;1"].getService(
      Ci.nsIAlertsService
    );
    svc.showAlertNotification(
      imageURL,
      options.title,
      options.message,
      true, // textClickable
      this.id,
      this,
      this.id,
      undefined,
      undefined,
      undefined,
      // Principal is not set because doing so reveals buttons to control
      // notification preferences, which are currently not implemented for
      // notifications triggered via this extension API (bug 1589693).
      undefined,
      context.incognito
    );
  } catch (e) {
    // This will fail if alerts aren't available on the system.

    this.observe(null, "alertfinished", id);
  }
}

Notification.prototype = {
  clear() {
    try {
      let svc = Cc["@mozilla.org/alerts-service;1"].getService(
        Ci.nsIAlertsService
      );
      svc.closeAlert(this.id);
    } catch (e) {
      // This will fail if the OS doesn't support this function.
    }
    this.notificationsMap.delete(this.id);
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "alertclickcallback":
        this.notificationsMap.emit("clicked", data);
        break;
      case "alertfinished":
        this.notificationsMap.emit("closed", data);
        this.notificationsMap.delete(this.id);
        break;
      case "alertshow":
        this.notificationsMap.emit("shown", data);
        break;
    }
  },
};

this.notifications = class extends ExtensionAPIPersistent {
  constructor(extension) {
    super(extension);

    this.nextId = 0;
    this.notificationsMap = new Map();
    ToolkitModules.EventEmitter.decorate(this.notificationsMap);
  }

  onShutdown() {
    for (let notification of this.notificationsMap.values()) {
      notification.clear();
    }
  }

  PERSISTENT_EVENTS = {
    onClosed({ fire }) {
      let listener = (event, notificationId) => {
        // TODO Bug 1413188, Support the byUser argument.
        fire.async(notificationId, true);
      };

      this.notificationsMap.on("closed", listener);
      return {
        unregister: () => {
          this.notificationsMap.off("closed", listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onClicked({ fire }) {
      let listener = (event, notificationId) => {
        fire.async(notificationId);
      };

      this.notificationsMap.on("clicked", listener);
      return {
        unregister: () => {
          this.notificationsMap.off("clicked", listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
    onShown({ fire }) {
      let listener = (event, notificationId) => {
        fire.async(notificationId);
      };

      this.notificationsMap.on("shown", listener);
      return {
        unregister: () => {
          this.notificationsMap.off("shown", listener);
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };

  getAPI(context) {
    let notificationsMap = this.notificationsMap;

    return {
      notifications: {
        create: (notificationId, options) => {
          if (!notificationId) {
            notificationId = String(this.nextId++);
          }

          if (notificationsMap.has(notificationId)) {
            notificationsMap.get(notificationId).clear();
          }

          new Notification(context, notificationsMap, notificationId, options);

          return Promise.resolve(notificationId);
        },

        clear: function (notificationId) {
          if (notificationsMap.has(notificationId)) {
            notificationsMap.get(notificationId).clear();
            return Promise.resolve(true);
          }
          return Promise.resolve(false);
        },

        getAll: function () {
          let result = {};
          notificationsMap.forEach((value, key) => {
            result[key] = value.options;
          });
          return Promise.resolve(result);
        },

        onClosed: new EventManager({
          context,
          module: "notifications",
          event: "onClosed",
          extensionApi: this,
        }).api(),

        onClicked: new EventManager({
          context,
          module: "notifications",
          event: "onClicked",
          extensionApi: this,
        }).api(),

        onShown: new EventManager({
          context,
          module: "notifications",
          event: "onShown",
          extensionApi: this,
        }).api(),

        // TODO Bug 1190681, implement button support.
        onButtonClicked: ignoreEvent(context, "notifications.onButtonClicked"),
      },
    };
  }
};
