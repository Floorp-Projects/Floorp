/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["PopupNotifications"];

var Cc = Components.classes, Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/Services.jsm");

const NOTIFICATION_EVENT_DISMISSED = "dismissed";
const NOTIFICATION_EVENT_REMOVED = "removed";
const NOTIFICATION_EVENT_SHOWN = "shown";

const ICON_SELECTOR = ".notification-anchor-icon";
const ICON_ATTRIBUTE_SHOWING = "showing";

/**
 * Notification object describes a single popup notification.
 *
 * @see PopupNotifications.show()
 */
function Notification(id, message, anchorID, mainAction, secondaryActions,
                      browser, owner, options) {
  this.id = id;
  this.message = message;
  this.anchorID = anchorID;
  this.mainAction = mainAction;
  this.secondaryActions = secondaryActions || [];
  this.browser = browser;
  this.owner = owner;
  this.options = options || {};
}

Notification.prototype = {

  id: null,
  message: null,
  anchorID: null,
  mainAction: null,
  secondaryActions: null,
  browser: null,
  owner: null,
  options: null,

  /**
   * Removes the notification and updates the popup accordingly if needed.
   */
  remove: function Notification_remove() {
    this.owner.remove(this);
  },

  get anchorElement() {
    let iconBox = this.owner.iconBox;
    if (!iconBox)
      return null;

    let anchorElement = null;
    if (this.anchorID)
      anchorElement = iconBox.querySelector("#"+this.anchorID);

    // Use a default anchor icon if it's available
    if (!anchorElement)
      anchorElement = iconBox.querySelector("#default-notification-icon") ||
                      iconBox;

    return anchorElement;
  }
};

/**
 * The PopupNotifications object manages popup notifications for a given browser
 * window.
 * @param tabbrowser
 *        window's <xul:tabbrowser/>. Used to observe tab switching events and
 *        for determining the active browser element.
 * @param panel
 *        The <xul:panel/> element to use for notifications. The panel is
 *        populated with <popupnotification> children and displayed it as
 *        needed.
 * @param iconBox
 *        Reference to a container element that should be hidden or
 *        unhidden when notifications are hidden or shown. It should be the
 *        parent of anchor elements whose IDs are passed to show().
 *        It is used as a fallback popup anchor if notifications specify
 *        invalid or non-existent anchor IDs.
 */
function PopupNotifications(tabbrowser, panel, iconBox) {
  if (!(tabbrowser instanceof Ci.nsIDOMXULElement))
    throw "Invalid tabbrowser";
  if (iconBox && !(iconBox instanceof Ci.nsIDOMXULElement))
    throw "Invalid iconBox";
  if (!(panel instanceof Ci.nsIDOMXULElement))
    throw "Invalid panel";

  this.window = tabbrowser.ownerDocument.defaultView;
  this.panel = panel;
  this.tabbrowser = tabbrowser;
  this.iconBox = iconBox;

  this.panel.addEventListener("popuphidden", this, true);

  this.window.addEventListener("activate", this, true);
  this.tabbrowser.tabContainer.addEventListener("TabSelect", this, true);
}

