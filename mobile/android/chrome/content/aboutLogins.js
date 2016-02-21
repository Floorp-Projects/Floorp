/* Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces, Cc = Components.classes, Cu = Components.utils;

Cu.import("resource://services-common/utils.js"); /*global: CommonUtils */
Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm");

XPCOMUtils.defineLazyGetter(window, "gChromeWin", () =>
  window.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIDocShellTreeItem)
    .rootTreeItem
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindow)
    .QueryInterface(Ci.nsIDOMChromeWindow));

XPCOMUtils.defineLazyModuleGetter(this, "Snackbars", "resource://gre/modules/Snackbars.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Prompt",
                                  "resource://gre/modules/Prompt.jsm");

var debug = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.d.bind(null, "AboutLogins");

var gStringBundle = Services.strings.createBundle("chrome://browser/locale/aboutLogins.properties");

function copyStringShowSnackbar(string, notifyString) {
  try {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(string);
    Snackbars.show(notifyString, Snackbars.LENGTH_LONG);
  } catch (e) {
    debug("Error copying from about:logins");
    Snackbars.show(gStringBundle.GetStringFromName("loginsDetails.copyFailed"), Snackbars.LENGTH_LONG);
  }
}

// Delay filtering while typing in MS
const FILTER_DELAY = 500;

var Logins = {
  _logins: [],
  _filterTimer: null,
  _selectedLogin: null,

  // Load the logins list, displaying interstitial UI (see
  // #logins-list-loading-body) while loading.  There are careful
  // jank-avoiding measures taken in this function; be careful when
  // modifying it!
  //
  // Returns a Promise that resolves to the list of logins, ordered by
  // hostname.
  _promiseLogins: function() {
    let contentBody = document.getElementById("content-body");
    let emptyBody = document.getElementById("empty-body");
    let filterIcon = document.getElementById("filter-button");

    let showSpinner = () => {
      this._toggleListBody(true);
      emptyBody.classList.add("hidden");
    };

    let getAllLogins = () => {
      let logins = [];
      try {
        logins = Services.logins.getAllLogins();
      } catch(e) {
        // It's likely that the Master Password was not entered; give
        // a hint to the next person.
        throw new Error("Possible Master Password permissions error: " + e.toString());
      }

      logins.sort((a, b) => a.hostname.localeCompare(b.hostname));

      return logins;
    };

    let hideSpinner = (logins) => {
      this._toggleListBody(false);

      if (!logins.length) {
        contentBody.classList.add("hidden");
        filterIcon.classList.add("hidden");
        emptyBody.classList.remove("hidden");
      } else {
        contentBody.classList.remove("hidden");
        emptyBody.classList.add("hidden");
      }

      return logins;
    };

    // Return a promise that is resolved after a paint.
    let waitForPaint = () => {
      // We're changing 'display'.  We need to wait for the new value to take
      // effect; otherwise, we'll block and never paint a change.  Since
      // requestAnimationFrame callback is generally triggered *before* any
      // style flush and layout, we wait for two animation frames.  This
      // approach was cribbed from
      // https://dxr.mozilla.org/mozilla-central/rev/5abe3c4deab94270440422c850bbeaf512b1f38d/browser/base/content/browser-fullScreen.js?offset=0#469.
      return new Promise(function(resolve, reject) {
        requestAnimationFrame(() => {
          requestAnimationFrame(() => {
            resolve();
          });
        });
      });
    };

    // getAllLogins janks the main-thread.  We need to paint before that jank;
    // by throwing the janky load onto the next tick, we paint the spinner; the
    // spinner is CSS animated off-main-thread.
    return Promise.resolve()
      .then(showSpinner)
      .then(waitForPaint)
      .then(getAllLogins)
      .then(hideSpinner);
  },

  // Reload the logins list, displaying interstitial UI while loading.
  // Update the stored and displayed list upon completion.
  _reloadList: function() {
    this._promiseLogins()
      .then((logins) => {
        this._logins = logins;
        this._loadList(logins);
      })
      .catch((e) => {
        // There's no way to recover from errors, sadly.  Log and make
        // it obvious that something is up.
        this._logins = [];
        debug("Failed to _reloadList!");
        Cu.reportError(e);
      });
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
    document.getElementById("update-btn").addEventListener("click", this._onSaveEditLogin.bind(this), false);
    document.getElementById("password-btn").addEventListener("click", this._onPasswordBtn.bind(this), false);

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

    this._reloadList();
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

    passwordField.addEventListener("input", (event) => {
      let newPassword = passwordField.value;
      let updateBtn = document.getElementById("update-btn");

      if (newPassword === "") {
        updateBtn.disabled = true;
        updateBtn.classList.add("disabled-btn");
      } else if ((newPassword !== "") && (updateBtn.disabled === true)) {
        updateBtn.disabled = false;
        updateBtn.classList.remove("disabled-btn");
      }
    }, false);
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
        Snackbars.show(gStringBundle.GetStringFromName("editLogin.saved1"), Snackbars.LENGTH_LONG);
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
      Snackbars.show(gStringBundle.GetStringFromName("editLogin.couldNotSave"), Snackbars.LENGTH_LONG);
      return;
    }
    Snackbars.show(gStringBundle.GetStringFromName("editLogin.saved1"), Snackbars.LENGTH_LONG);
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
          copyStringShowSnackbar(password, gStringBundle.GetStringFromName("loginsDetails.passwordCopied"));
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
          copyStringShowSnackbar(login.password, gStringBundle.GetStringFromName("loginsDetails.passwordCopied"));
          break;
        case 2:
          copyStringShowSnackbar(login.username, gStringBundle.GetStringFromName("loginsDetails.usernameCopied"));
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
        this._reloadList();
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
