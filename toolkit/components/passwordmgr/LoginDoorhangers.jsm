/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "LoginDoorhangers",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoginManagerParent.jsm");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// Helper function needed because the "disabled" property may not be available
// if the XBL binding of the UI control has not been constructed yet.
function setDisabled(element, disabled) {
  if (disabled) {
    element.setAttribute("disabled", "true");
  } else {
    element.removeAttribute("disabled");
  }
}

this.LoginDoorhangers = {};

/**
 * Doorhanger for selecting and filling logins.
 *
 * @param {Object} properties
 *        Properties from this object will be applied to the new instance.
 */
this.LoginDoorhangers.FillDoorhanger = function (properties) {
  // Set up infrastructure to access our elements and listen to events.
  this.el = new Proxy({}, {
    get: (target, name) => {
      return this.chromeDocument.getElementById("login-fill-" + name);
    },
  });
  this.eventHandlers = [];
  for (let elementName of Object.keys(this.events)) {
    let handlers = this.events[elementName];
    for (let eventName of Object.keys(handlers)) {
      let handler = handlers[eventName];
      this.eventHandlers.push([elementName, eventName, handler.bind(this)]);
    }
  }
  for (let name of Object.getOwnPropertyNames(properties)) {
    this[name] = properties[name];
  }
};

