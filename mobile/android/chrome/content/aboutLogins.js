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

let debug = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "AboutLogins");

let gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutLogins.properties");

function copyStringAndToast(string, notifyString) {
  try {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(string);
    gChromeWin.NativeWindow.toast.show(notifyString, "short");
  } catch (e) {
    debug("Error copying from about:logins");
    gChromeWin.NativeWindow.toast.show(gStringBundle.GetStringFromName("loginsDetails.copyFailed"), "short");
  }
}

// Delay filtering while typing in MS
const FILTER_DELAY = 500;

let Logins = {
  _logins: [],
  _filterTimer: null,
  _selectedLogin: null,

  _getLogins: function() {
    let logins;
    let contentBody = document.getElementById("content-body");
    let emptyBody = document.getElementById("empty-body");
    let filterIcon = document.getElementById("filter-button");

    this._toggleListBody(true);
    emptyBody.classList.add("hidden");

    try {
      logins = Services.logins.getAllLogins();
    } catch(e) {
      // Master password was not entered
      debug("Master password permissions error: " + e);
      logins = [];
    }
    this._toggleListBody(false);

    if (!logins.length) {
      emptyBody.classList.remove("hidden");

      filterIcon.classList.add("hidden");
      contentBody.classList.add("hidden");
    } else {
      emptyBody.classList.add("hidden");

      filterIcon.classList.remove("hidden");
    }

    logins.sort((a, b) => a.hostname.localeCompare(b.hostname));
    return this._logins = logins;
  },

  _toggleListBody: function(isLoading) {
    let contentBody = document.getElementById("content-body");
    let loadingBody = document.getElementById("logins-list-loading-body");

    if (isLoading) {
      contentBody.classList.add("hidden");
      loadingBody.classList.remove("hidden");
    } else {
      loadingBody.classList.add("hidden");
      contentBody.classList.remove("hidden");
    }

  },

  init: function () {
    window.addEventListener("popstate", this , false);

    Services.obs.addObserver(this, "passwordmgr-storage-changed", false);
    document.getElementById("save-btn").addEventListener("click", this._onSaveEditLogin.bind(this), false);
    document.getElementById("password-btn").addEventListener("click", this._onPasswordBtn.bind(this), false);

    this._loadList(this._getLogins());

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

    this._updatePasswordBtn(true);
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
    let loginsListPage = document.getElementById("logins-list-page");
    loginsListPage.classList.remove("hidden");

    let editLoginPage = document.getElementById("edit-login-page");
    editLoginPage.classList.add("hidden");

    // If the Show/Hide password button has been flipped, reset it
    if (this._isPasswordBtnInHideMode()) {
      this._updatePasswordBtn(true);
    }
  },

  _onPopState: function (event) {
    // Called when back/forward is used to change the state of the page
    if (event.state) {
      this._showEditLoginDialog(event.state.id);
    } else {
      this._selectedLogin = null;
      this._showList();
    }
  },
  _showEditLoginDialog: function (login) {
    let listPage = document.getElementById("logins-list-page");
    listPage.classList.add("hidden");

    let editLoginPage = document.getElementById("edit-login-page");
    editLoginPage.classList.remove("hidden");

    let usernameField = document.getElementById("username");
    usernameField.value = login.username;
    let passwordField = document.getElementById("password");
    passwordField.value = login.password;
    let domainField = document.getElementById("hostname");
    domainField.value = login.hostname;

    let img = document.getElementById("favicon");
    this._loadFavicon(img, login.hostname);

    let headerText = document.getElementById("edit-login-header-text");
    if (login.hostname && (login.hostname != "")) {
      headerText.textContent = login.hostname;
    }
    else {
      headerText.textContent = gStringBundle.GetStringFromName("editLogin.fallbackTitle");
    }
  },

  _onSaveEditLogin: function() {
    let newUsername = document.getElementById("username").value;
    let newPassword = document.getElementById("password").value;
    let newDomain  = document.getElementById("hostname").value;
    let origUsername = this._selectedLogin.username;
    let origPassword = this._selectedLogin.password;
    let origDomain = this._selectedLogin.hostname;

    try {
      if ((newUsername === origUsername) &&
          (newPassword === origPassword) &&
          (newDomain === origDomain) ) {
        gChromeWin.NativeWindow.toast.show(gStringBundle.GetStringFromName("editLogin.saved"), "short");
        this._showList();
        return;
      }

      let logins = Services.logins.findLogins({}, origDomain, origDomain, null);

      for (let i = 0; i < logins.length; i++) {
        if (logins[i].username == origUsername) {
          let clone = logins[i].clone();
          clone.username = newUsername;
          clone.password = newPassword;
          clone.hostname = newDomain;
          Services.logins.removeLogin(logins[i]);
          Services.logins.addLogin(clone);
          break;
        }
      }
    } catch (e) {
      gChromeWin.NativeWindow.toast.show(gStringBundle.GetStringFromName("editLogin.couldNotSave"), "short");
      return;
    }
    gChromeWin.NativeWindow.toast.show(gStringBundle.GetStringFromName("editLogin.saved"), "short");
    this._showList();
  },

  _onPasswordBtn: function () {
    this._updatePasswordBtn(this._isPasswordBtnInHideMode());
  },

  _updatePasswordBtn: function (aShouldShow) {
    let passwordField = document.getElementById("password");
    let button = document.getElementById("password-btn");
    let show = gStringBundle.GetStringFromName("password-btn.show");
    let hide = gStringBundle.GetStringFromName("password-btn.hide");
    if (aShouldShow) {
      passwordField.type = "password";
      button.textContent = show;
      button.classList.remove("password-btn-hide");
    } else {
      passwordField.type = "text";
      button.textContent= hide;
      button.classList.add("password-btn-hide");
    }
  },

  _isPasswordBtnInHideMode: function () {
    let button = document.getElementById("password-btn");
    return button.classList.contains("password-btn-hide");
  },

  _showPassword: function(password) {
    let passwordPrompt = new Prompt({
      window: window,
      message: password,
      buttons: [
        gStringBundle.GetStringFromName("loginsDialog.copy"),
        gStringBundle.GetStringFromName("loginsDialog.cancel") ]
      }).show((data) => {
        switch (data.button) {
          case 0:
          // Corresponds to "Copy password" button.
          copyStringAndToast(password, gStringBundle.GetStringFromName("loginsDetails.passwordCopied"));
        }
     });
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
      { label: gStringBundle.GetStringFromName("loginsMenu.showPassword") },
      { label: gStringBundle.GetStringFromName("loginsMenu.copyPassword") },
      { label: gStringBundle.GetStringFromName("loginsMenu.copyUsername") },
      { label: gStringBundle.GetStringFromName("loginsMenu.editLogin") },
      { label: gStringBundle.GetStringFromName("loginsMenu.delete") }
    ];

    prompt.setSingleChoiceItems(menuItems);
    prompt.show((data) => {
      // Switch on indices of buttons, as they were added when creating login item.
      switch (data.button) {
        case 0:
          this._showPassword(login.password);
          break;
        case 1:
          copyStringAndToast(login.password, gStringBundle.GetStringFromName("loginsDetails.passwordCopied"));
          break;
        case 2:
          copyStringAndToast(login.username, gStringBundle.GetStringFromName("loginsDetails.usernameCopied"));
          break;
        case 3:
          this._selectedLogin = login;
          this._showEditLoginDialog(login);
          history.pushState({ id: login.guid }, document.title);
          break;
        case 4:
          let confirmPrompt = new Prompt({
            window: window,
            message: gStringBundle.GetStringFromName("loginsDialog.confirmDelete"),
            buttons: [
              gStringBundle.GetStringFromName("loginsDialog.confirm"),
              gStringBundle.GetStringFromName("loginsDialog.cancel") ]
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

  _loadFavicon: function (aImg, aHostname) {
    // Load favicon from cache.
    Messaging.sendRequestForResult({
      type: "Favicon:CacheLoad",
      url: aHostname,
    }).then(function(faviconUrl) {
      aImg.style.backgroundImage= "url('" + faviconUrl + "')";
      aImg.style.visibility = "visible";
    }, function(data) {
      debug("Favicon cache failure : " + data);
      aImg.style.visibility = "visible";
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

    this._loadFavicon(img, login.hostname);
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
        // Reload logins content.
        this._loadList(this._getLogins());
        break;
      }
    }
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

window.addEventListener("load", Logins.init.bind(Logins), false);
window.addEventListener("unload", Logins.uninit.bind(Logins), false);
