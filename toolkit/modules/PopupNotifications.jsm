/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PopupNotifications"];

var Cc = Components.classes, Ci = Components.interfaces, Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PrivateBrowsingUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const NOTIFICATION_EVENT_DISMISSED = "dismissed";
const NOTIFICATION_EVENT_REMOVED = "removed";
const NOTIFICATION_EVENT_SHOWING = "showing";
const NOTIFICATION_EVENT_SHOWN = "shown";
const NOTIFICATION_EVENT_SWAPPING = "swapping";

const ICON_SELECTOR = ".notification-anchor-icon";
const ICON_ATTRIBUTE_SHOWING = "showing";
const ICON_ANCHOR_ATTRIBUTE = "popupnotificationanchor";

const PREF_SECURITY_DELAY = "security.notification_enable_delay";

// Enumerated values for the POPUP_NOTIFICATION_STATS telemetry histogram.
const TELEMETRY_STAT_OFFERED = 0;
const TELEMETRY_STAT_ACTION_1 = 1;
const TELEMETRY_STAT_ACTION_2 = 2;
const TELEMETRY_STAT_ACTION_3 = 3;
const TELEMETRY_STAT_ACTION_LAST = 4;
const TELEMETRY_STAT_DISMISSAL_CLICK_ELSEWHERE = 5;
const TELEMETRY_STAT_DISMISSAL_LEAVE_PAGE = 6;
const TELEMETRY_STAT_DISMISSAL_CLOSE_BUTTON = 7;
const TELEMETRY_STAT_OPEN_SUBMENU = 10;
const TELEMETRY_STAT_LEARN_MORE = 11;

const TELEMETRY_STAT_REOPENED_OFFSET = 20;

var popupNotificationsMap = new WeakMap();
var gNotificationParents = new WeakMap;

function getAnchorFromBrowser(aBrowser, aAnchorID) {
  let attrPrefix = aAnchorID ? aAnchorID.replace("notification-icon", "") : "";
  let anchor = aBrowser.getAttribute(attrPrefix + ICON_ANCHOR_ATTRIBUTE) ||
               aBrowser[attrPrefix + ICON_ANCHOR_ATTRIBUTE] ||
               aBrowser.getAttribute(ICON_ANCHOR_ATTRIBUTE) ||
               aBrowser[ICON_ANCHOR_ATTRIBUTE];
  if (anchor) {
    if (anchor instanceof Ci.nsIDOMXULElement) {
      return anchor;
    }
    return aBrowser.ownerDocument.getElementById(anchor);
  }
  return null;
}

function getNotificationFromElement(aElement) {
  // Need to find the associated notification object, which is a bit tricky
  // since it isn't associated with the element directly - this is kind of
  // gross and very dependent on the structure of the popupnotification
  // binding's content.
  let notificationEl;
  let parent = aElement;
  while (parent && (parent = aElement.ownerDocument.getBindingParent(parent)))
    notificationEl = parent;
  return notificationEl;
}

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

  this._dismissed = false;
  // Will become a boolean when manually toggled by the user.
  this._checkboxChecked = null;
  this.wasDismissed = false;
  this.recordedTelemetryStats = new Set();
  this.isPrivate = PrivateBrowsingUtils.isWindowPrivate(
                                        this.browser.ownerGlobal);
  this.timeCreated = this.owner.window.performance.now();
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
  timeShown: null,

  /**
   * Indicates whether the notification is currently dismissed.
   */
  set dismissed(value) {
    this._dismissed = value;
    if (value) {
      // Keep the dismissal into account when recording telemetry.
      this.wasDismissed = true;
    }
  },
  get dismissed() {
    return this._dismissed;
  },

  /**
   * Removes the notification and updates the popup accordingly if needed.
   */
  remove: function Notification_remove() {
    this.owner.remove(this);
  },

  get anchorElement() {
    let iconBox = this.owner.iconBox;

    let anchorElement = getAnchorFromBrowser(this.browser, this.anchorID);
    if (!iconBox)
      return anchorElement;

    if (!anchorElement && this.anchorID)
      anchorElement = iconBox.querySelector("#" + this.anchorID);

    // Use a default anchor icon if it's available
    if (!anchorElement)
      anchorElement = iconBox.querySelector("#default-notification-icon") ||
                      iconBox;

    return anchorElement;
  },

  reshow() {
    this.owner._reshowNotifications(this.anchorElement, this.browser);
  },

  /**
   * Adds a value to the specified histogram, that must be keyed by ID.
   */
  _recordTelemetry(histogramId, value) {
    if (this.isPrivate) {
      // The reason why we don't record telemetry in private windows is because
      // the available actions can be different from regular mode. The main
      // difference is that all of the persistent permission options like
      // "Always remember" aren't there, so they really need to be handled
      // separately to avoid skewing results. For notifications with the same
      // choices, there would be no reason not to record in private windows as
      // well, but it's just simpler to use the same check for everything.
      return;
    }
    let histogram = Services.telemetry.getKeyedHistogramById(histogramId);
    histogram.add("(all)", value);
    histogram.add(this.id, value);
  },

  /**
   * Adds an enumerated value to the POPUP_NOTIFICATION_STATS histogram,
   * ensuring that it is recorded at most once for each distinct Notification.
   *
   * Statistics for reopened notifications are recorded in separate buckets.
   *
   * @param value
   *        One of the TELEMETRY_STAT_ constants.
   */
  _recordTelemetryStat(value) {
    if (this.wasDismissed) {
      value += TELEMETRY_STAT_REOPENED_OFFSET;
    }
    if (!this.recordedTelemetryStats.has(value)) {
      this.recordedTelemetryStats.add(value);
      this._recordTelemetry("POPUP_NOTIFICATION_STATS", value);
    }
  },
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
 * @param options
 *        An optional object with the following optional properties:
 *        {
 *          shouldSuppress:
 *            If this function returns true, then all notifications are
 *            suppressed for this window. This state is checked on construction
 *            and when the "anchorVisibilityChange" method is called.
 *        }
 */
