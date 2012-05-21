/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["Notifications", "Notification", "NotificationButton"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/util.js");

let Notifications = {
  // Match the referenced values in toolkit/content/widgets/notification.xml.
  get PRIORITY_INFO()     1, // PRIORITY_INFO_LOW
  get PRIORITY_WARNING()  4, // PRIORITY_WARNING_LOW
  get PRIORITY_ERROR()    7, // PRIORITY_CRITICAL_LOW

  // FIXME: instead of making this public, dress the Notifications object
  // to behave like an iterator (using generators?) and have callers access
  // this array through the Notifications object.
  notifications: [],

  _observers: [],

  // XXX Should we have a helper method for adding a simple notification?
  // I.e. something like |function notify(title, description, priority)|.

  add: function Notifications_add(notification) {
    this.notifications.push(notification);
    Observers.notify("weave:notification:added", notification, null);
  },

  remove: function Notifications_remove(notification) {
    let index = this.notifications.indexOf(notification);

    if (index != -1) {
      this.notifications.splice(index, 1);
      Observers.notify("weave:notification:removed", notification, null);
    }
  },

  /**
   * Replace an existing notification.
   */
  replace: function Notifications_replace(oldNotification, newNotification) {
    let index = this.notifications.indexOf(oldNotification);

    if (index != -1)
      this.notifications.splice(index, 1, newNotification);
    else {
      this.notifications.push(newNotification);
      // XXX Should we throw because we didn't find the existing notification?
      // XXX Should we notify observers about weave:notification:added?
    }

    // XXX Should we notify observers about weave:notification:replaced?
  },

  /**
   * Remove all notifications that match a title. If no title is provided, all
   * notifications are removed.
   *
   * @param title [optional]
   *        Title of notifications to remove; falsy value means remove all
   */
  removeAll: function Notifications_removeAll(title) {
    this.notifications.filter(function(old) old.title == title || !title).
      forEach(function(old) this.remove(old), this);
  },

  // replaces all existing notifications with the same title as the new one
  replaceTitle: function Notifications_replaceTitle(notification) {
    this.removeAll(notification.title);
    this.add(notification);
  }
};


/**
 * A basic notification.  Subclass this to create more complex notifications.
 */
function Notification(title, description, iconURL, priority, buttons) {
  this.title = title;
  this.description = description;

  if (iconURL)
    this.iconURL = iconURL;

  if (priority)
    this.priority = priority;

  if (buttons)
    this.buttons = buttons;
}

// We set each prototype property individually instead of redefining
// the entire prototype to avoid blowing away existing properties
// of the prototype like the the "constructor" property, which we use
// to bind notification objects to their XBL representations.
Notification.prototype.priority = Notifications.PRIORITY_INFO;
Notification.prototype.iconURL = null;
Notification.prototype.buttons = [];

/**
 * A button to display in a notification.
 */
function NotificationButton(label, accessKey, callback) {
  function callbackWrapper() {
    try {
      callback.apply(this, arguments);
    } catch (e) {
      let logger = Log4Moz.repository.getLogger("Sync.Notifications");
      logger.error("An exception occurred: " + Utils.exceptionStr(e));
      logger.info(Utils.stackTrace(e));
      throw e;
    }
  }

  this.label = label;
  this.accessKey = accessKey;
  this.callback = callbackWrapper;
}