this.LoginDoorhangers.FillDoorhanger.prototype = {
  /**
   * Whether the elements for this doorhanger are currently in the document.
   */
  bound: false,

  /**
   * Associates the doorhanger with its browser. When the tab associated to this
   * browser is selected, the anchor icon for the doorhanger will appear.
   *
   * The browser may change during the lifetime of the doorhanger, in case the
   * web page is moved to a different chrome window by the swapDocShells method.
   */
  set browser(browser) {
    const MAX_DATE_VALUE = new Date(8640000000000000);

    this._browser = browser;

    let doorhanger = this;
    let PopupNotifications = this.chromeDocument.defaultView.PopupNotifications;
    let notification = PopupNotifications.show(
      browser,
      "login-fill",
      "",
      "login-fill-notification-icon",
      null,
      null,
      {
        dismissed: true,
        // This will make the anchor persist forever even if the popup is not
        // visible. We'll remove the notification manually when the page
        // changes, after we had time to check its final state asynchronously.
        timeout: MAX_DATE_VALUE,
        eventCallback: function (topic, otherBrowser) {
          switch (topic) {
            case "shown":
              // Since we specified the "dismissed" option, this event will only
              // be called after the "show" method returns, so the reference to
              // "this.notification" will be available in "bind" at this point.
              doorhanger.promiseHidden =
                         new Promise(resolve => doorhanger.onUnbind = resolve);
              doorhanger.bind();
              break;

            case "dismissed":
            case "removed":
              if (doorhanger.bound) {
                doorhanger.unbind();
                doorhanger.onUnbind();
              }
              break;

            case "swapping":
              doorhanger._browser = otherBrowser;
              return true;
          }
          return false;
        },
      }
    );

    this.notification = notification;
    notification.doorhanger = this;
  },
  get browser() {
    return this._browser;
  },
  _browser: null,

  /**
   * DOM document to which the doorhanger is currently associated.
   *
   * This may change during the lifetime of the doorhanger, in case the web page
   * is moved to a different chrome window by the swapDocShells method.
   */
  get chromeDocument() {
    return this.browser.ownerDocument;
  },

  /**
   * Hides this notification, if the notification panel is currently open.
   */
  hide() {
    let PopupNotifications = this.chromeDocument.defaultView.PopupNotifications;
    if (PopupNotifications.isPanelOpen) {
      PopupNotifications.panel.hidePopup();
    }
  },

  /**
   * Promise resolved as soon as the notification is hidden.
   */
  promiseHidden: Promise.resolve(),

  /**
   * Removes the doorhanger from the browser.
   */
  remove() {
    this.notification.remove();
  },

  /**
   * Binds this doorhanger to its UI controls.
   */
  bind() {
    // Since this may ask for the master password, we must do it at bind time.
    if (this.autoDetailLogin) {
      let formLogins = Services.logins.findLogins({}, this.loginFormOrigin, "",
                                                  null);
      if (formLogins.length == 1) {
        this.detailLogin = formLogins[0];
      }
      this.autoDetailLogin = false;
    }

    this.el.filter.setAttribute("value", this.filterString);
    this.refreshList();
    this.refreshDetailView();

    this.eventHandlers.forEach(([elementName, eventName, handler]) => {
      this.el[elementName].addEventListener(eventName, handler, true);
    });

    // Move the main element to the notification panel for displaying.
    this.notification.owner.panel.firstElementChild.appendChild(this.el.doorhanger);
    this.el.doorhanger.hidden = false;

    this.bound = true;
  },

  /**
   * Unbinds this doorhanger from its UI controls.
   */
  unbind() {
    this.bound = false;

    this.eventHandlers.forEach(([elementName, eventName, handler]) => {
      this.el[elementName].removeEventListener(eventName, handler, true);
    });

    this.clearList();

    // Place the element back in the document for the next time we need it.
    this.el.doorhanger.hidden = true;
    this.chromeDocument.getElementById("mainPopupSet")
                       .appendChild(this.el.doorhanger);
  },

  /**
   * Origin for which the manual fill UI should be displayed, for example
   * "http://www.example.com".
   */
  loginFormOrigin: "",

  /**
   * When no login form is present on the page, we may still display a list of
   * logins, but we cannot offer manual filling.
   */
  loginFormPresent: false,

  /**
   * User-editable string used to filter the list of all logins.
   */
  filterString: "",

  /**
   * Show login details automatically when the panel is first opened.
   */
  autoDetailLogin: false,

  /**
   * Indicates which particular login to show in the detail view.
   */
  set detailLogin(detailLogin) {
    this._detailLogin = detailLogin;
    if (this.bound) {
      this.refreshDetailView();
    }
  },
  get detailLogin() {
    return this._detailLogin;
  },
  _detailLogin: null,

  /**
   * Prototype functions for event handling.
   */
  events: {
    mainview: {
      focus(event) {
        // If keyboard focus returns to any control in the the main view (for
        // example using SHIFT+TAB) close the details view.
        this.detailLogin = null;
      },
    },
    filter: {
      input(event) {
        this.filterString = this.el.filter.value;
        this.refreshList();
      },
    },
    list: {
      click(event) {
        if (event.button == 0 && this.el.list.selectedItem) {
          this.displaySelectedLoginDetails();
        }
      },
      keypress(event) {
        if (event.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN &&
            this.el.list.selectedItem) {
          this.displaySelectedLoginDetails();
        }
      },
    },
    clickcapturer: {
      click(event) {
        this.detailLogin = null;
      },
    },
    details: {
      transitionend(event) {
        // We must set focus to the detail controls only when the transition has
        // ended, otherwise focus will interfere with the animation. We do this
        // only when we're showing the detail view, not when leaving.
        if (event.target == this.el.details && this.detailLogin) {
          if (this.loginFormPresent) {
            this.el.use.focus();
          } else {
            this.el.username.focus();
          }
        }
      },
    },
    use: {
      command(event) {
        this.fillLogin();
      },
    },
  },

  /**
   * Rebuilds the list of logins.
   */
  refreshList() {
    this.clearList();

    let formLogins = Services.logins.findLogins({}, "", "", null);
    let filterToUse = this.filterString.trim().toLowerCase();
    if (filterToUse) {
      formLogins = formLogins.filter(login => {
        return login.hostname.toLowerCase().indexOf(filterToUse) != -1 ||
               login.username.toLowerCase().indexOf(filterToUse) != -1 ;
      });
    }

    for (let { hostname, username } of formLogins) {
      let item = this.chromeDocument.createElementNS(XUL_NS, "richlistitem");
      item.classList.add("login-fill-item");
      item.setAttribute("hostname", hostname);
      item.setAttribute("username", username);
      if (hostname != this.loginFormOrigin) {
        item.classList.add("different-hostname");
      }
      this.el.list.appendChild(item);
    }
  },

  /**
   * Clears the list of logins.
   */
  clearList() {
    let list = this.el.list;
    while (list.firstChild) {
      list.firstChild.remove();
    }
  },

  /**
   * Updates all the controls of the detail view based on the chosen login.
   */
  refreshDetailView() {
    if (this.detailLogin) {
      this.el.username.setAttribute("value", this.detailLogin.username);
      this.el.password.setAttribute("value", this.detailLogin.password);
      this.el.doorhanger.setAttribute("inDetailView", "true");
      setDisabled(this.el.username, false);
      setDisabled(this.el.use, !this.loginFormPresent);
    } else {
      this.el.doorhanger.removeAttribute("inDetailView");
      // We must disable all the detail controls to ensure they cannot be
      // selected with the keyboard while they are outside the visible area.
      setDisabled(this.el.username, true);
      setDisabled(this.el.use, true);
    }
  },

  displaySelectedLoginDetails() {
    let selectedItem = this.el.list.selectedItem;
    let hostLogins = Services.logins.findLogins({},
                               selectedItem.getAttribute("hostname"), "", null);
    let login = hostLogins.find(login => {
      return login.username == selectedItem.getAttribute("username");
    });
    if (!login) {
      Cu.reportError("The selected login has been removed in the meantime.");
      return;
    }
    this.detailLogin = login;
  },

  fillLogin() {
    LoginManagerParent.fillForm({
      browser: this.browser,
      loginFormOrigin: this.loginFormOrigin,
      login: this.detailLogin,
    }).catch(Cu.reportError);
    this.hide();
  },
};

/**
 * Retrieves an existing FillDoorhanger associated with a browser, or null if an
 * associated doorhanger of that type cannot be found.
 *
 * @param An object with the following properties:
 *        {
 *          browser:
 *            The <browser> element to which the doorhanger is associated.
 *        }
 */
this.LoginDoorhangers.FillDoorhanger.find = function ({ browser }) {
  let PopupNotifications = browser.ownerDocument.defaultView.PopupNotifications;
  let notification = PopupNotifications.getNotification("login-fill",
                                                        browser);
  return notification && notification.doorhanger;
};
