/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ToolkitModules = {};

ChromeUtils.defineModuleGetter(
  ToolkitModules,
  "EventEmitter",
  "resource://gre/modules/EventEmitter.jsm"
);

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
      context.principal, // ensures that Close button is shown on macOS.
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

this.notifications = class extends ExtensionAPI {
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

        clear: function(notificationId) {
          if (notificationsMap.has(notificationId)) {
            notificationsMap.get(notificationId).clear();
            return Promise.resolve(true);
          }
          return Promise.resolve(false);
        },

        getAll: function() {
          let result = {};
          notificationsMap.forEach((value, key) => {
            result[key] = value.options;
          });
          return Promise.resolve(result);
        },

        onClosed: new EventManager({
          context,
          name: "notifications.onClosed",
          register: fire => {
            let listener = (event, notificationId) => {
              // TODO Bug 1413188, Support the byUser argument.
              fire.async(notificationId, true);
            };

            notificationsMap.on("closed", listener);
            return () => {
              notificationsMap.off("closed", listener);
            };
          },
        }).api(),

        onClicked: new EventManager({
          context,
          name: "notifications.onClicked",
          register: fire => {
            let listener = (event, notificationId) => {
              fire.async(notificationId);
            };

            notificationsMap.on("clicked", listener);
            return () => {
              notificationsMap.off("clicked", listener);
            };
          },
        }).api(),

        onShown: new EventManager({
          context,
          name: "notifications.onShown",
          register: fire => {
            let listener = (event, notificationId) => {
              fire.async(notificationId);
            };

            notificationsMap.on("shown", listener);
            return () => {
              notificationsMap.off("shown", listener);
            };
          },
        }).api(),

        // TODO Bug 1190681, implement button support.
        onButtonClicked: ignoreEvent(context, "notifications.onButtonClicked"),
      },
    };
  }
};
