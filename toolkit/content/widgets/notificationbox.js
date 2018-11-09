/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. If you need to
// define globals, wrap in a block to prevent leaking onto `window`.

MozElements.NotificationBox = class NotificationBox {
  /**
   * Creates a new class to handle a notification box, but does not add any
   * elements to the DOM until a notification has to be displayed.
   *
   * @param insertElementFn
   *        Called with the "notification-stack" element as an argument when the
   *        first notification has to be displayed.
   */
  constructor(insertElementFn) {
    this._insertElementFn = insertElementFn;
    this._animating = false;
    this.currentNotification = null;
  }

  get stack() {
    if (!this._stack) {
      let stack = document.createXULElement("stack");
      stack._notificationBox = this;
      stack.className = "notificationbox-stack";
      stack.appendChild(document.createXULElement("spacer"));
      stack.addEventListener("transitionend", event => {
        if (event.target.localName == "notification" &&
            event.propertyName == "margin-top") {
          this._finishAnimation();
        }
      });
      this._insertElementFn(stack);
      this._stack = stack;
    }
    return this._stack;
  }

  get _allowAnimation() {
    return Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled");
  }

  get allNotifications() {
    // Don't create any DOM if no new notification has been added yet.
    if (!this._stack) {
      return [];
    }

    var closedNotification = this._closedNotification;
    var notifications = this.stack.getElementsByTagName("notification");
    return Array.filter(notifications, n => n != closedNotification);
  }

  getNotificationWithValue(aValue) {
    var notifications = this.allNotifications;
    for (var n = notifications.length - 1; n >= 0; n--) {
      if (aValue == notifications[n].getAttribute("value"))
        return notifications[n];
    }
    return null;
  }

  appendNotification(aLabel, aValue, aImage, aPriority, aButtons, aEventCallback) {
    if (aPriority < this.PRIORITY_INFO_LOW ||
      aPriority > this.PRIORITY_CRITICAL_HIGH)
      throw "Invalid notification priority " + aPriority;

    // check for where the notification should be inserted according to
    // priority. If two are equal, the existing one appears on top.
    var notifications = this.allNotifications;
    var insertPos = null;
    for (var n = notifications.length - 1; n >= 0; n--) {
      if (notifications[n].priority < aPriority)
        break;
      insertPos = notifications[n];
    }

    var newitem = document.createXULElement("notification");
    // Can't use instanceof in case this was created from a different document:
    let labelIsDocFragment = aLabel && typeof aLabel == "object" && aLabel.nodeType &&
      aLabel.nodeType == aLabel.DOCUMENT_FRAGMENT_NODE;
    if (!labelIsDocFragment)
      newitem.setAttribute("label", aLabel);
    newitem.setAttribute("value", aValue);
    if (aImage)
      newitem.setAttribute("image", aImage);
    newitem.eventCallback = aEventCallback;

    if (aButtons) {
      // The notification-button-default class is added to the button
      // with isDefault set to true. If there is no such button, it is
      // added to the first button (unless that button has isDefault
      // set to false). There cannot be multiple default buttons.
      var defaultElem;

      for (var b = 0; b < aButtons.length; b++) {
        var button = aButtons[b];
        var buttonElem = document.createXULElement("button");
        buttonElem.setAttribute("label", button.label);
        if (typeof button.accessKey == "string")
          buttonElem.setAttribute("accesskey", button.accessKey);
        buttonElem.classList.add("notification-button");

        if (button.isDefault ||
          b == 0 && !("isDefault" in button))
          defaultElem = buttonElem;

        newitem.appendChild(buttonElem);
        buttonElem.buttonInfo = button;
      }

      if (defaultElem)
        defaultElem.classList.add("notification-button-default");
    }

    newitem.setAttribute("priority", aPriority);
    if (aPriority >= this.PRIORITY_CRITICAL_LOW)
      newitem.setAttribute("type", "critical");
    else if (aPriority <= this.PRIORITY_INFO_HIGH)
      newitem.setAttribute("type", "info");
    else
      newitem.setAttribute("type", "warning");

    if (!insertPos) {
      newitem.style.position = "fixed";
      newitem.style.top = "100%";
      newitem.style.marginTop = "-15px";
      newitem.style.opacity = "0";
    }
    this.stack.insertBefore(newitem, insertPos);
    // Can only insert the document fragment after the item has been created because
    // otherwise the XBL structure isn't there yet:
    if (labelIsDocFragment) {
      document.getAnonymousElementByAttribute(newitem, "anonid", "messageText")
        .appendChild(aLabel);
    }

    if (!insertPos)
      this._showNotification(newitem, true);

    // Fire event for accessibility APIs
    var event = document.createEvent("Events");
    event.initEvent("AlertActive", true, true);
    newitem.dispatchEvent(event);

    return newitem;
  }

  removeNotification(aItem, aSkipAnimation) {
    if (aItem == this.currentNotification)
      this.removeCurrentNotification(aSkipAnimation);
    else if (aItem != this._closedNotification)
      this._removeNotificationElement(aItem);
    return aItem;
  }

  _removeNotificationElement(aChild) {
    if (aChild.eventCallback)
      aChild.eventCallback("removed");
    this.stack.removeChild(aChild);

    // make sure focus doesn't get lost (workaround for bug 570835)
    if (!Services.focus.getFocusedElementForWindow(window, false, {})) {
      Services.focus.moveFocus(window, this.stack,
                               Services.focus.MOVEFOCUS_FORWARD, 0);
    }
  }

  removeCurrentNotification(aSkipAnimation) {
    this._showNotification(this.currentNotification, false, aSkipAnimation);
  }

  removeAllNotifications(aImmediate) {
    var notifications = this.allNotifications;
    for (var n = notifications.length - 1; n >= 0; n--) {
      if (aImmediate)
        this._removeNotificationElement(notifications[n]);
      else
        this.removeNotification(notifications[n]);
    }
    this.currentNotification = null;

    // Clean up any currently-animating notification; this is necessary
    // if a notification was just opened and is still animating, but we
    // want to close it *without* animating.  This can even happen if
    // the user toggled `toolkit.cosmeticAnimations.enabled` to false
    // and called this method immediately after an animated notification
    // displayed (although this case isn't very likely).
    if (aImmediate || !this._allowAnimation)
      this._finishAnimation();
  }

  removeTransientNotifications() {
    var notifications = this.allNotifications;
    for (var n = notifications.length - 1; n >= 0; n--) {
      var notification = notifications[n];
      if (notification.persistence)
        notification.persistence--;
      else if (Date.now() > notification.timeout)
        this.removeNotification(notification);
    }
  }

  _showNotification(aNotification, aSlideIn, aSkipAnimation) {
    this._finishAnimation();

    var height = aNotification.boxObject.height;
    var skipAnimation = aSkipAnimation || height == 0 ||
      !this._allowAnimation;
    aNotification.classList.toggle("animated", !skipAnimation);

    if (aSlideIn) {
      this.currentNotification = aNotification;
      aNotification.style.removeProperty("position");
      aNotification.style.removeProperty("top");
      aNotification.style.removeProperty("margin-top");
      aNotification.style.removeProperty("opacity");

      if (skipAnimation) {
        return;
      }
    } else {
      this._closedNotification = aNotification;
      var notifications = this.allNotifications;
      var idx = notifications.length - 1;
      this.currentNotification = (idx >= 0) ? notifications[idx] : null;

      if (skipAnimation) {
        this._removeNotificationElement(this._closedNotification);
        delete this._closedNotification;
        return;
      }

      aNotification.style.marginTop = -height + "px";
      aNotification.style.opacity = 0;
    }

    this._animating = true;
  }

  _finishAnimation() {
    if (this._animating) {
      this._animating = false;
      if (this._closedNotification) {
        this._removeNotificationElement(this._closedNotification);
        delete this._closedNotification;
      }
    }
  }
};

// These are defined on the instance prototype for backwards compatibility.
Object.assign(MozElements.NotificationBox.prototype, {
  PRIORITY_INFO_LOW: 1,
  PRIORITY_INFO_MEDIUM: 2,
  PRIORITY_INFO_HIGH: 3,
  PRIORITY_WARNING_LOW: 4,
  PRIORITY_WARNING_MEDIUM: 5,
  PRIORITY_WARNING_HIGH: 6,
  PRIORITY_CRITICAL_LOW: 7,
  PRIORITY_CRITICAL_MEDIUM: 8,
  PRIORITY_CRITICAL_HIGH: 9,
});
