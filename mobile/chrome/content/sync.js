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
 * The Original Code is Bookmarks sync code.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mfinkle@mozila.com>
 *  Matt Brubeck <mbrubeck@mozila.com>
 *  Jono DiCarlo <jdicarlo@mozilla.com>
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

let WeaveGlue = {
  setupData: null,
  jpake: null,
  _bundle: null,

  init: function init() {
    if (this._bundle)
      return;

    this._bundle = Services.strings.createBundle("chrome://browser/locale/sync.properties");
    this._msg = document.getElementById("prefs-messages");

    this._addListeners();

    this.setupData = { account: "", password: "" , synckey: "", serverURL: "" };

    // Generating keypairs is expensive on mobile, so disable it
    if (Services.prefs.prefHasUserValue("services.sync.username")) {
      // Put the settings UI into a state of "connecting..." if we are going to auto-connect
      this._elements.connect.firstChild.disabled = true;
      this._elements.connect.setAttribute("title", this._bundle.GetStringFromName("connecting.label"));

      try {
        this._elements.device.value = Services.prefs.getCharPref("services.sync.client.name");
      } catch(e) {}
    } else if (Weave.Status.login != Weave.LOGIN_FAILED_NO_USERNAME) {
      this.loadSetupData();
    }
  },

  abortEasySetup: function abortEasySetup() {
    document.getElementById("syncsetup-code1").value = "....";
    document.getElementById("syncsetup-code2").value = "....";
    document.getElementById("syncsetup-code3").value = "....";
    if (!this.jpake)
      return;

    this.jpake.abort();
    this.jpake = null;
  },

  _resetScrollPosition: function _resetScrollPosition() {
    let scrollboxes = document.getElementsByClassName("syncsetup-scrollbox");
    for (let i = 0; i < scrollboxes.length; i++) {
      let sbo = scrollboxes[i].boxObject.QueryInterface(Ci.nsIScrollBoxObject);
      try {
        sbo.scrollTo(0, 0);
      } catch(e) {}
    }
  },

  open: function open() {
    let container = document.getElementById("syncsetup-container");
    if (!container.hidden)
      return;

    // Services.io.offline is lying to us, so we use the NetworkLinkService instead
    let nls = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
    if (!nls.isLinkUp) {
      Services.obs.notifyObservers(null, "browser:sync:setup:networkerror", "");
      Services.prompt.alert(window,
                             this._bundle.GetStringFromName("sync.setup.error.title"),
                             this._bundle.GetStringFromName("sync.setup.error.network"));
      return;
    }

    // Clear up any previous JPAKE codes
    this.abortEasySetup();

    // Show the connect UI
    container.hidden = false;
    document.getElementById("syncsetup-simple").hidden = false;
    document.getElementById("syncsetup-waiting").hidden = true;
    document.getElementById("syncsetup-fallback").hidden = true;

    BrowserUI.pushDialog(this);

    let self = this;
    this.jpake = new Weave.JPAKEClient({
      displayPIN: function displayPIN(aPin) {
        document.getElementById("syncsetup-code1").value = aPin.slice(0, 4);
        document.getElementById("syncsetup-code2").value = aPin.slice(4, 8);
        document.getElementById("syncsetup-code3").value = aPin.slice(8);
      },

      onPairingStart: function onPairingStart() {
        document.getElementById("syncsetup-simple").hidden = true;
        document.getElementById("syncsetup-waiting").hidden = false;
      },

      onComplete: function onComplete(aCredentials) {
        self.jpake = null;
        self.close();
        self.setupData = aCredentials;
        self.connect();
      },

      onAbort: function onAbort(aError) {
        self.jpake = null;

        if (aError == "jpake.error.userabort" || container.hidden) {
          Services.obs.notifyObservers(null, "browser:sync:setup:userabort", "");
          return;
        }

        // Automatically go to manual setup if we couldn't acquire a channel.
        let brandShortName = Strings.brand.GetStringFromName("brandShortName");
        let tryAgain = self._bundle.GetStringFromName("sync.setup.tryagain");
        let manualSetup = self._bundle.GetStringFromName("sync.setup.manual");
        let buttonFlags = Ci.nsIPrompt.BUTTON_POS_1_DEFAULT +
                         (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_0) +
                         (Ci.nsIPrompt.BUTTON_TITLE_IS_STRING * Ci.nsIPrompt.BUTTON_POS_1) +
                         (Ci.nsIPrompt.BUTTON_TITLE_CANCEL    * Ci.nsIPrompt.BUTTON_POS_2);

        let button = Services.prompt.confirmEx(window,
                               self._bundle.GetStringFromName("sync.setup.error.title"),
                               self._bundle.formatStringFromName("sync.setup.error.nodata", [brandShortName], 1),
                               buttonFlags, tryAgain, manualSetup, null, "", {});
        switch (button) {
          case 0:
            // we have to build a new JPAKEClient here rather than reuse the old one
            container.hidden = true;
            self.open();
            break;
          case 1:
            self.openManual();
            break;
          case 2:
          default:
            self.close();
            break;
        }
      }
    });
    this.jpake.receiveNoPIN();
  },

  openManual: function openManual() {
    this.abortEasySetup();

    // Reset the scroll since the previous page might have been scrolled
    this._resetScrollPosition();

    document.getElementById("syncsetup-simple").hidden = true;
    document.getElementById("syncsetup-fallback").hidden = false;

    // Push the current setup data into the UI
    if (this.setupData && "account" in this.setupData) {
      this._elements.account.value = this.setupData.account;
      this._elements.password.value = this.setupData.password;
      let pp = this.setupData.synckey;
      if (Weave.Utils.isPassphrase(pp))
        pp = Weave.Utils.hyphenatePassphrase(pp);
      this._elements.synckey.value = pp;
      if (this.setupData.serverURL && this.setupData.serverURL.length) {
        this._elements.usecustomserver.checked = true;
        this._elements.customserver.disabled = false;
        this._elements.customserver.value = this.setupData.serverURL;
      } else {
        this._elements.usecustomserver.checked = false;
        this._elements.customserver.disabled = true;
        this._elements.customserver.value = "";
      }
    }

    this.canConnect();
  },

  close: function close() {
    if (this.jpake)
      this.abortEasySetup();

    // Reset the scroll since the previous page might have been scrolled
    this._resetScrollPosition();

    // Save current setup data
    this.setupData = {
      account: this._elements.account.value.trim(),
      password: this._elements.password.value.trim(),
      synckey: Weave.Utils.normalizePassphrase(this._elements.synckey.value.trim()),
      serverURL: this._validateServer(this._elements.customserver.value.trim())
    };

    // Clear the UI so it's ready for next time
    this._elements.account.value = "";
    this._elements.password.value = "";
    this._elements.synckey.value = "";
    this._elements.usecustomserver.checked = false;
    this._elements.customserver.disabled = true;
    this._elements.customserver.value = "";

    // Close the connect UI
    document.getElementById("syncsetup-container").hidden = true;
    BrowserUI.popDialog();
  },

  toggleCustomServer: function toggleCustomServer() {
    let useCustomServer = this._elements.usecustomserver.checked;
    this._elements.customserver.disabled = !useCustomServer;
    if (!useCustomServer)
      this._elements.customserver.value = "";
  },

  canConnect: function canConnect() {
    let account = this._elements.account.value;
    let password = this._elements.password.value;
    let synckey = this._elements.synckey.value;

    let disabled = !(account && password && synckey);
    document.getElementById("syncsetup-button-connect").disabled = disabled;
  },

  showDetails: function showDetails() {
    // Show the connect UI detail settings
    let show = this._elements.sync.collapsed;
    this._elements.details.checked = show;
    this._elements.sync.collapsed = !show;
    this._elements.device.collapsed = !show;
    this._elements.disconnect.collapsed = !show;
  },

  tryConnect: function login() {
    // If Sync is not configured, simply show the setup dialog
    if (!Services.prefs.prefHasUserValue("services.sync.username")) {
      this.open();
      return;
    }

    // No setup data, do nothing
    if (!this.setupData)
      return;

    if (this.setupData.serverURL && this.setupData.serverURL.length)
      Weave.Service.serverURL = this.setupData.serverURL;

    // We might still be in the middle of a sync from before Sync was disabled, so
    // let's force the UI into a state that the Sync code feels comfortable
    this.observe(null, "", "");

    // Now try to re-connect. If successful, this will reset the UI into the
    // correct state automatically.
    Weave.Service.login(Weave.Service.username, this.setupData.password, this.setupData.synckey);
  },

  connect: function connect(aSetupData) {
    // Use setup data to pre-configure manual fields
    if (aSetupData)
      this.setupData = aSetupData;

    // Cause the Sync system to reset internals if we seem to be switching accounts
    if (this.setupData.account != Weave.Service.account)
      Weave.Service.startOver();

    // Remove any leftover connection error string
    this._elements.connect.removeAttribute("desc");

    // Reset the custom server URL, if we have one
    if (this.setupData.serverURL && this.setupData.serverURL.length)
      Weave.Service.serverURL = this.setupData.serverURL;

    // Sync will use the account value and munge it into a username, as needed
    Weave.Service.account = this.setupData.account;
    Weave.Service.password = this.setupData.password;
    Weave.Service.passphrase = this.setupData.synckey;
    Weave.Service.persistLogin();
    Weave.Svc.Obs.notify("weave:service:setup-complete");
    setTimeout(function () { Weave.Service.sync(); }, 0);
  },

  disconnect: function disconnect() {
    // Save credentials for undo
    let undoData = this.setupData;

    // Remove all credentials
    this.setupData = null;
    Weave.Service.startOver();

    let message = this._bundle.GetStringFromName("notificationDisconnect.label");
    let button = this._bundle.GetStringFromName("notificationDisconnect.button");
    let buttons = [ {
      label: button,
      accessKey: "",
      callback: function() { WeaveGlue.connect(undoData); }
    } ];
    this.showMessage(message, "undo-disconnect", buttons);

    // Hide the notification when the panel is changed or closed.
    let panel = document.getElementById("prefs-container");
    panel.addEventListener("ToolPanelHidden", function onHide(aEvent) {
      panel.removeEventListener(aEvent.type, onHide, false);
      let notification = WeaveGlue._msg.getNotificationWithValue("undo-disconnect");
      if (notification)
        notification.close();
    }, false);

    Weave.Service.logout();
  },

  sync: function sync() {
    Weave.Service.sync();
  },

  _addListeners: function _addListeners() {
    let topics = ["weave:service:setup-complete",
      "weave:service:sync:start", "weave:service:sync:finish",
      "weave:service:sync:error", "weave:service:login:start",
      "weave:service:login:finish", "weave:service:login:error",
      "weave:service:logout:finish"];

    // For each topic, add WeaveGlue the observer
    topics.forEach(function(topic) {
      Services.obs.addObserver(WeaveGlue, topic, false);
    });

    // Remove them on unload
    addEventListener("unload", function() {
      topics.forEach(function(topic) {
        Services.obs.removeObserver(WeaveGlue, topic, false);
      });
    }, false);
  },

  get _elements() {
    // Do a quick test to see if the options exist yet
    let syncButton = document.getElementById("sync-syncButton");
    if (syncButton == null)
      return null;

    // Get all the setting nodes from the add-ons display
    let elements = {};
    let setupids = ["account", "password", "synckey", "usecustomserver", "customserver"];
    setupids.forEach(function(id) {
      elements[id] = document.getElementById("syncsetup-" + id);
    });

    let settingids = ["device", "connect", "connected", "disconnect", "sync", "details"];
    settingids.forEach(function(id) {
      elements[id] = document.getElementById("sync-" + id);
    });

    // Replace the getter with the collection of settings
    delete this._elements;
    return this._elements = elements;
  },

  observe: function observe(aSubject, aTopic, aData) {
    // Make sure we're online when connecting/syncing
    Util.forceOnline();

    // Can't do anything before settings are loaded
    if (this._elements == null)
      return;

    // Make some aliases
    let connect = this._elements.connect;
    let connected = this._elements.connected;
    let details = this._elements.details;
    let device = this._elements.device;
    let disconnect = this._elements.disconnect;
    let sync = this._elements.sync;

    let isConfigured = Services.prefs.prefHasUserValue("services.sync.username");

    connect.collapsed = isConfigured;
    connected.collapsed = !isConfigured;

    if (!isConfigured) {
      connect.setAttribute("title", this._bundle.GetStringFromName("notconnected.label"));
      connect.firstChild.disabled = false;
      details.checked = false;
      sync.collapsed = true;
      device.collapsed = true;
      disconnect.collapsed = true;
    }

    // Check the lock on a timeout because it's set just after notifying
    setTimeout(function(self) {
      // Prevent certain actions when the service is locked
      if (Weave.Service.locked) {
        connect.firstChild.disabled = true;
        sync.firstChild.disabled = true;

        if (aTopic == "weave:service:login:start")
          connect.setAttribute("title", self._bundle.GetStringFromName("connecting.label"));

        if (aTopic == "weave:service:sync:start")
          sync.setAttribute("title", self._bundle.GetStringFromName("lastSyncInProgress2.label"));
      } else {
        connect.firstChild.disabled = false;
        sync.firstChild.disabled = false;
      }
    }, 0, this);

    // Dynamically generate some strings
    let accountStr = this._bundle.formatStringFromName("account.label", [Weave.Service.account], 1);
    disconnect.setAttribute("title", accountStr);

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSync = Weave.Svc.Prefs.get("lastSync");
    if (lastSync != null) {
      let syncDate = new Date(lastSync).toLocaleFormat("%a %R");
      let dateStr = this._bundle.formatStringFromName("lastSync2.label", [syncDate], 1);
      sync.setAttribute("title", dateStr);
    }

    // Show what went wrong with login if necessary
    if (aTopic == "weave:service:login:error") {
      if (Weave.Status.login == "service.master_password_locked")
        Weave.Service.logout();
      else
        connect.setAttribute("desc", Weave.Utils.getErrorString(Weave.Status.login));
    } else {
      connect.removeAttribute("desc");
    }

    // Init the setup data if we just logged in
    if (!this.setupData && aTopic == "weave:service:login:finish")
      this.loadSetupData();

    // Check for a storage format update, update the user and load the Sync update page
    if (aTopic =="weave:service:sync:error") {
      let clientOutdated = false, remoteOutdated = false;
      if (Weave.Status.sync == Weave.VERSION_OUT_OF_DATE) {
        clientOutdated = true;
      } else if (Weave.Status.sync == Weave.DESKTOP_VERSION_OUT_OF_DATE) {
        remoteOutdated = true;
      } else if (Weave.Status.service == Weave.SYNC_FAILED_PARTIAL) {
        // Some engines failed, check for per-engine compat
        for (let [engine, reason] in Iterator(Weave.Status.engines)) {
           clientOutdated = clientOutdated || reason == Weave.VERSION_OUT_OF_DATE;
           remoteOutdated = remoteOutdated || reason == Weave.DESKTOP_VERSION_OUT_OF_DATE;
        }
      }

      if (clientOutdated || remoteOutdated) {
        let brand = Services.strings.createBundle("chrome://branding/locale/brand.properties");
        let brandName = brand.GetStringFromName("brandShortName");

        let type = clientOutdated ? "client" : "remote";
        let message = this._bundle.GetStringFromName("sync.update." + type);
        message = message.replace("#1", brandName);
        message = message.replace("#2", Services.appinfo.version);
        let title = this._bundle.GetStringFromName("sync.update.title")
        let button = this._bundle.GetStringFromName("sync.update.button")
        let close = this._bundle.GetStringFromName("sync.update.close")

        let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                    Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_IS_STRING;
        let choice = Services.prompt.confirmEx(window, title, message, flags, button, close, null, null, {});
        if (choice == 0)
          Browser.addTab("https://services.mozilla.com/update/", true, Browser.selectedTab);
      }
    }

    device.value = Weave.Clients.localName || "";
  },

  changeName: function changeName(aInput) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Clients.localName = aInput.value;
    aInput.value = Weave.Clients.localName;
  },

  showMessage: function showMessage(aMsg, aValue, aButtons) {
    let notification = this._msg.getNotificationWithValue(aValue);
    if (notification)
      return;

    this._msg.appendNotification(aMsg, aValue, "", this._msg.PRIORITY_WARNING_LOW, aButtons);
  },

  _validateServer: function _validateServer(aURL) {
    let uri = Weave.Utils.makeURI(aURL);

    if (!uri && aURL)
      uri = Weave.Utils.makeURI("https://" + aURL);

    if (!uri)
      return "";
    return uri.spec;
  },

  openTutorial: function _openTutorial() {
    WeaveGlue.close();

    let formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].getService(Ci.nsIURLFormatter);
    let url = formatter.formatURLPref("app.sync.tutorialURL");
    BrowserUI.newTab(url, Browser.selectedTab);
  },

  loadSetupData: function _loadSetupData() {
    this.setupData = {};
    this.setupData.account = Weave.Service.account || "";
    this.setupData.password = Weave.Service.password || "";
    this.setupData.synckey = Weave.Service.passphrase || "";

    let serverURL = Weave.Service.serverURL;
    let defaultPrefs = Services.prefs.getDefaultBranch(null);
    if (serverURL == defaultPrefs.getCharPref("services.sync.serverURL"))
      serverURL = "";
    this.setupData.serverURL = serverURL;
  }
};
