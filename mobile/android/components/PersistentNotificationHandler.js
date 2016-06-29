/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

XPCOMUtils.defineLazyModuleGetter(this, 'Services', // jshint ignore:line
                                  'resource://gre/modules/Services.jsm');
XPCOMUtils.defineLazyServiceGetter(this, "notificationStorage",
                                   "@mozilla.org/notificationStorage;1",
                                   "nsINotificationStorage");
XPCOMUtils.defineLazyServiceGetter(this, "serviceWorkerManager",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

function PersistentNotificationHandler() {
}

PersistentNotificationHandler.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  classID: Components.ID("{75390fe7-f8a3-423a-b3b1-258d7eabed40}"),

  observe(subject, topic, data) {
    if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT) {
      Cu.import("resource://gre/modules/NotificationDB.jsm");
    }
    const persistentInfo = JSON.parse(data);

    if (topic === 'persistent-notification-click') {
      notificationStorage.getByID(persistentInfo.origin, persistentInfo.id, {
        handle(id, title, dir, lang, body, tag, icon, data, behavior, serviceWorkerRegistrationScope) {
          serviceWorkerManager.sendNotificationClickEvent(
            persistentInfo.originSuffix,
            serviceWorkerRegistrationScope,
            id,
            title,
            dir,
            lang,
            body,
            tag,
            icon,
            data,
            behavior
          );
          notificationStorage.delete(persistentInfo.origin, persistentInfo.id);
        }
      });
    } else if (topic === 'persistent-notification-close') {
      notificationStorage.getByID(persistentInfo.origin, persistentInfo.id, {
        handle(id, title, dir, lang, body, tag, icon, data, behavior, serviceWorkerRegistrationScope) {
          serviceWorkerManager.sendNotificationCloseEvent(
            persistentInfo.originSuffix,
            serviceWorkerRegistrationScope,
            id,
            title,
            dir,
            lang,
            body,
            tag,
            icon,
            data,
            behavior
          );
          notificationStorage.delete(persistentInfo.origin, persistentInfo.id);
        }
      });
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  PersistentNotificationHandler
]);
