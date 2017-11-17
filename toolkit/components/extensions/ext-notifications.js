"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

const ToolkitModules = {};

XPCOMUtils.defineLazyModuleGetter(ToolkitModules, "EventEmitter",
                                  "resource://gre/modules/EventEmitter.jsm");

var {
  ignoreEvent,
} = ExtensionCommon;

// Manages a notification popup (notifications API) created by the extension.
function Notification(extension, notificationsMap, id, options) {
  this.notificationsMap = notificationsMap;
  this.id = id;
  this.options = options;

  let imageURL;
  if (options.iconUrl) {
    imageURL = extension.baseURI.resolve(options.iconUrl);
  }

  try {
    let svc = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    svc.showAlertNotification(imageURL,
                              options.title,
                              options.message,
                              true, // textClickable
                              this.id,
                              this,
                              this.id);
  } catch (e) {
    // This will fail if alerts aren't available on the system.
  }
}

Notification.prototype = {
  clear() {
    try {
      let svc = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
      svc.closeAlert(this.id);
    } catch (e) {
      // This will fail if the OS doesn't support this function.
    }
    this.notificationsMap.delete(this.id);
  },

  observe(subject, topic, data) {
    let emitAndDelete = event => {
      this.notificationsMap.emit(event, data);
      this.notificationsMap.delete(this.id);
    };

    switch (topic) {
      case "alertclickcallback":
        emitAndDelete("clicked");
        break;
      case "alertfinished":
        emitAndDelete("closed");
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
    let {extension} = context;
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

          let notification = new Notification(extension, notificationsMap, notificationId, options);
          notificationsMap.set(notificationId, notification);

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

        onClosed: new EventManager(context, "notifications.onClosed", fire => {
          let listener = (event, notificationId) => {
            // TODO Bug 1413188, Support the byUser argument.
            fire.async(notificationId, true);
          };

          notificationsMap.on("closed", listener);
          return () => {
            notificationsMap.off("closed", listener);
          };
        }).api(),

        onClicked: new EventManager(context, "notifications.onClicked", fire => {
          let listener = (event, notificationId) => {
            fire.async(notificationId, true);
          };

          notificationsMap.on("clicked", listener);
          return () => {
            notificationsMap.off("clicked", listener);
          };
        }).api(),

        onShown: new EventManager(context, "notifications.onShown", fire => {
          let listener = (event, notificationId) => {
            fire.async(notificationId, true);
          };

          notificationsMap.on("shown", listener);
          return () => {
            notificationsMap.off("shown", listener);
          };
        }).api(),

        // TODO Bug 1190681, implement button support.
        onButtonClicked: ignoreEvent(context, "notifications.onButtonClicked"),
      },
    };
  }
};
