"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
} = ExtensionUtils;

// WeakMap[Extension -> Map[id -> Notification]]
var notificationsMap = new WeakMap();

// WeakMap[Extension -> Set[callback]]
var notificationCallbacksMap = new WeakMap();

// Manages a notification popup (notifications API) created by the extension.
function Notification(extension, id, options) {
  this.extension = extension;
  this.id = id;
  this.options = options;

  let imageURL;
  if (options.iconUrl) {
    imageURL = this.extension.baseURI.resolve(options.iconUrl);
  }

  try {
    let svc = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
    svc.showAlertNotification(imageURL,
                              options.title,
                              options.message,
                              false, // textClickable
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
    notificationsMap.get(this.extension).delete(this.id);
  },

  observe(subject, topic, data) {
    if (topic != "alertfinished") {
      return;
    }

    for (let callback of notificationCallbacksMap.get(this.extension)) {
      callback(this);
    }

    notificationsMap.get(this.extension).delete(this.id);
  },
};

/* eslint-disable mozilla/balanced-listeners */
extensions.on("startup", (type, extension) => {
  notificationsMap.set(extension, new Map());
  notificationCallbacksMap.set(extension, new Set());
});

extensions.on("shutdown", (type, extension) => {
  for (let notification of notificationsMap.get(extension).values()) {
    notification.clear();
  }
  notificationsMap.delete(extension);
  notificationCallbacksMap.delete(extension);
});
/* eslint-enable mozilla/balanced-listeners */

var nextId = 0;

extensions.registerSchemaAPI("notifications", "notifications", (extension, context) => {
  return {
    notifications: {
      create: function(notificationId, options) {
        if (!notificationId) {
          notificationId = String(nextId++);
        }

        let notifications = notificationsMap.get(extension);
        if (notifications.has(notificationId)) {
          notifications.get(notificationId).clear();
        }

        // FIXME: Lots of options still aren't supported, especially
        // buttons.
        let notification = new Notification(extension, notificationId, options);
        notificationsMap.get(extension).set(notificationId, notification);

        return Promise.resolve(notificationId);
      },

      clear: function(notificationId) {
        let notifications = notificationsMap.get(extension);
        if (notifications.has(notificationId)) {
          notifications.get(notificationId).clear();
          return Promise.resolve(true);
        }
        return Promise.resolve(false);
      },

      getAll: function() {
        let result = Array.from(notificationsMap.get(extension).keys());
        return Promise.resolve(result);
      },

      onClosed: new EventManager(context, "notifications.onClosed", fire => {
        let listener = notification => {
          // FIXME: Support the byUser argument.
          fire(notification.id, true);
        };

        notificationCallbacksMap.get(extension).add(listener);
        return () => {
          notificationCallbacksMap.get(extension).delete(listener);
        };
      }).api(),

      // FIXME
      onButtonClicked: ignoreEvent(context, "notifications.onButtonClicked"),
      onClicked: ignoreEvent(context, "notifications.onClicked"),
    },
  };
});
