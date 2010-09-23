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
  init: function init() {
    Components.utils.import("resource://services-sync/service.js");

    this._addListeners();

    // Generating keypairs is expensive on mobile, so disable it
    Weave.Service.keyGenEnabled = false;

    // Load the values for the string inputs
    this.loadInputs();
  },

  connect: function connect() {
    // Cause the Sync system to reset internals if we seem to be switching accounts
    if (this._settings.account.value != Weave.Service.account)
      Weave.Service.startOver();

    // Remove any leftover connection string
    this._settings.connect.removeAttribute("desc");

    // Sync will use the account value and munge it into a username, as needed
    Weave.Service.account = this._settings.account.value;
    Weave.Service.login(Weave.Service.username, this._settings.pass.value, this.normalizePassphrase(this._settings.secret.value));
    Weave.Service.persistLogin();
  },

  disconnect: function disconnect() {
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

  get _settings() {
    // Do a quick test to see if the options exist yet
    let syncButton = document.getElementById("sync-syncButton");
    if (syncButton == null)
      return;

    // Get all the setting nodes from the add-ons display
    let settings = {};
    let ids = ["account", "pass", "secret", "device", "connect", "disconnect", "sync"];
    ids.forEach(function(id) {
      settings[id] = document.getElementById("sync-" + id);
    });

    // Replace the getter with the collection of settings
    delete this._settings;
    return this._settings = settings;
  },

  observe: function observe(aSubject, aTopic, aData) {
    let loggedIn = Weave.Service.isLoggedIn;
    document.getElementById("cmd_remoteTabs").setAttribute("disabled", !loggedIn);

    // Make sure we're online when connecting/syncing
    Util.forceOnline();

    // Can't do anything before settings are loaded
    if (this._settings == null)
      return;

    // Make some aliases
    let account = this._settings.account;
    let pass = this._settings.pass;
    let secret = this._settings.secret;
    let connect = this._settings.connect;
    let device = this._settings.device;
    let disconnect = this._settings.disconnect;
    let sync = this._settings.sync;
    let syncStr = Weave.Str.sync;

    // Make sure the options are in the right state
    account.collapsed = loggedIn;
    pass.collapsed = loggedIn;
    secret.collapsed = loggedIn;
    connect.collapsed = loggedIn;
    device.collapsed = !loggedIn;
    disconnect.collapsed = !loggedIn;
    sync.collapsed = !loggedIn;

    // Check the lock on a timeout because it's set just after notifying
    setTimeout(function() {
      // Prevent certain actions when the service is locked
      if (Weave.Service.locked) {
        connect.firstChild.disabled = true;
        sync.firstChild.disabled = true;

        if (aTopic == "weave:service:login:start")
          connect.setAttribute("title", syncStr.get("connecting.label"));

        if (aTopic == "weave:service:sync:start")
          sync.setAttribute("title", syncStr.get("lastSyncInProgress.label"));
      } else {
        connect.firstChild.disabled = false;
        sync.firstChild.disabled = false;
        connect.setAttribute("title", syncStr.get("disconnected.label"));
      }
    }, 0);

    // Move the disconnect and sync settings out to make connect the last item
    let parent = connect.parentNode;
    if (!loggedIn)
      parent = parent.parentNode;
    parent.appendChild(disconnect);
    parent.appendChild(sync);

    // Dynamically generate some strings
    let connectedStr = syncStr.get("connected.label", [Weave.Service.account]);
    disconnect.setAttribute("title", connectedStr);

    // Show the day-of-week and time (HH:MM) of last sync
    let lastSync = Weave.Svc.Prefs.get("lastSync");
    if (lastSync != null) {
      let syncDate = new Date(lastSync).toLocaleFormat("%a %R");
      let dateStr = syncStr.get("lastSync.label", [syncDate]);
      sync.setAttribute("title", dateStr);
    }

    // Show what went wrong with login if necessary
    if (aTopic == "weave:service:login:error")
      connect.setAttribute("desc", Weave.Utils.getErrorString(Weave.Status.login));
    else
      connect.removeAttribute("desc");

    // Load the values for the string inputs
    this.loadInputs();
  },

  loadInputs: function loadInputs() {
    this._settings.account.value = Weave.Service.account || "";
    this._settings.pass.value = Weave.Service.password || "";
    let pp = Weave.Service.passphrase || "";
    if (pp.length == 20)
      pp = this.hyphenatePassphrase(pp);
    this._settings.secret.value = pp;
    this._settings.device.value = Weave.Clients.localName || "";
  },

  changeName: function changeName(aInput) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Clients.localName = aInput.value;
    aInput.value = Weave.Clients.localName;
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
