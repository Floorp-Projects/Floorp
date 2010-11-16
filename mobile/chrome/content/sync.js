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
  autoConnect: false,

  init: function init() {
    Components.utils.import("resource://services-sync/main.js");

    this._bundle = Services.strings.createBundle("chrome://browser/locale/sync.properties");
    this._msg = document.getElementById("prefs-messages");

    this._addListeners();

    // Generating keypairs is expensive on mobile, so disable it
    if (Weave.Status.checkSetup() != Weave.CLIENT_NOT_CONFIGURED) {
      Weave.Service.keyGenEnabled = false;

      this.autoConnect = Services.prefs.getBoolPref("services.sync.autoconnect");
      if (this.autoConnect) {
        // Put the settings UI into a state of "connecting..." if we are going to auto-connect
        this._elements.connect.collapsed = false;
        this._elements.sync.collapsed = false;

        this._elements.connect.firstChild.disabled = true;
        this._elements.sync.firstChild.disabled = true;

        this._elements.connect.setAttribute("title", this._bundle.GetStringFromName("connecting.label"));
        this._elements.autosync.value = true;

        try {
          this._elements.device.value = Services.prefs.getCharPref("services.sync.client.name");
        } catch(e) {}
      }
    }
  },

  show: function show() {
    // Show the connect UI
    document.getElementById("syncsetup-container").hidden = false;
    document.getElementById("syncsetup-jpake").hidden = false;
    document.getElementById("syncsetup-manual").hidden = true;
  },

  close: function close() {
    // Close the connect UI
    document.getElementById("syncsetup-container").hidden = true;
  },

  showDetails: function showDetails() {
    // Show the connect UI detail settings
    let show = this._elements.details.checked;
    this._elements.autosync.collapsed = show;
    this._elements.device.collapsed = show;
    this._elements.disconnect.collapsed = show;
  },

  connect: function connect() {
    // Cause the Sync system to reset internals if we seem to be switching accounts
    if (this._elements.account.value != Weave.Service.account)
      Weave.Service.startOver();

    // Remove any leftover connection string
    this._elements.connect.removeAttribute("desc");

    // Sync will use the account value and munge it into a username, as needed
    Weave.Service.account = this._elements.account.value;
    Weave.Service.login(Weave.Service.username, this._elements.password.value, this.normalizePassphrase(this._elements.synckey.value));
    Weave.Service.persistLogin();
  },

  disconnect: function disconnect() {
    let message = this._bundle.GetStringFromName("notificationDisconnect.label");
    let button = this._bundle.GetStringFromName("notificationDisconnect.button");
    let buttons = [ {
      label: button,
      accessKey: "",
      callback: function() { WeaveGlue.connect(); }
    } ];
    this.showMessage(message, "undo-disconnect", buttons);

    // XXX change to an event that fires when panel is changed or closed
    setTimeout(function(self) {
      let notification = self._msg.getNotificationWithValue("undo-disconnect");
      if (notification)
        notification.close();
    }, 10000, this);

    // TODO: When the notification closes, not from the "undo" button, we should clean up the credentials

    Weave.Service.logout();
  },

  sync: function sync() {
    Weave.Service.sync();
  },

  _addListeners: function _addListeners() {
    let topics = ["weave:service:sync:start", "weave:service:sync:finish",
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
    let setupids = ["account", "password", "synckey", "customserver"];
    setupids.forEach(function(id) {
      elements[id] = document.getElementById("syncsetup-" + id);
    });

    let settingids = ["device", "connect", "connected", "disconnect", "sync", "autosync", "details"];
    settingids.forEach(function(id) {
      elements[id] = document.getElementById("sync-" + id);
    });

    // Replace the getter with the collection of settings
    delete this._elements;
    return this._elements = elements;
  },

  observe: function observe(aSubject, aTopic, aData) {
    let loggedIn = Weave.Service.isLoggedIn;
    document.getElementById("cmd_remoteTabs").setAttribute("disabled", !loggedIn);

    // If we are going to auto-connect anyway, fake the settings UI to make it
    // look like we are connecting
    loggedIn = loggedIn || this.autoConnect;

    // Make sure we're online when connecting/syncing
    Util.forceOnline();

    // Can't do anything before settings are loaded
    if (this._elements == null)
      return;

    // Make some aliases
    let account = this._elements.account;
    let password = this._elements.password;
    let synckey = this._elements.synckey;
    let connect = this._elements.connect;
    let connected = this._elements.connected;
    let autosync = this._elements.autosync;
    let device = this._elements.device;
    let disconnect = this._elements.disconnect;
    let sync = this._elements.sync;

    // Make sure the options are in the right state
    connect.collapsed = loggedIn;
    connected.collapsed = !loggedIn;
    sync.collapsed = !loggedIn;

    if (connected.collapsed) {
      connect.setAttribute("title", this._bundle.GetStringFromName("notconnected.label"));
      this._elements.details.checked = false;
      this._elements.autosync.collapsed = true;
      this._elements.device.collapsed = true;
      this._elements.disconnect.collapsed = true;
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
          sync.setAttribute("title", self._bundle.GetStringFromName("lastSyncInProgress.label"));
      } else {
        connect.firstChild.disabled = false;
        sync.firstChild.disabled = false;
      }
    }, 0, this);

    // Move the disconnect and sync settings out to make connect the last item
    let parent = connect.parentNode;
    if (!loggedIn)
      parent = parent.parentNode;
    parent.appendChild(disconnect);
    parent.appendChild(sync);

    // Dynamically generate some strings
    let accountStr = this._bundle.formatStringFromName("account.label", [Weave.Service.account], 1);
    disconnect.setAttribute("title", accountStr);

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSync = Weave.Svc.Prefs.get("lastSync");
    if (lastSync != null) {
      let syncDate = new Date(lastSync).toLocaleFormat("%a %R");
      let dateStr = this._bundle.formatStringFromName("lastSync.label", [syncDate], 1);
      sync.setAttribute("title", dateStr);
    }

    // Show what went wrong with login if necessary
    if (aTopic == "weave:service:login:error")
      connect.setAttribute("desc", Weave.Utils.getErrorString(Weave.Status.login));
    else
      connect.removeAttribute("desc");

    // Reset the auto-connect flag after the first attempted login
    if (aTopic == "weave:service:login:finish" || aTopic == "weave:service:login:error")
      this.autoConnect = false;

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

    // Load the values for the string inputs
    account.value = Weave.Service.account || "";
    password.value = Weave.Service.password || "";
    let pp = Weave.Service.passphrase || "";
    if (pp.length == 20)
      pp = this.hyphenatePassphrase(pp);
    synckey.value = pp;
    device.value = Weave.Clients.localName || "";
  },

  changeName: function changeName(aInput) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Clients.localName = aInput.value;
    aInput.value = Weave.Clients.localName;
  },

  changeSync: function changeSync() {
    // XXX enable/disable sync without actually disconnecting
  },

  showMessage: function showMessage(aMsg, aValue, aButtons) {
    let notification = this._msg.getNotificationWithValue(aValue);
    if (notification)
      return;

    this._msg.appendNotification(aMsg, aValue, "", this._msg.PRIORITY_WARNING_LOW, aButtons);
  },

  hyphenatePassphrase: function(passphrase) {
    // Hyphenate a 20 character passphrase in 4 groups of 5
    return passphrase.slice(0, 5) + '-'
         + passphrase.slice(5, 10) + '-'
         + passphrase.slice(10, 15) + '-'
         + passphrase.slice(15, 20);
  },

  normalizePassphrase: function(pp) {
    // Remove hyphens as inserted by hyphenatePassphrase()
    if (pp.length == 23 && pp[5] == '-' && pp[11] == '-' && pp[17] == '-')
      return pp.slice(0, 5) + pp.slice(6, 11) + pp.slice(12, 17) + pp.slice(18, 23);
    return pp;
  }
};
