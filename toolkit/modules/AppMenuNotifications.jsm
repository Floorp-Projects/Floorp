/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AppMenuNotifications"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function AppMenuNotification(id, mainAction, secondaryAction, options = {}) {
  this.id = id;
  this.mainAction = mainAction;
  this.secondaryAction = secondaryAction;
  this.options = options;
  this.dismissed = this.options.dismissed || false;
}

var AppMenuNotifications = {
  _notifications: [],
  _hasInitialized: false,

  get notifications() {
    return Array.from(this._notifications);
  },

  _lazyInit() {
    if (!this._hasInitialized) {
      Services.obs.addObserver(this, "xpcom-shutdown");
      Services.obs.addObserver(this, "appMenu-notifications-request");
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.obs.removeObserver(this, "appMenu-notifications-request");
  },

  observe(subject, topic, status) {
    switch (topic) {
      case "xpcom-shutdown":
        this.uninit();
        break;
      case "appMenu-notifications-request":
        if (this._notifications.length) {
          Services.obs.notifyObservers(null, "appMenu-notifications", "init");
        }
        break;
    }
  },

  get activeNotification() {
    if (this._notifications.length) {
      const doorhanger = this._notifications.find(
        n => !n.dismissed && !n.options.badgeOnly
      );
      return doorhanger || this._notifications[0];
    }

    return null;
  },

  showNotification(id, mainAction, secondaryAction, options = {}) {
    let notification = new AppMenuNotification(
      id,
      mainAction,
      secondaryAction,
      options
    );
    let existingIndex = this._notifications.findIndex(n => n.id == id);
    if (existingIndex != -1) {
      this._notifications.splice(existingIndex, 1);
    }

    // We don't want to clobber doorhanger notifications just to show a badge,
    // so don't dismiss any of them and the badge will show once the doorhanger
    // gets resolved.
    if (!options.badgeOnly && !options.dismissed) {
      this._notifications.forEach(n => {
        n.dismissed = true;
      });
    }

    // Since notifications are generally somewhat pressing, the ideal case is that
    // we never have two notifications at once. However, in the event that we do,
    // it's more likely that the older notification has been sitting around for a
    // bit, and so we don't want to hide the new notification behind it. Thus,
    // we want our notifications to behave like a stack instead of a queue.
    this._notifications.unshift(notification);

    this._lazyInit();
    this._updateNotifications();
    return notification;
  },

  showBadgeOnlyNotification(id) {
    return this.showNotification(id, null, null, { badgeOnly: true });
  },

  removeNotification(id) {
    let notifications;
    if (typeof id == "string") {
      notifications = this._notifications.filter(n => n.id == id);
    } else {
      // If it's not a string, assume RegExp
      notifications = this._notifications.filter(n => id.test(n.id));
    }
    // _updateNotifications can be expensive if it forces attachment of XBL
    // bindings that haven't been used yet, so return early if we haven't found
    // any notification to remove, as callers may expect this removeNotification
    // method to be a no-op for non-existent notifications.
    if (!notifications.length) {
      return;
    }

    notifications.forEach(n => {
      this._removeNotification(n);
    });
    this._updateNotifications();
  },

  dismissNotification(id) {
    let notifications;
    if (typeof id == "string") {
      notifications = this._notifications.filter(n => n.id == id);
    } else {
      // If it's not a string, assume RegExp
      notifications = this._notifications.filter(n => id.test(n.id));
    }

    notifications.forEach(n => {
      n.dismissed = true;
      if (n.options.onDismissed) {
        n.options.onDismissed();
      }
    });
    this._updateNotifications();
  },

  callMainAction(win, notification, fromDoorhanger) {
    let action = notification.mainAction;
    this._callAction(win, notification, action, fromDoorhanger);
  },

  callSecondaryAction(win, notification) {
    let action = notification.secondaryAction;
    this._callAction(win, notification, action, true);
  },

  _callAction(win, notification, action, fromDoorhanger) {
    let dismiss = true;
    if (action) {
      try {
        action.callback(win, fromDoorhanger);
      } catch (error) {
        Cu.reportError(error);
      }

      dismiss = action.dismiss;
    }

    if (dismiss) {
      notification.dismissed = true;
    } else {
      this._removeNotification(notification);
    }

    this._updateNotifications();
  },

  _removeNotification(notification) {
    // This notification may already be removed, in which case let's just ignore.
    let notifications = this._notifications;
    if (!notifications) {
      return;
    }

    var index = notifications.indexOf(notification);
    if (index == -1) {
      return;
    }

    // Remove the notification
    notifications.splice(index, 1);
  },

  _updateNotifications() {
    Services.obs.notifyObservers(null, "appMenu-notifications", "update");
  },
};