this.PopupNotifications = function PopupNotifications(tabbrowser, panel,
                                                      iconBox, options = {}) {
  if (!(tabbrowser instanceof Ci.nsIDOMXULElement))
    throw "Invalid tabbrowser";
  if (iconBox && !(iconBox instanceof Ci.nsIDOMXULElement))
    throw "Invalid iconBox";
  if (!(panel instanceof Ci.nsIDOMXULElement))
    throw "Invalid panel";

  this._shouldSuppress = options.shouldSuppress || (() => false);
  this._suppress = this._shouldSuppress();

  this.window = tabbrowser.ownerGlobal;
  this.panel = panel;
  this.tabbrowser = tabbrowser;
  this.iconBox = iconBox;
  this.buttonDelay = Services.prefs.getIntPref(PREF_SECURITY_DELAY);

  this.panel.addEventListener("popuphidden", this, true);
  this.panel.classList.add("popup-notification-panel");

  // This listener will be attached to the chrome window whenever a notification
  // is showing, to allow the user to dismiss notifications using the escape key.
  this._handleWindowKeyPress = aEvent => {
    if (aEvent.keyCode != aEvent.DOM_VK_ESCAPE) {
      return;
    }

    // Esc key cancels the topmost notification, if there is one.
    let notification = this.panel.firstChild;
    if (!notification) {
      return;
    }

    let doc = this.window.document;
    let focusedElement = Services.focus.focusedElement;

    // If the chrome window has a focused element, let it handle the ESC key instead.
    if (!focusedElement ||
        focusedElement == doc.body ||
        focusedElement == this.tabbrowser.selectedBrowser ||
        // Ignore focused elements inside the notification.
        getNotificationFromElement(focusedElement) == notification ||
        notification.contains(focusedElement)) {
      this._onButtonEvent(aEvent, "secondarybuttoncommand", notification);
    }
  };

  // There are no anchor icons in DOM fullscreen mode, but we would
  // still like to show the popup notification. To avoid an infinite
  // loop of showing and hiding, we have to disable followanchor
  // (which hides the element without an anchor) in fullscreen.
  this.window.addEventListener("MozDOMFullscreen:Entered", () => {
    this.panel.setAttribute("followanchor", "false");
  }, true);
  this.window.addEventListener("MozDOMFullscreen:Exited", () => {
    this.panel.setAttribute("followanchor", "true");
  }, true);

  this.window.addEventListener("activate", this, true);
  if (this.tabbrowser.tabContainer)
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
      this._iconBox.removeEventListener("click", this);
      this._iconBox.removeEventListener("keypress", this);
    }
    this._iconBox = iconBox;
    if (iconBox) {
      iconBox.addEventListener("click", this);
      iconBox.addEventListener("keypress", this);
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
    let notifications = this._getNotificationsForBrowser(browser || this.tabbrowser.selectedBrowser);
    return notifications.find(x => x.id == id) || null;
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
   *            pressed, is passed an object that contains the following fields:
   *              - checkboxChecked: (boolean) If the optional checkbox is checked.
   *          - [optional] dismiss (boolean): If this is true, the notification
   *            will be dismissed instead of removed after running the callback.
   *        If null, the notification will have a default "OK" action button
   *        that can be used to dismiss the popup and secondaryActions will be ignored.
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
   *        persistent:  A boolean. If true, the notification will always
   *                     persist even across tab and app changes (but not across
   *                     location changes), until the user accepts or rejects
   *                     the request. The notification will never be implicitly
   *                     dismissed.
   *        dismissed:   Whether the notification should be added as a dismissed
   *                     notification. Dismissed notifications can be activated
   *                     by clicking on their anchorElement.
   *        autofocus:   Whether the notification should be autofocused on
   *                     showing, stealing focus from any other focused element.
   *        eventCallback:
   *                     Callback to be invoked when the notification changes
   *                     state. The callback's first argument is a string
   *                     identifying the state change:
   *                     "dismissed": notification has been dismissed by the
   *                                  user (e.g. by clicking away or switching
   *                                  tabs)
   *                     "removed": notification has been removed (due to
   *                                location change or user action)
   *                     "showing": notification is about to be shown
   *                                (this can be fired multiple times as
   *                                 notifications are dismissed and re-shown)
   *                                If the callback returns true, the notification
   *                                will be dismissed.
   *                     "shown": notification has been shown (this can be fired
   *                              multiple times as notifications are dismissed
   *                              and re-shown)
   *                     "swapping": the docshell of the browser that created
   *                                 the notification is about to be swapped to
   *                                 another browser. A second parameter contains
   *                                 the browser that is receiving the docshell,
   *                                 so that the event callback can transfer stuff
   *                                 specific to this notification.
   *                                 If the callback returns true, the notification
   *                                 will be moved to the new browser.
   *                                 If the callback isn't implemented, returns false,
   *                                 or doesn't return any value, the notification
   *                                 will be removed.
   *        neverShow:   Indicate that no popup should be shown for this
   *                     notification. Useful for just showing the anchor icon.
   *        removeOnDismissal:
   *                     Notifications with this parameter set to true will be
   *                     removed when they would have otherwise been dismissed
   *                     (i.e. any time the popup is closed due to user
   *                     interaction).
   *        hideClose:   Indicate that the little close button in the corner of
   *                     the panel should be hidden.
   *        checkbox:    An object that allows you to add a checkbox and
   *                     control its behavior with these fields:
   *                       label:
   *                         (required) Label to be shown next to the checkbox.
   *                       checked:
   *                         (optional) Whether the checkbox should be checked
   *                         by default. Defaults to false.
   *                       checkedState:
   *                         (optional) An object that allows you to customize
   *                         the notification state when the checkbox is checked.
   *                           disableMainAction:
   *                             (optional) Whether the mainAction is disabled.
   *                             Defaults to false.
   *                           warningLabel:
   *                             (optional) A (warning) text that is shown below the
   *                             checkbox. Pass null to hide.
   *                       uncheckedState:
   *                         (optional) An object that allows you to customize
   *                         the notification state when the checkbox is not checked.
   *                         Has the same attributes as checkedState.
   *        popupIconClass:
   *                     A string. A class (or space separated list of classes)
   *                     that will be applied to the icon in the popup so that
   *                     several notifications using the same panel can use
   *                     different icons.
   *        popupIconURL:
   *                     A string. URL of the image to be displayed in the popup.
   *                     Normally specified in CSS using list-style-image and the
   *                     .popup-notification-icon[popupid=...] selector.
   *        learnMoreURL:
   *                     A string URL. Setting this property will make the
   *                     prompt display a "Learn More" link that, when clicked,
   *                     opens the URL in a new tab.
   *        displayURI:
   *                     The nsIURI of the page the notification came
   *                     from. If present, this will be displayed above the message.
   *                     If the nsIURI represents a file, the path will be displayed,
   *                     otherwise the hostPort will be displayed.
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

    let isActiveBrowser = this._isActiveBrowser(browser);
    let fm = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
    let isActiveWindow = fm.activeWindow == this.window;

    if (isActiveBrowser) {
      if (isActiveWindow) {

        // Autofocus if the notification requests focus.
        if (options && !options.dismissed && options.autofocus) {
          this.panel.removeAttribute("noautofocus");
        } else {
          this.panel.setAttribute("noautofocus", "true");
        }

        // show panel now
        this._update(notifications, new Set([notification.anchorElement]), true);
      } else {
        // indicate attention and update the icon if necessary
        if (!notification.dismissed) {
          this.window.getAttention();
        }
        this._updateAnchorIcons(notifications, this._getAnchorsForNotifications(
          notifications, notification.anchorElement));
        this._notify("backgroundShow");
      }

    } else {
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
   * Called by the consumer to indicate that a browser's location has changed,
   * so that we can update the active notifications accordingly.
   */
  locationChange: function PopupNotifications_locationChange(aBrowser) {
    if (!aBrowser)
      throw "PopupNotifications_locationChange: invalid browser";

    let notifications = this._getNotificationsForBrowser(aBrowser);

    notifications = notifications.filter(function(notification) {
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

    this._setNotificationsForBrowser(aBrowser, notifications);

    if (this._isActiveBrowser(aBrowser)) {
      this.anchorVisibilityChange();
    }
  },

  /**
   * Called by the consumer to indicate that the visibility of the notification
   * anchors may have changed, but the location has not changed. This also
   * checks whether all notifications are suppressed for this window.
   *
   * Calling this method may result in the "showing" and "shown" events for
   * visible notifications to be invoked even if the anchor has not changed.
   */
  anchorVisibilityChange() {
    let suppress = this._shouldSuppress();
    if (!suppress) {
      // If notifications are not suppressed, always update the visibility.
      this._suppress = false;
      let notifications =
        this._getNotificationsForBrowser(this.tabbrowser.selectedBrowser);
      this._update(notifications, this._getAnchorsForNotifications(notifications,
        getAnchorFromBrowser(this.tabbrowser.selectedBrowser)));
      return;
    }

    // Notifications are suppressed, ensure that the panel is hidden.
    if (!this._suppress) {
      this._suppress = true;
      this._hidePanel().catch(Cu.reportError);
    }
  },

  /**
   * Removes a Notification.
   * @param notification
   *        The Notification object to remove.
   */
  remove: function PopupNotifications_remove(notification) {
    this._remove(notification);

    if (this._isActiveBrowser(notification.browser)) {
      let notifications = this._getNotificationsForBrowser(notification.browser);
      this._update(notifications);
    }
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "popuphidden":
        this._onPopupHidden(aEvent);
        break;
      case "activate":
        if (this.isPanelOpen) {
          for (let elt of this.panel.children)
            elt.notification.timeShown = this.window.performance.now();
          break;
        }

      case "TabSelect":
        let self = this;
        // This is where we could detect if the panel is dismissed if the page
        // was switched. Unfortunately, the user usually has clicked elsewhere
        // at this point so this value only gets recorded for programmatic
        // reasons, like the "Learn More" link being clicked and resulting in a
        // tab switch.
        this.nextDismissReason = TELEMETRY_STAT_DISMISSAL_LEAVE_PAGE;
        // setTimeout(..., 0) needed, otherwise openPopup from "activate" event
        // handler results in the popup being hidden again for some reason...
        this.window.setTimeout(function() {
          self._update();
        }, 0);
        break;
      case "click":
      case "keypress":
        this._onIconBoxCommand(aEvent);
        break;
    }
  },

// Utility methods

  _ignoreDismissal: null,
  _currentAnchorElement: null,

  /**
   * Gets notifications for the currently selected browser.
   */
  get _currentNotifications() {
    return this.tabbrowser.selectedBrowser ? this._getNotificationsForBrowser(this.tabbrowser.selectedBrowser) : [];
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

    if (this._isActiveBrowser(notification.browser))
      notification.anchorElement.removeAttribute(ICON_ATTRIBUTE_SHOWING);

    // remove the notification
    notifications.splice(index, 1);
    this._fireCallback(notification, NOTIFICATION_EVENT_REMOVED);
  },

  /**
   * Dismisses the notification without removing it.
   */
  _dismiss: function PopupNotifications_dismiss(event, telemetryReason) {
    if (telemetryReason) {
      this.nextDismissReason = telemetryReason;
    }

    // An explicitly dismissed persistent notification effectively becomes
    // non-persistent.
    if (event && telemetryReason == TELEMETRY_STAT_DISMISSAL_CLOSE_BUTTON) {
      let notificationEl = getNotificationFromElement(event.target);
      if (notificationEl) {
        notificationEl.notification.options.persistent = false;
      }
    }

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
    if (this.panel.state == "closed") {
      return Promise.resolve();
    }
    if (this._ignoreDismissal) {
      return this._ignoreDismissal.promise;
    }
    let deferred = Promise.defer();
    this._ignoreDismissal = deferred;
    this.panel.hidePopup();
    return deferred.promise;
  },

  /**
   * Removes all notifications from the notification popup.
   */
  _clearPanel() {
    let popupnotification;
    while ((popupnotification = this.panel.lastChild)) {
      this.panel.removeChild(popupnotification);

      // If this notification was provided by the chrome document rather than
      // created ad hoc, move it back to where we got it from.
      let originalParent = gNotificationParents.get(popupnotification);
      if (originalParent) {
        popupnotification.notification = null;

        // Remove nodes dynamically added to the notification's menu button
        // in _refreshPanel.
        let contentNode = popupnotification.lastChild;
        while (contentNode) {
          let previousSibling = contentNode.previousSibling;
          if (contentNode.nodeName == "menuitem" ||
              contentNode.nodeName == "menuseparator")
            popupnotification.removeChild(contentNode);
          contentNode = previousSibling;
        }

        // Re-hide the notification such that it isn't rendered in the chrome
        // document. _refreshPanel will unhide it again when needed.
        popupnotification.hidden = true;

        originalParent.appendChild(popupnotification);
      }
    }
  },

  _refreshPanel: function PopupNotifications_refreshPanel(notificationsToShow) {
    this._clearPanel();

    const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    notificationsToShow.forEach(function(n) {
      let doc = this.window.document;

      // Append "-notification" to the ID to try to avoid ID conflicts with other stuff
      // in the document.
      let popupnotificationID = n.id + "-notification";

      // If the chrome document provides a popupnotification with this id, use
      // that. Otherwise create it ad-hoc.
      let popupnotification = doc.getElementById(popupnotificationID);
      if (popupnotification)
        gNotificationParents.set(popupnotification, popupnotification.parentNode);
      else
        popupnotification = doc.createElementNS(XUL_NS, "popupnotification");

      popupnotification.setAttribute("label", n.message);
      popupnotification.setAttribute("id", popupnotificationID);
      popupnotification.setAttribute("popupid", n.id);
      if (Services.prefs.getBoolPref("privacy.permissionPrompts.showCloseButton")) {
        popupnotification.setAttribute("closebuttoncommand", "PopupNotifications._onButtonEvent(event, 'secondarybuttoncommand');");
      } else {
        popupnotification.setAttribute("closebuttoncommand", `PopupNotifications._dismiss(event, ${TELEMETRY_STAT_DISMISSAL_CLOSE_BUTTON});`);
      }
      if (n.mainAction) {
        popupnotification.setAttribute("buttonlabel", n.mainAction.label);
        popupnotification.setAttribute("buttonaccesskey", n.mainAction.accessKey);
        popupnotification.setAttribute("buttoncommand", "PopupNotifications._onButtonEvent(event, 'buttoncommand');");
        popupnotification.setAttribute("dropmarkerpopupshown", "PopupNotifications._onButtonEvent(event, 'dropmarkerpopupshown');");
        popupnotification.setAttribute("learnmoreclick", "PopupNotifications._onButtonEvent(event, 'learnmoreclick');");
        popupnotification.setAttribute("menucommand", "PopupNotifications._onMenuCommand(event);");
      } else {
        // Enable the default button to let the user close the popup if the close button is hidden
        popupnotification.setAttribute("buttoncommand", "PopupNotifications._onButtonEvent(event, 'buttoncommand');");
        popupnotification.removeAttribute("buttonlabel");
        popupnotification.removeAttribute("buttonaccesskey");
        popupnotification.removeAttribute("dropmarkerpopupshown");
        popupnotification.removeAttribute("learnmoreclick");
        popupnotification.removeAttribute("menucommand");
      }

      if (n.options.popupIconClass) {
        let classes = "popup-notification-icon " + n.options.popupIconClass;
        popupnotification.setAttribute("iconclass", classes);
      }
      if (n.options.popupIconURL)
        popupnotification.setAttribute("icon", n.options.popupIconURL);

      if (n.options.learnMoreURL)
        popupnotification.setAttribute("learnmoreurl", n.options.learnMoreURL);
      else
        popupnotification.removeAttribute("learnmoreurl");

      if (n.options.displayURI) {
        let uri;
        try {
           if (n.options.displayURI instanceof Ci.nsIFileURL) {
            uri = n.options.displayURI.path;
          } else {
            uri = n.options.displayURI.hostPort;
          }
          popupnotification.setAttribute("origin", uri);
        } catch (e) {
          Cu.reportError(e);
          popupnotification.removeAttribute("origin");
        }
      } else
        popupnotification.removeAttribute("origin");

      if (n.options.hideClose)
        popupnotification.setAttribute("closebuttonhidden", "true");

      popupnotification.notification = n;

      if (n.mainAction && n.secondaryActions && n.secondaryActions.length > 0) {
        let telemetryStatId = TELEMETRY_STAT_ACTION_2;

        let secondaryAction = n.secondaryActions[0];
        popupnotification.setAttribute("secondarybuttonlabel", secondaryAction.label);
        popupnotification.setAttribute("secondarybuttonaccesskey", secondaryAction.accessKey);
        popupnotification.setAttribute("secondarybuttoncommand", "PopupNotifications._onButtonEvent(event, 'secondarybuttoncommand');");

        for (let i = 1; i < n.secondaryActions.length; i++) {
          let action = n.secondaryActions[i];
          let item = doc.createElementNS(XUL_NS, "menuitem");
          item.setAttribute("label", action.label);
          item.setAttribute("accesskey", action.accessKey);
          item.notification = n;
          item.action = action;

          popupnotification.appendChild(item);

          // We can only record a limited number of actions in telemetry. If
          // there are more, the latest are all recorded in the last bucket.
          item.action.telemetryStatId = telemetryStatId;
          if (telemetryStatId < TELEMETRY_STAT_ACTION_LAST) {
            telemetryStatId++;
          }
        }

        if (n.secondaryActions.length < 2) {
          popupnotification.setAttribute("dropmarkerhidden", "true");
        }
      } else {
        popupnotification.setAttribute("secondarybuttonhidden", "true");
        popupnotification.setAttribute("dropmarkerhidden", "true");
      }

      let checkbox = n.options.checkbox;
      if (checkbox && checkbox.label) {
        let checked = n._checkboxChecked != null ? n._checkboxChecked : !!checkbox.checked;

        popupnotification.setAttribute("checkboxhidden", "false");
        popupnotification.setAttribute("checkboxchecked", checked);
        popupnotification.setAttribute("checkboxlabel", checkbox.label);

        popupnotification.setAttribute("checkboxcommand", "PopupNotifications._onCheckboxCommand(event);");

        if (checked) {
          this._setNotificationUIState(popupnotification, checkbox.checkedState);
        } else {
          this._setNotificationUIState(popupnotification, checkbox.uncheckedState);
        }
      } else {
        popupnotification.setAttribute("checkboxhidden", "true");
        popupnotification.setAttribute("warninghidden", "true");
      }

      this.panel.appendChild(popupnotification);

      // The popupnotification may be hidden if we got it from the chrome
      // document rather than creating it ad hoc.
      popupnotification.hidden = false;
    }, this);
  },

  _setNotificationUIState(notification, state = {}) {
    if (state.disableMainAction) {
      notification.setAttribute("mainactiondisabled", "true");
    } else {
      notification.removeAttribute("mainactiondisabled");
    }
    if (state.warningLabel) {
      notification.setAttribute("warninglabel", state.warningLabel);
      notification.removeAttribute("warninghidden");
    } else {
      notification.setAttribute("warninghidden", "true");
    }
  },

  _onCheckboxCommand(event) {
    let notificationEl = getNotificationFromElement(event.originalTarget);
    let checked = notificationEl.checkbox.checked;
    let notification = notificationEl.notification;

    // Save checkbox state to be able to persist it when re-opening the doorhanger.
    notification._checkboxChecked = checked;

    if (checked) {
      this._setNotificationUIState(notificationEl, notification.options.checkbox.checkedState);
    } else {
      this._setNotificationUIState(notificationEl, notification.options.checkbox.uncheckedState);
    }
  },

  _showPanel: function PopupNotifications_showPanel(notificationsToShow, anchorElement) {
    this.panel.hidden = false;

    notificationsToShow = notificationsToShow.filter(n => {
      if (anchorElement != n.anchorElement) {
        return false;
      }

      let dismiss = this._fireCallback(n, NOTIFICATION_EVENT_SHOWING);
      if (dismiss)
        n.dismissed = true;
      return !dismiss;
    });
    if (!notificationsToShow.length)
      return;
    let notificationIds = notificationsToShow.map(n => n.id);

    this._refreshPanel(notificationsToShow);

    // If the anchor element is hidden or null, fall back to the identity icon.
    if (!anchorElement || (anchorElement.boxObject.height == 0 &&
                           anchorElement.boxObject.width == 0)) {
      anchorElement = this.window.document.getElementById("identity-icon");

      // If the identity icon is not available in this window, or maybe the
      // entire location bar is hidden for any reason, use the tab as the
      // anchor. We only ever show notifications for the current browser, so we
      // can just use the current tab.
      if (!anchorElement || (anchorElement.boxObject.height == 0 &&
                             anchorElement.boxObject.width == 0)) {
        anchorElement = this.tabbrowser.selectedTab;
      }
    }

    if (this.isPanelOpen && this._currentAnchorElement == anchorElement) {
      notificationsToShow.forEach(function(n) {
        this._fireCallback(n, NOTIFICATION_EVENT_SHOWN);
      }, this);

      // Make sure we update the noautohide attribute on the panel, in case it changed.
      if (notificationsToShow.some(n => n.options.persistent)) {
        this.panel.setAttribute("noautohide", "true");
      } else {
        this.panel.removeAttribute("noautohide");
      }

      // Let tests know that the panel was updated and what notifications it was
      // updated with so that tests can wait for the correct notifications to be
      // added.
      let event = new this.window.CustomEvent("PanelUpdated",
                                              {"detail": notificationIds});
      this.panel.dispatchEvent(event);
      return;
    }

    // If the panel is already open but we're changing anchors, we need to hide
    // it first.  Otherwise it can appear in the wrong spot.  (_hidePanel is
    // safe to call even if the panel is already hidden.)
    this._hidePanel().then(() => {
      this._currentAnchorElement = anchorElement;

      if (notificationsToShow.some(n => n.options.persistent)) {
        this.panel.setAttribute("noautohide", "true");
      } else {
        this.panel.removeAttribute("noautohide");
      }

      // On OS X and Linux we need a different panel arrow color for
      // click-to-play plugins, so copy the popupid and use css.
      this.panel.setAttribute("popupid", this.panel.firstChild.getAttribute("popupid"));
      notificationsToShow.forEach(function(n) {
        // Record that the notification was actually displayed on screen.
        // Notifications that were opened a second time or that were originally
        // shown with "options.dismissed" will be recorded in a separate bucket.
        n._recordTelemetryStat(TELEMETRY_STAT_OFFERED);
        // Remember the time the notification was shown for the security delay.
        n.timeShown = this.window.performance.now();
      }, this);

      // Unless the panel closing is triggered by a specific known code path,
      // the next reason will be that the user clicked elsewhere.
      this.nextDismissReason = TELEMETRY_STAT_DISMISSAL_CLICK_ELSEWHERE;

      let target = this.panel;
      if (target.parentNode) {
        // NOTIFICATION_EVENT_SHOWN should be fired for the panel before
        // anyone listening for popupshown on the panel gets run. Otherwise,
        // the panel will not be initialized when the popupshown event
        // listeners run.
        // By targeting the panel's parent and using a capturing listener, we
        // can have our listener called before others waiting for the panel to
        // be shown (which probably expect the panel to be fully initialized)
        target = target.parentNode;
      }
      if (this._popupshownListener) {
        target.removeEventListener("popupshown", this._popupshownListener, true);
      }
      this._popupshownListener = function(e) {
        target.removeEventListener("popupshown", this._popupshownListener, true);
        this._popupshownListener = null;

        notificationsToShow.forEach(function(n) {
          this._fireCallback(n, NOTIFICATION_EVENT_SHOWN);
        }, this);
        // These notifications are used by tests to know when all the processing
        // required to display the panel has happened.
        this.panel.dispatchEvent(new this.window.CustomEvent("Shown"));
        let event = new this.window.CustomEvent("PanelUpdated",
                                                {"detail": notificationIds});
        this.panel.dispatchEvent(event);
      };
      this._popupshownListener = this._popupshownListener.bind(this);
      target.addEventListener("popupshown", this._popupshownListener, true);

      this.panel.openPopup(anchorElement, "bottomcenter topleft");
    });
  },

  /**
   * Updates the notification state in response to window activation or tab
   * selection changes.
   *
   * @param notifications an array of Notification instances. if null,
   *                      notifications will be retrieved off the current
   *                      browser tab
   * @param anchors       is a XUL element or a Set of XUL elements that the
   *                      notifications panel(s) will be anchored to.
   * @param dismissShowing if true, dismiss any currently visible notifications
   *                       if there are no notifications to show. Otherwise,
   *                       currently displayed notifications will be left alone.
   */
  _update: function PopupNotifications_update(notifications, anchors = new Set(), dismissShowing = false) {
    if (anchors instanceof Ci.nsIDOMXULElement)
      anchors = new Set([anchors]);

    if (!notifications)
      notifications = this._currentNotifications;

    let haveNotifications = notifications.length > 0;
    if (!anchors.size && haveNotifications)
      anchors = this._getAnchorsForNotifications(notifications);

    let useIconBox = !!this.iconBox;
    if (useIconBox && anchors.size) {
      for (let anchor of anchors) {
        if (anchor.parentNode == this.iconBox)
          continue;
        useIconBox = false;
        break;
      }
    }

    // Filter out notifications that have been dismissed, unless they are
    // persistent. Also check if we should not show any notification.
    let notificationsToShow = [];
    if (!this._suppress) {
      notificationsToShow = notifications.filter(
        n => (!n.dismissed || n.options.persistent) && !n.options.neverShow);
    }

    if (useIconBox) {
      // Hide icons of the previous tab.
      this._hideIcons();
    }

    if (haveNotifications) {
      // Also filter out notifications that are for a different anchor.
      notificationsToShow = notificationsToShow.filter(function(n) {
        return anchors.has(n.anchorElement);
      });

      if (useIconBox) {
        this._showIcons(notifications);
        this.iconBox.hidden = false;
        // Make sure that panels can only be attached to anchors of shown
        // notifications inside an iconBox.
        anchors = this._getAnchorsForNotifications(notificationsToShow);
      } else if (anchors.size) {
        this._updateAnchorIcons(notifications, anchors);
      }
    }

    if (notificationsToShow.length > 0) {
      let anchorElement = anchors.values().next().value;
      if (anchorElement) {
        this._showPanel(notificationsToShow, anchorElement);
      }

      // Setup a capturing event listener on the whole window to catch the
      // escape key while persistent notifications are visible.
      this.window.addEventListener("keypress", this._handleWindowKeyPress, true);
    } else {
      // Notify observers that we're not showing the popup (useful for testing)
      this._notify("updateNotShowing");

      // Close the panel if there are no notifications to show.
      // When called from PopupNotifications.show() we should never close the
      // panel, however. It may just be adding a dismissed notification, in
      // which case we want to continue showing any existing notifications.
      if (!dismissShowing)
        this._dismiss();

      // Only hide the iconBox if we actually have no notifications (as opposed
      // to not having any showable notifications)
      if (!haveNotifications) {
        if (useIconBox) {
          this.iconBox.hidden = true;
        } else if (anchors.size) {
          for (let anchorElement of anchors)
            anchorElement.removeAttribute(ICON_ATTRIBUTE_SHOWING);
        }
      }

      // Stop listening to keyboard events for notifications.
      this.window.removeEventListener("keypress", this._handleWindowKeyPress, true);
    }
  },

  _updateAnchorIcons: function PopupNotifications_updateAnchorIcons(notifications,
                                                                    anchorElements) {
    for (let anchorElement of anchorElements) {
      anchorElement.setAttribute(ICON_ATTRIBUTE_SHOWING, "true");
      // Use the anchorID as a class along with the default icon class as a
      // fallback if anchorID is not defined in CSS. We always use the first
      // notifications icon, so in the case of multiple notifications we'll
      // only use the default icon.
      if (anchorElement.classList.contains("notification-anchor-icon")) {
        // remove previous icon classes
        let className = anchorElement.className.replace(/([-\w]+-notification-icon\s?)/g, "")
        if (notifications.length > 0) {
          // Find the first notification this anchor used for.
          let notification = notifications[0];
          for (let n of notifications) {
            if (n.anchorElement == anchorElement) {
              notification = n;
              break;
            }
          }
          // With this notification we can better approximate the most fitting
          // style.
          className = notification.anchorID + " " + className;
        }
        anchorElement.className = className;
      }
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

  /**
   * Gets and sets notifications for the browser.
   */
  _getNotificationsForBrowser: function PopupNotifications_getNotifications(browser) {
    let notifications = popupNotificationsMap.get(browser);
    if (!notifications) {
      // Initialize the WeakMap for the browser so callers can reference/manipulate the array.
      notifications = [];
      popupNotificationsMap.set(browser, notifications);
    }
    return notifications;
  },
  _setNotificationsForBrowser: function PopupNotifications_setNotifications(browser, notifications) {
    popupNotificationsMap.set(browser, notifications);
    return notifications;
  },

  _getAnchorsForNotifications: function PopupNotifications_getAnchorsForNotifications(notifications, defaultAnchor) {
    let anchors = new Set();
    for (let notification of notifications) {
      if (notification.anchorElement)
        anchors.add(notification.anchorElement)
    }
    if (defaultAnchor && !anchors.size)
      anchors.add(defaultAnchor);
    return anchors;
  },

  _isActiveBrowser(browser) {
    // We compare on frameLoader instead of just comparing the
    // selectedBrowser and browser directly because browser tabs in
    // Responsive Design Mode put the actual web content into a
    // mozbrowser iframe and proxy property read/write and method
    // calls from the tab to that iframe. This is so that attempts
    // to reload the tab end up reloading the content in
    // Responsive Design Mode, and not the Responsive Design Mode
    // viewer itself.
    //
    // This means that PopupNotifications can come up from a browser
    // in Responsive Design Mode, but the selectedBrowser will not match
    // the browser being passed into this function, despite the browser
    // actually being within the selected tab. We workaround this by
    // comparing frameLoader instead, which is proxied from the outer
    // <xul:browser> to the inner mozbrowser <iframe>.
    return this.tabbrowser.selectedBrowser.frameLoader == browser.frameLoader;
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

    event.stopPropagation();

    // Get the anchor that is the immediate child of the icon box
    let anchor = event.target;
    while (anchor && anchor.parentNode != this.iconBox)
      anchor = anchor.parentNode;

    if (!anchor) {
      return;
    }

    // If the panel is not closed, and the anchor is different, immediately mark all
    // active notifications for the previous anchor as dismissed
    if (this.panel.state != "closed" && anchor != this._currentAnchorElement) {
      this._dismissOrRemoveCurrentNotifications();
    }

    // Avoid reshowing notifications that are already shown and have not been dismissed.
    if (this.panel.state == "closed" || anchor != this._currentAnchorElement) {
      // As soon as the panel is shown, focus the first element in the selected notification.
      this.panel.addEventListener("popupshown",
        () => this.window.document.commandDispatcher.advanceFocusIntoSubtree(this.panel),
        {once: true});

      this._reshowNotifications(anchor);
    } else {
      // Focus the first element in the selected notification.
      this.window.document.commandDispatcher.advanceFocusIntoSubtree(this.panel);
    }
  },

  _reshowNotifications: function PopupNotifications_reshowNotifications(anchor, browser) {
    // Mark notifications anchored to this anchor as un-dismissed
    browser = browser || this.tabbrowser.selectedBrowser;
    let notifications = this._getNotificationsForBrowser(browser);
    notifications.forEach(function(n) {
      if (n.anchorElement == anchor)
        n.dismissed = false;
    });

    if (this._isActiveBrowser(browser)) {
      // ...and then show them.
      this._update(notifications, anchor);
    }
  },

  _swapBrowserNotifications: function PopupNotifications_swapBrowserNoficications(ourBrowser, otherBrowser) {
    // When swaping browser docshells (e.g. dragging tab to new window) we need
    // to update our notification map.

    let ourNotifications = this._getNotificationsForBrowser(ourBrowser);
    let other = otherBrowser.ownerGlobal.PopupNotifications;
    if (!other) {
      if (ourNotifications.length > 0)
        Cu.reportError("unable to swap notifications: otherBrowser doesn't support notifications");
      return;
    }
    let otherNotifications = other._getNotificationsForBrowser(otherBrowser);
    if (ourNotifications.length < 1 && otherNotifications.length < 1) {
      // No notification to swap.
      return;
    }

    otherNotifications = otherNotifications.filter(n => {
      if (this._fireCallback(n, NOTIFICATION_EVENT_SWAPPING, ourBrowser)) {
        n.browser = ourBrowser;
        n.owner = this;
        return true;
      }
      other._fireCallback(n, NOTIFICATION_EVENT_REMOVED);
      return false;
    });

    ourNotifications = ourNotifications.filter(n => {
      if (this._fireCallback(n, NOTIFICATION_EVENT_SWAPPING, otherBrowser)) {
        n.browser = otherBrowser;
        n.owner = other;
        return true;
      }
      this._fireCallback(n, NOTIFICATION_EVENT_REMOVED);
      return false;
    });

    this._setNotificationsForBrowser(otherBrowser, ourNotifications);
    other._setNotificationsForBrowser(ourBrowser, otherNotifications);

    if (otherNotifications.length > 0)
      this._update(otherNotifications);
    if (ourNotifications.length > 0)
      other._update(ourNotifications);
  },

  _fireCallback: function PopupNotifications_fireCallback(n, event, ...args) {
    try {
      if (n.options.eventCallback)
        return n.options.eventCallback.call(n, event, ...args);
    } catch (error) {
      Cu.reportError(error);
    }
    return undefined;
  },

  _onPopupHidden: function PopupNotifications_onPopupHidden(event) {
    if (event.target != this.panel) {
      return;
    }

    // We may have removed the "noautofocus" attribute before showing the panel
    // if the notification specified it wants to autofocus on first show.
    // When the panel is closed, we have to restore the attribute to its default
    // value, so we don't autofocus it if it's subsequently opened from a different code path.
    this.panel.setAttribute("noautofocus", "true");

    // Handle the case where the panel was closed programmatically.
    if (this._ignoreDismissal) {
      this._ignoreDismissal.resolve();
      this._ignoreDismissal = null;
      return;
    }

    this._dismissOrRemoveCurrentNotifications();

    this._clearPanel();

    this._update();
  },

  _dismissOrRemoveCurrentNotifications() {
    let browser = this.panel.firstChild &&
                  this.panel.firstChild.notification.browser;
    if (!browser)
      return;

    let notifications = this._getNotificationsForBrowser(browser);
    // Mark notifications as dismissed and call dismissal callbacks
    Array.forEach(this.panel.childNodes, function(nEl) {
      let notificationObj = nEl.notification;
      // Never call a dismissal handler on a notification that's been removed.
      if (notifications.indexOf(notificationObj) == -1)
        return;

      // Record the time of the first notification dismissal if the main action
      // was not triggered in the meantime.
      let timeSinceShown = this.window.performance.now() - notificationObj.timeShown;
      if (!notificationObj.wasDismissed &&
          !notificationObj.recordedTelemetryMainAction) {
        notificationObj._recordTelemetry("POPUP_NOTIFICATION_DISMISSAL_MS",
                                         timeSinceShown);
      }
      notificationObj._recordTelemetryStat(this.nextDismissReason);

      // Do not mark the notification as dismissed or fire NOTIFICATION_EVENT_DISMISSED
      // if the notification is removed.
      if (notificationObj.options.removeOnDismissal) {
        this._remove(notificationObj);
      } else {
        notificationObj.dismissed = true;
        this._fireCallback(notificationObj, NOTIFICATION_EVENT_DISMISSED);
      }
    }, this);
  },

  _onButtonEvent(event, type, notificationEl = null) {
    if (!notificationEl) {
      notificationEl = getNotificationFromElement(event.originalTarget);
    }

    if (!notificationEl)
      throw "PopupNotifications._onButtonEvent: couldn't find notification element";

    if (!notificationEl.notification)
      throw "PopupNotifications._onButtonEvent: couldn't find notification";

    let notification = notificationEl.notification;

    if (type == "dropmarkerpopupshown") {
      notification._recordTelemetryStat(TELEMETRY_STAT_OPEN_SUBMENU);
      return;
    }

    if (type == "learnmoreclick") {
      notification._recordTelemetryStat(TELEMETRY_STAT_LEARN_MORE);
      return;
    }

    if (type == "buttoncommand") {
      // Record the total timing of the main action since the notification was
      // created, even if the notification was dismissed in the meantime.
      let timeSinceCreated = this.window.performance.now() - notification.timeCreated;
      if (!notification.recordedTelemetryMainAction) {
        notification.recordedTelemetryMainAction = true;
        notification._recordTelemetry("POPUP_NOTIFICATION_MAIN_ACTION_MS",
                                      timeSinceCreated);
      }
    }

    if (type == "buttoncommand" || type == "secondarybuttoncommand") {
      if (Services.focus.activeWindow != this.window) {
        Services.console.logStringMessage("PopupNotifications._onButtonEvent: " +
                                          "Button click happened before the window was focused");
        this.window.focus();
        return;
      }

      let timeSinceShown = this.window.performance.now() - notification.timeShown;
      if (timeSinceShown < this.buttonDelay) {
        Services.console.logStringMessage("PopupNotifications._onButtonEvent: " +
                                          "Button click happened before the security delay: " +
                                          timeSinceShown + "ms");
        return;
      }
    }

    let action = notification.mainAction;
    let telemetryStatId = TELEMETRY_STAT_ACTION_1;

    if (type == "secondarybuttoncommand") {
      action = notification.secondaryActions[0];
      telemetryStatId = TELEMETRY_STAT_ACTION_2;
    }

    notification._recordTelemetryStat(telemetryStatId);

    if (action) {
      try {
        action.callback.call(undefined, {
          checkboxChecked: notificationEl.checkbox.checked
        });
      } catch (error) {
        Cu.reportError(error);
      }

      if (action.dismiss) {
        this._dismiss();
        return;
      }
    }

    this._remove(notification);
    this._update();
  },

  _onMenuCommand: function PopupNotifications_onMenuCommand(event) {
    let target = event.originalTarget;
    if (!target.action || !target.notification)
      throw "menucommand target has no associated action/notification";

    let notificationEl = target.parentElement;
    event.stopPropagation();

    target.notification._recordTelemetryStat(target.action.telemetryStatId);

    try {
      target.action.callback.call(undefined, {
        checkboxChecked: notificationEl.checkbox.checked
      });
    } catch (error) {
      Cu.reportError(error);
    }

    if (target.action.dismiss) {
      this._dismiss();
      return;
    }

    this._remove(target.notification);
    this._update();
  },

  _notify: function PopupNotifications_notify(topic) {
    Services.obs.notifyObservers(null, "PopupNotifications-" + topic);
  },
};
