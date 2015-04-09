/* Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

let Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm")
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", function()
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

let debug = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "AboutPasswords");

let gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutPasswords.properties");

function copyStringAndToast(string, notifyString) {
  try {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(string);
    gChromeWin.NativeWindow.toast.show(notifyString, "short");
  } catch (e) {
    debug("Error copying from about:passwords");
    gChromeWin.NativeWindow.toast.show(gStringBundle.GetStringFromName("passwordsDetails.copyFailed"), "short");
  }
}

// Delay filtering while typing in MS
const FILTER_DELAY = 500;

let Passwords = {
  _logins: [],
  _filterTimer: null,

  _getLogins: function() {
    let logins;
    try {
      logins = Services.logins.getAllLogins();
    } catch(e) {
      // Master password was not entered
      debug("Master password permissions error: " + e);
      logins = [];
    }

    logins.sort((a, b) => a.hostname.localeCompare(b.hostname));
    return this._logins = logins;
  },

  init: function () {
    window.addEventListener("popstate", this , false);

    Services.obs.addObserver(this, "passwordmgr-storage-changed", false);

    this._loadList(this._getLogins());

    document.getElementById("copyusername-btn").addEventListener("click", this._copyUsername.bind(this), false);
    document.getElementById("copypassword-btn").addEventListener("click", this._copyPassword.bind(this), false);
    document.getElementById("details-header").addEventListener("click", this._openLink.bind(this), false);

    let filterInput = document.getElementById("filter-input");
    let filterContainer = document.getElementById("filter-input-container");

    filterInput.addEventListener("input", (event) => {
      // Stop any in-progress filter timer
      if (this._filterTimer) {
        clearTimeout(this._filterTimer);
        this._filterTimer = null;
      }

      // Start a new timer
      this._filterTimer = setTimeout(() => {
        this._filter(event);
      }, FILTER_DELAY);
    }, false);

    filterInput.addEventListener("blur", (event) => {
      filterContainer.setAttribute("hidden", true);
    });

    document.getElementById("filter-button").addEventListener("click", (event) => {
      filterContainer.removeAttribute("hidden");
      filterInput.focus();
    }, false);

    document.getElementById("filter-clear").addEventListener("click", (event) => {
      // Stop any in-progress filter timer
      if (this._filterTimer) {
        clearTimeout(this._filterTimer);
        this._filterTimer = null;
      }

      filterInput.blur();
      filterInput.value = "";
      this._loadList(this._logins);
    }, false);

    this._showList();
  },

  uninit: function () {
    Services.obs.removeObserver(this, "passwordmgr-storage-changed");
    window.removeEventListener("popstate", this, false);
  },

  _loadList: function (logins) {
    let list = document.getElementById("logins-list");
    let newList = list.cloneNode(false);

    logins.forEach(login => {
      let item = this._createItemForLogin(login);
      newList.appendChild(item);
    });

    list.parentNode.replaceChild(newList, list);
  },

  _showList: function () {
    // Hide the detail page and show the list
    let details = document.getElementById("login-details");
    details.setAttribute("hidden", "true");
    let list = document.getElementById("logins-list");
    list.removeAttribute("hidden");
  },

  _onPopState: function (event) {
    // Called when back/forward is used to change the state of the page
    if (event.state) {
      // Show the detail page for an addon
      this._showDetails(this._getElementForLogin(event.state.id));
    } else {
      // Clear any previous detail addon
      let detailItem = document.querySelector("#login-details > .login-item");
      detailItem.login = null;
      this._showList();
    }
  },

  _onLoginClick: function (event) {
    let loginItem = event.currentTarget;
    let login = loginItem.login;
    if (!login) {
      debug("No login!");
      return;
    }

    let prompt = new Prompt({
      window: window,
    });
    let menuItems = [
      { label: gStringBundle.GetStringFromName("passwordsMenu.copyPassword") },
      { label: gStringBundle.GetStringFromName("passwordsMenu.copyUsername") },
      { label: gStringBundle.GetStringFromName("passwordsMenu.details") },
      { label: gStringBundle.GetStringFromName("passwordsMenu.delete") }
    ];

    prompt.setSingleChoiceItems(menuItems);
    prompt.show((data) => {
      // Switch on indices of buttons, as they were added when creating login item.
      switch (data.button) {
        case 0:
          copyStringAndToast(login.password, gStringBundle.GetStringFromName("passwordsDetails.passwordCopied"));
          break;
        case 1:
          copyStringAndToast(login.username, gStringBundle.GetStringFromName("passwordsDetails.usernameCopied"));
          break;
        case 2:
          this._showDetails(loginItem);
          history.pushState({ id: login.guid }, document.title);
          break;
        case 3:
          let confirmPrompt = new Prompt({
            window: window,
            message: gStringBundle.GetStringFromName("passwordsDialog.confirmDelete"),
            buttons: [
              gStringBundle.GetStringFromName("passwordsDialog.confirm"),
              gStringBundle.GetStringFromName("passwordsDialog.cancel") ]
          });
          confirmPrompt.show((data) => {
            switch (data.button) {
              case 0:
                // Corresponds to "confirm" button.
                Services.logins.removeLogin(login);
            }
          });
      }
    });
  },

  _createItemForLogin: function (login) {
    let loginItem = document.createElement("div");

    loginItem.setAttribute("loginID", login.guid);
    loginItem.className = "login-item list-item";

    loginItem.addEventListener("click", this, true);

    // Create item icon.
    let img = document.createElement("div");
    img.className = "icon";

    // Load favicon from cache.
    Messaging.sendRequestForResult({
      type: "Favicon:CacheLoad",
      url: login.hostname,
    }).then(function(faviconUrl) {
      img.style.backgroundImage= "url('" + faviconUrl + "')";
      img.style.visibility = "visible";
    }, function(data) {
      debug("Favicon cache failure : " + data);
      img.style.visibility = "visible";
    });
    loginItem.appendChild(img);

    // Create item details.
    let inner = document.createElement("div");
    inner.className = "inner";

    let details = document.createElement("div");
    details.className = "details";
    inner.appendChild(details);

    let titlePart = document.createElement("div");
    titlePart.className = "hostname";
    titlePart.textContent = login.hostname;
    details.appendChild(titlePart);

    let versionPart = document.createElement("div");
    versionPart.textContent = login.httpRealm;
    versionPart.className = "realm";
    details.appendChild(versionPart);

    let descPart = document.createElement("div");
    descPart.textContent = login.username;
    descPart.className = "username";
    inner.appendChild(descPart);

    loginItem.appendChild(inner);
    loginItem.login = login;
    return loginItem;
  },

  _getElementForLogin: function (login) {
    let list = document.getElementById("logins-list");
    let element = list.querySelector("div[loginID=" + login.quote() + "]");
    return element;
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "popstate": {
        this._onPopState(event);
        break;
      }
      case "click": {
        this._onLoginClick(event);
        break;
      }
    }
  },

  observe: function (subject, topic, data) {
    switch(topic) {
      case "passwordmgr-storage-changed": {
        // Reload passwords content.
        this._loadList(this._getLogins());
        break;
      }
    }
  },

  _showDetails: function (listItem) {
    let detailItem = document.querySelector("#login-details > .login-item");
    let login = detailItem.login = listItem.login;
    let favicon = detailItem.querySelector(".icon");
    favicon.style["background-image"] = listItem.querySelector(".icon").style["background-image"];
    favicon.style.visibility = "visible";
    document.getElementById("details-header").setAttribute("link", login.hostname);

    document.getElementById("detail-hostname").textContent = login.hostname;
    document.getElementById("detail-realm").textContent = login.httpRealm;
    document.getElementById("detail-username").textContent = login.username;

    // Borrowed from desktop Firefox: http://mxr.mozilla.org/mozilla-central/source/browser/base/content/urlbarBindings.xml#204
    let matchedURL = login.hostname.match(/^((?:[a-z]+:\/\/)?(?:[^\/]+@)?)(.+?)(?::\d+)?(?:\/|$)/);

    let userInputs = [];
    if (matchedURL) {
      let [, , domain] = matchedURL;
      userInputs = domain.split(".").filter(part => part.length > 3);
    }

    let lastChanged = new Date(login.QueryInterface(Ci.nsILoginMetaInfo).timePasswordChanged);
    let days = Math.round((Date.now() - lastChanged) / 1000 / 60 / 60/ 24);
    document.getElementById("detail-age").textContent = gStringBundle.formatStringFromName("passwordsDetails.age", [days], 1);

    let list = document.getElementById("logins-list");
    list.setAttribute("hidden", "true");

    let loginDetails = document.getElementById("login-details");
    loginDetails.removeAttribute("hidden");

    // Password details page is loaded.
    let loadEvent = document.createEvent("Events");
    loadEvent.initEvent("PasswordsDetailsLoad", true, false);
    window.dispatchEvent(loadEvent);
  },

  _copyUsername: function() {
    let detailItem = document.querySelector("#login-details > .login-item");
    let login = detailItem.login;
    copyStringAndToast(login.username, gStringBundle.GetStringFromName("passwordsDetails.usernameCopied"));
  },

  _copyPassword: function() {
    let detailItem = document.querySelector("#login-details > .login-item");
    let login = detailItem.login;
    copyStringAndToast(login.password, gStringBundle.GetStringFromName("passwordsDetails.passwordCopied"));
  },

  _openLink: function (clickEvent) {
    let url = clickEvent.currentTarget.getAttribute("link");
    let BrowserApp = gChromeWin.BrowserApp;
    BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id });
  },

  _filter: function(event) {
    let value = event.target.value.toLowerCase();
    let logins = this._logins.filter((login) => {
      if (login.hostname.toLowerCase().indexOf(value) != -1) {
        return true;
      }
      if (login.username &&
          login.username.toLowerCase().indexOf(value) != -1) {
        return true;
      }
      if (login.httpRealm &&
          login.httpRealm.toLowerCase().indexOf(value) != -1) {
        return true;
      }
      return false;
    });

    this._loadList(logins);
  }
};

window.addEventListener("load", Passwords.init.bind(Passwords), false);
window.addEventListener("unload", Passwords.uninit.bind(Passwords), false);