PopupNotifications.prototype = {

  window: null,
  panel: null,
  tabbrowser: null,

  _iconBox: null,
  set iconBox(iconBox) {
    // Remove the listeners on the old iconBox, if needed
    if (this._iconBox) {
      this._iconBox.removeEventListener("click", this, false);
      this._iconBox.removeEventListener("keypress", this, false);
    }
    this._iconBox = iconBox;
    if (iconBox) {
      iconBox.addEventListener("click", this, false);
      iconBox.addEventListener("keypress", this, false);
    }
  },
  get iconBox() {
    return this._iconBox;
  },

  /**
   * Retrieve a Notification object associated with the browser/ID pair.
   * @param id
   *        The Notification ID to search for.
   * @param browser
   *        The browser whose notifications should be searched. If null, the
   *        currently selected browser's notifications will be searched.
   *
   * @returns the corresponding Notification object, or null if no such
   *          notification exists.
   */
  getNotification: function PopupNotifications_getNotification(id, browser) {
    let n = null;
    let notifications = this._getNotificationsForBrowser(browser || this.tabbrowser.selectedBrowser);
    notifications.some(function(x) x.id == id && (n = x));
    return n;
  },

  /**
   * Adds a new popup notification.
   * @param browser
   *        The <xul:browser> element associated with the notification. Must not
   *        be null.
   * @param id
   *        A unique ID that identifies the type of notification (e.g.
   *        "geolocation"). Only one notification with a given ID can be visible
   *        at a time. If a notification already exists with the given ID, it
   *        will be replaced.
   * @param message
   *        The text to be displayed in the notification.
   * @param anchorID
   *        The ID of the element that should be used as this notification
   *        popup's anchor. May be null, in which case the notification will be
   *        anchored to the iconBox.
   * @param mainAction
   *        A JavaScript object literal describing the notification button's
   *        action. If present, it must have the following properties:
   *          - label (string): the button's label.
   *          - accessKey (string): the button's accessKey.
   *          - callback (function): a callback to be invoked when the button is
   *            pressed.
   *        If null, the notification will not have a button, and
   *        secondaryActions will be ignored.
   * @param secondaryActions
   *        An optional JavaScript array describing the notification's alternate
   *        actions. The array should contain objects with the same properties
   *        as mainAction. These are used to populate the notification button's
   *        dropdown menu.
   * @param options
   *        An options JavaScript object holding additional properties for the
   *        notification. The following properties are currently supported:
   *        persistence: An integer. The notification will not automatically
   *                     dismiss for this many page loads.
   *        timeout:     A time in milliseconds. The notification will not
   *                     automatically dismiss before this time.
   *        persistWhileVisible:
   *                     A boolean. If true, a visible notification will always
   *                     persist across location changes.
   *        dismissed:   Whether the notification should be added as a dismissed
   *                     notification. Dismissed notifications can be activated
   *                     by clicking on their anchorElement.
   *        eventCallback:
   *                     Callback to be invoked when the notification changes
   *                     state. The callback's first argument is a string
   *                     identifying the state change:
   *                     "dismissed": notification has been dismissed by the
   *                                  user (e.g. by clicking away or switching
   *                                  tabs)
   *                     "removed": notification has been removed (due to
   *                                location change or user action)
   *                     "shown": notification has been shown (this can be fired
   *                              multiple times as notifications are dismissed
   *                              and re-shown)
   *        neverShow:   Indicate that no popup should be shown for this
   *                     notification. Useful for just showing the anchor icon.
   *        removeOnDismissal:
   *                     Notifications with this parameter set to true will be
   *                     removed when they would have otherwise been dismissed
   *                     (i.e. any time the popup is closed due to user
   *                     interaction).
   *        popupIconURL:
   *                     A string. URL of the image to be displayed in the popup.
   *                     Normally specified in CSS using list-style-image and the
   *                     .popup-notification-icon[popupid=...] selector.
   * @returns the Notification object corresponding to the added notification.
   */
  show: function PopupNotifications_show(browser, id, message, anchorID,
                                         mainAction, secondaryActions, options) {
    function isInvalidAction(a) {
      return !a || !(typeof(a.callback) == "function") || !a.label || !a.accessKey;
    }

    if (!browser)
      throw "PopupNotifications_show: invalid browser";
    if (!id)
      throw "PopupNotifications_show: invalid ID";
    if (mainAction && isInvalidAction(mainAction))
      throw "PopupNotifications_show: invalid mainAction";
    if (secondaryActions && secondaryActions.some(isInvalidAction))
      throw "PopupNotifications_show: invalid secondaryActions";

    let notification = new Notification(id, message, anchorID, mainAction,
                                        secondaryActions, browser, this, options);

    if (options && options.dismissed)
      notification.dismissed = true;

    let existingNotification = this.getNotification(id, browser);
    if (existingNotification)
      this._remove(existingNotification);

    let notifications = this._getNotificationsForBrowser(browser);
    notifications.push(notification);

    let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
    if (browser == this.tabbrowser.selectedBrowser && fm.activeWindow == this.window) {
      // show panel now
      this._update(notification.anchorElement);
    } else {
      // Otherwise, update() will display the notification the next time the
      // relevant tab/window is selected.

      // Notify observers that we're not showing the popup (useful for testing)
      this._notify("backgroundShow");
    }

    return notification;
  },

  /**
   * Returns true if the notification popup is currently being displayed.
   */
  get isPanelOpen() {
    let panelState = this.panel.state;

    return panelState == "showing" || panelState == "open";
  },

  /**
   * Called by the consumer to indicate that the current browser's location has
   * changed, so that we can update the active notifications accordingly.
   */
  locationChange: function PopupNotifications_locationChange() {
    this._currentNotifications = this._currentNotifications.filter(function(notification) {
      // The persistWhileVisible option allows an open notification to persist
      // across location changes
      if (notification.options.persistWhileVisible &&
          this.isPanelOpen) {
        if ("persistence" in notification.options &&
          notification.options.persistence)
          notification.options.persistence--;
        return true;
      }

      // The persistence option allows a notification to persist across multiple
      // page loads
      if ("persistence" in notification.options &&
          notification.options.persistence) {
        notification.options.persistence--;
        return true;
      }

      // The timeout option allows a notification to persist until a certain time
      if ("timeout" in notification.options &&
          Date.now() <= notification.options.timeout) {
        return true;
      }

      this._fireCallback(notification, NOTIFICATION_EVENT_REMOVED);
      return false;
    }, this);

    this._update();
  },

  /**
   * Removes a Notification.
   * @param notification
   *        The Notification object to remove.
   */
  remove: function PopupNotifications_remove(notification) {
    let isCurrent = this._currentNotifications.indexOf(notification) != -1;
    this._remove(notification);

    // update the panel, if needed
    if (isCurrent)
      this._update();
  },

  handleEvent: function (aEvent) {
    switch (aEvent.type) {
      case "popuphidden":
        this._onPopupHidden(aEvent);
        break;
      case "activate":
      case "TabSelect":
        let self = this;
        // setTimeout(..., 0) needed, otherwise openPopup from "activate" event
        // handler results in the popup being hidden again for some reason...
        this.window.setTimeout(function () {
          self._update();
        }, 0);
        break;
      case "click":
      case "keypress":
        this._onIconBoxCommand(aEvent);
        break;
    }
  },

////////////////////////////////////////////////////////////////////////////////
// Utility methods
////////////////////////////////////////////////////////////////////////////////

  _ignoreDismissal: null,
  _currentAnchorElement: null,

  /**
   * Gets and sets notifications for the currently selected browser.
   */
  get _currentNotifications() {
    return this._getNotificationsForBrowser(this.tabbrowser.selectedBrowser);
  },
  set _currentNotifications(a) {
    return this.tabbrowser.selectedBrowser.popupNotifications = a;
  },

  _remove: function PopupNotifications_removeHelper(notification) {
    // This notification may already be removed, in which case let's just fail
    // silently.
    let notifications = this._getNotificationsForBrowser(notification.browser);
    if (!notifications)
      return;

    var index = notifications.indexOf(notification);
    if (index == -1)
      return;

    notification.anchorElement.removeAttribute(ICON_ATTRIBUTE_SHOWING);

    // remove the notification
    notifications.splice(index, 1);
    this._fireCallback(notification, NOTIFICATION_EVENT_REMOVED);
  },

  /**
   * Dismisses the notification without removing it.
   */
  _dismiss: function PopupNotifications_dismiss() {
    let browser = this.panel.firstChild &&
                  this.panel.firstChild.notification.browser;
    this.panel.hidePopup();
    if (browser)
      browser.focus();
  },

  /**
   * Hides the notification popup.
   */
  _hidePanel: function PopupNotifications_hide() {
    this._ignoreDismissal = true;
    this.panel.hidePopup();
    this._ignoreDismissal = false;
  },

  /**
   *
   */
  _refreshPanel: function PopupNotifications_refreshPanel(notificationsToShow) {
    while (this.panel.lastChild)
      this.panel.removeChild(this.panel.lastChild);

    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    notificationsToShow.forEach(function (n) {
      let doc = this.window.document;
      let popupnotification = doc.createElementNS(XUL_NS, "popupnotification");
      popupnotification.setAttribute("label", n.message);
      // Append "-notification" to the ID to try to avoid ID conflicts with other stuff
      // in the document.
      popupnotification.setAttribute("id", n.id + "-notification");
      popupnotification.setAttribute("popupid", n.id);
      popupnotification.setAttribute("closebuttoncommand", "PopupNotifications._dismiss();");
      if (n.mainAction) {
        popupnotification.setAttribute("buttonlabel", n.mainAction.label);
        popupnotification.setAttribute("buttonaccesskey", n.mainAction.accessKey);
        popupnotification.setAttribute("buttoncommand", "PopupNotifications._onButtonCommand(event);");
        popupnotification.setAttribute("menucommand", "PopupNotifications._onMenuCommand(event);");
        popupnotification.setAttribute("closeitemcommand", "PopupNotifications._dismiss();event.stopPropagation();");
      }
      if (n.options.popupIconURL)
        popupnotification.setAttribute("icon", n.options.popupIconURL);
      popupnotification.notification = n;

      if (n.secondaryActions) {
        n.secondaryActions.forEach(function (a) {
          let item = doc.createElementNS(XUL_NS, "menuitem");
          item.setAttribute("label", a.label);
          item.setAttribute("accesskey", a.accessKey);
          item.notification = n;
          item.action = a;

          popupnotification.appendChild(item);
        }, this);
  
        if (n.secondaryActions.length) {
          let closeItemSeparator = doc.createElementNS(XUL_NS, "menuseparator");
          popupnotification.appendChild(closeItemSeparator);
        }
      }

      this.panel.appendChild(popupnotification);
    }, this);
  },

  _showPanel: function PopupNotifications_showPanel(notificationsToShow, anchorElement) {
    this.panel.hidden = false;

    this._refreshPanel(notificationsToShow);

    if (this.isPanelOpen && this._currentAnchorElement == anchorElement)
      return;

    // If the panel is already open but we're changing anchors, we need to hide
    // it first.  Otherwise it can appear in the wrong spot.  (_hidePanel is
    // safe to call even if the panel is already hidden.)
    this._hidePanel();

    // If the anchor element is hidden or null, use the tab as the anchor. We
    // only ever show notifications for the current browser, so we can just use
    // the current tab.
    let selectedTab = this.tabbrowser.selectedTab;
    if (anchorElement) {
      let bo = anchorElement.boxObject;
      if (bo.height == 0 && bo.width == 0)
        anchorElement = selectedTab; // hidden
    } else {
      anchorElement = selectedTab; // null
    }

    this._currentAnchorElement = anchorElement;

    this.panel.openPopup(anchorElement, "bottomcenter topleft");
    notificationsToShow.forEach(function (n) {
      this._fireCallback(n, NOTIFICATION_EVENT_SHOWN);
    }, this);
  },

  /**
   * Updates the notification state in response to window activation or tab
   * selection changes.
   */
  _update: function PopupNotifications_update(anchor) {
    if (this.iconBox) {
      // hide icons of the previous tab.
      this._hideIcons();
    }

    let anchorElement, notificationsToShow = [];
    let currentNotifications = this._currentNotifications;
    let haveNotifications = currentNotifications.length > 0;
    if (haveNotifications) {
      // Only show the notifications that have the passed-in anchor (or the
      // first notification's anchor, if none was passed in). Other
      // notifications will be shown once these are dismissed.
      anchorElement = anchor || currentNotifications[0].anchorElement;

      if (this.iconBox) {
        this._showIcons(currentNotifications);
        this.iconBox.hidden = false;
      }

      // Also filter out notifications that have been dismissed.
      notificationsToShow = currentNotifications.filter(function (n) {
        return !n.dismissed && n.anchorElement == anchorElement &&
               !n.options.neverShow;
      });
    }

    if (notificationsToShow.length > 0) {
      this._showPanel(notificationsToShow, anchorElement);
    } else {
      // Notify observers that we're not showing the popup (useful for testing)
      this._notify("updateNotShowing");

      // Dismiss the panel if needed. _onPopupHidden will ensure we never call
      // a dismissal handler on a notification that's been removed.
      this._dismiss();

      // Only hide the iconBox if we actually have no notifications (as opposed
      // to not having any showable notifications)
      if (this.iconBox && !haveNotifications)
        this.iconBox.hidden = true;
    }
  },

  _showIcons: function PopupNotifications_showIcons(aCurrentNotifications) {
    for (let notification of aCurrentNotifications) {
      let anchorElm = notification.anchorElement;
      if (anchorElm) {
        anchorElm.setAttribute(ICON_ATTRIBUTE_SHOWING, "true");
      }
    }
  },

  _hideIcons: function PopupNotifications_hideIcons() {
    let icons = this.iconBox.querySelectorAll(ICON_SELECTOR);
    for (let icon of icons) {
      icon.removeAttribute(ICON_ATTRIBUTE_SHOWING);
    }
  },

  _getNotificationsForBrowser: function PopupNotifications_getNotifications(browser) {
    if (browser.popupNotifications)
      return browser.popupNotifications;

    return browser.popupNotifications = [];
  },

  _onIconBoxCommand: function PopupNotifications_onIconBoxCommand(event) {
    // Left click, space or enter only
    let type = event.type;
    if (type == "click" && event.button != 0)
      return;

    if (type == "keypress" &&
        !(event.charCode == Ci.nsIDOMKeyEvent.DOM_VK_SPACE ||
          event.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN))
      return;

    if (this._currentNotifications.length == 0)
      return;

    // Get the anchor that is the immediate child of the icon box
    let anchor = event.target;
    while (anchor && anchor.parentNode != this.iconBox)
      anchor = anchor.parentNode;

    // Mark notifications anchored to this anchor as un-dismissed
    this._currentNotifications.forEach(function (n) {
      if (n.anchorElement == anchor)
        n.dismissed = false;
    });

    // ...and then show them.
    this._update(anchor);
  },

  _fireCallback: function PopupNotifications_fireCallback(n, event) {
    if (n.options.eventCallback)
      n.options.eventCallback.call(n, event);
  },

  _onPopupHidden: function PopupNotifications_onPopupHidden(event) {
    if (event.target != this.panel || this._ignoreDismissal)
      return;

    let browser = this.panel.firstChild &&
                  this.panel.firstChild.notification.browser;
    if (!browser)
      return;

    let notifications = this._getNotificationsForBrowser(browser);
    // Mark notifications as dismissed and call dismissal callbacks
    Array.forEach(this.panel.childNodes, function (nEl) {
      let notificationObj = nEl.notification;
      // Never call a dismissal handler on a notification that's been removed.
      if (notifications.indexOf(notificationObj) == -1)
        return;

      // Do not mark the notification as dismissed or fire NOTIFICATION_EVENT_DISMISSED
      // if the notification is removed.
      if (notificationObj.options.removeOnDismissal)
        this._remove(notificationObj);
      else {
        notificationObj.dismissed = true;
        this._fireCallback(notificationObj, NOTIFICATION_EVENT_DISMISSED);
      }
    }, this);

    while (this.panel.lastChild)
      this.panel.removeChild(this.panel.lastChild);

    this._update();
  },

  _onButtonCommand: function PopupNotifications_onButtonCommand(event) {
    // Need to find the associated notification object, which is a bit tricky
    // since it isn't associated with the button directly - this is kind of
    // gross and very dependent on the structure of the popupnotification
    // binding's content.
    let target = event.originalTarget;
    let notificationEl;
    let parent = target;
    while (parent && (parent = target.ownerDocument.getBindingParent(parent)))
      notificationEl = parent;

    if (!notificationEl)
      throw "PopupNotifications_onButtonCommand: couldn't find notification element";

    if (!notificationEl.notification)
      throw "PopupNotifications_onButtonCommand: couldn't find notification";

    let notification = notificationEl.notification;
    notification.mainAction.callback.call();

    this._remove(notification);
    this._update();
  },

  _onMenuCommand: function PopupNotifications_onMenuCommand(event) {
    let target = event.originalTarget;
    if (!target.action || !target.notification)
      throw "menucommand target has no associated action/notification";

    event.stopPropagation();
    target.action.callback.call();

    this._remove(target.notification);
    this._update();
  },

  _notify: function PopupNotifications_notify(topic) {
    Services.obs.notifyObservers(null, "PopupNotifications-" + topic, "");
  },
};
