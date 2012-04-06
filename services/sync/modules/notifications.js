/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Myk Melez <myk@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
