/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict"

let Cc = Components.classes;
let Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["Notifications"];

function log(msg) {
  // Services.console.logStringMessage(msg);
}

let _notificationsMap = {};
let _handlersMap = {};

function Notification(aId, aOptions) {
  this._id = aId;
  this._when = (new Date).getTime();
  this.fillWithOptions(aOptions);
}

Notification.prototype = {
  fillWithOptions: function(aOptions) {
    if ("icon" in aOptions && aOptions.icon != null)
      this._icon = aOptions.icon;
    else
      throw "Notification icon is mandatory";

    if ("title" in aOptions && aOptions.title != null)
      this._title = aOptions.title;
    else
      throw "Notification title is mandatory";

    if ("message" in aOptions && aOptions.message != null)
      this._message = aOptions.message;
    else
      this._message = null;

    if ("priority" in aOptions && aOptions.priority != null)
      this._priority = aOptions.priority;

    if ("buttons" in aOptions && aOptions.buttons != null) {
      if (aOptions.buttons.length > 3)
        throw "Too many buttons provided. The max number is 3";

      this._buttons = {};
      for (let i = 0; i < aOptions.buttons.length; i++) {
        let button_id = aOptions.buttons[i].buttonId;
        this._buttons[button_id] = aOptions.buttons[i];
      }
    } else {
      this._buttons = null;
    }

    if ("ongoing" in aOptions && aOptions.ongoing != null)
      this._ongoing = aOptions.ongoing;
    else
      this._ongoing = false;

    if ("progress" in aOptions && aOptions.progress != null)
      this._progress = aOptions.progress;
    else
      this._progress = null;

    if ("onCancel" in aOptions && aOptions.onCancel != null)
      this._onCancel = aOptions.onCancel;
    else
      this._onCancel = null;

    if ("onClick" in aOptions && aOptions.onClick != null)
      this._onClick = aOptions.onClick;
    else
      this._onClick = null;

    if ("cookie" in aOptions && aOptions.cookie != null)
      this._cookie = aOptions.cookie;
    else
      this._cookie = null;

    if ("handlerKey" in aOptions && aOptions.handlerKey != null)
      this._handlerKey = aOptions.handlerKey;

    if ("persistent" in aOptions && aOptions.persistent != null)
      this._persistent = aOptions.persistent;
    else
      this._persistent = false;
  },

  show: function() {
    let msg = {
        type: "Notification:Show",
        id: this._id,
        title: this._title,
        smallIcon: this._icon,
        ongoing: this._ongoing,
        when: this._when,
        persistent: this._persistent,
    };

    if (this._message)
      msg.text = this._message;

    if (this._progress) {
      msg.progress_value = this._progress;
      msg.progress_max = 100;
      msg.progress_indeterminate = false;
    } else if (Number.isNaN(this._progress)) {
      msg.progress_value = 0;
      msg.progress_max = 0;
      msg.progress_indeterminate = true;
    }

    if (this._cookie)
      msg.cookie = JSON.stringify(this._cookie);

    if (this._priority)
      msg.priority = this._priority;

    if (this._buttons) {
      msg.actions = [];
      let buttonName;
      for (buttonName in this._buttons) {
        let button = this._buttons[buttonName];
        let obj = {
          buttonId: button.buttonId,
          title : button.title,
          icon : button.icon
        };
        msg.actions.push(obj);
      }
    }

    if (this._light)
      msg.light = this._light;

    if (this._handlerKey)
      msg.handlerKey = this._handlerKey;

    Services.androidBridge.handleGeckoMessage(msg);
    return this;
  },

  cancel: function() {
    let msg = {
      type: "Notification:Hide",
      id: this._id,
      handlerKey: this._handlerKey,
      cookie: JSON.stringify(this._cookie),
    };
    Services.androidBridge.handleGeckoMessage(msg);
  }
}

let Notifications = {
  get idService() {
    delete this.idService;
    return this.idService = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
  },

  registerHandler: function(key, handler) {
    if (!_handlersMap[key]) {
      _handlersMap[key] = [];
    }
    _handlersMap[key].push(handler);
  },

  unregisterHandler: function(key, handler) {
    let i = _handlersMap[key].indexOf(handler);
    if (i > -1) {
      _handlersMap.splice(i, 1);
    }
  },

  create: function notif_notify(aOptions) {
    let id = this.idService.generateUUID().toString();

    let notification = new Notification(id, aOptions);
    _notificationsMap[id] = notification;
    notification.show();

    return id;
  },

  update: function notif_update(aId, aOptions) {
    let notification = _notificationsMap[aId];
    if (!notification)
      throw "Unknown notification id";
    notification.fillWithOptions(aOptions);
    notification.show();
  },

  cancel: function notif_cancel(aId) {
    let notification = _notificationsMap[aId];
    if (notification)
      notification.cancel();
  },

  observe: function notif_observe(aSubject, aTopic, aData) {
    Services.console.logStringMessage(aTopic + " " + aData);

    let data = JSON.parse(aData);
    let id = data.id;
    let handlerKey = data.handlerKey;
    let cookie = data.cookie ? JSON.parse(data.cookie) : undefined;
    let notification = _notificationsMap[id];

    switch (data.eventType) {
      case "notification-clicked":
        if (notification && notification._onClick)
          notification._onClick(id, notification._cookie);

        if (handlerKey) {
          _handlersMap[handlerKey].forEach(function(handler) {
            handler.onClick(cookie);
          });
        }

        break;
      case "notification-button-clicked":
        if (handlerKey) {
          _handlersMap[handlerKey].forEach(function(handler) {
            handler.onButtonClick(data.buttonId, cookie);
          });
        }

        if (notification && !notification._buttons) {
          break;
        }

        let button = notification._buttons[data.buttonId];
        if (button) {
          button.onClicked(id, notification._cookie);
        }
        break;
      case "notification-cleared":
      case "notification-closed":
        if (handlerKey) {
          _handlersMap[handlerKey].forEach(function(handler) {
            handler.onCancel(cookie);
          });
        }

        if (notification && notification._onCancel)
          notification._onCancel(id, notification._cookie);
        delete _notificationsMap[id]; // since the notification was dismissed, we no longer need to hold a reference.
        break;
    }
  },

  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsISupports) &&
        !aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
}
