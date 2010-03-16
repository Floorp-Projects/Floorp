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
 * The Original Code is Weave
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

const EXPORTED_SYMBOLS = ["Clients"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/engines.js");
Cu.import("resource://weave/stores.js");
Cu.import("resource://weave/type_records/clients.js");

Utils.lazy(this, "Clients", ClientEngine);

function ClientEngine() {
  SyncEngine.call(this, "Clients");

  // Reset the client on every startup so that we fetch recent clients
  this._resetClient();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: ClientStore,
  _recordObj: ClientsRec,

  // Aggregate some stats on the composition of clients on this account
  get stats() {
    let stats = {
      hasMobile: this.localType == "mobile",
      names: [this.localName],
      numClients: 1,
    };

    for each (let {name, type} in this._store._remoteClients) {
      stats.hasMobile = stats.hasMobile || type == "mobile";
      stats.names.push(name);
      stats.numClients++;
    }

    return stats;
  },

  // Remove any commands for the local client and mark it for upload
  clearCommands: function clearCommands() {
    delete this.localCommands;
    this._tracker.addChangedID(this.localID);
  },

  // Send a command+args pair to each remote client
  sendCommand: function sendCommand(command, args) {
    // Helper to determine if the client already has this command
    let notDupe = function(other) other.command != command ||
      JSON.stringify(other.args) != JSON.stringify(args);

    // Package the command/args pair into an object
    let action = {
      command: command,
      args: args,
    };

    // Send the command to each remote client
    for (let [id, client] in Iterator(this._store._remoteClients)) {
      // Set the action to be a new commands array if none exists
      if (client.commands == null)
        client.commands = [action];
      // Add the new action if there are no duplicates
      else if (client.commands.every(notDupe))
        client.commands.push(action);
      // Must have been a dupe.. skip!
      else
        continue;

      this._log.trace("Client " + id + " got a new action: " + [command, args]);
      this._tracker.addChangedID(id);
    }
  },

  get localID() {
    // Generate a random GUID id we don't have one
    let localID = Svc.Prefs.get("client.GUID", "");
    return localID == "" ? this.localID = Utils.makeGUID() : localID;
  },
  set localID(value) Svc.Prefs.set("client.GUID", value),

  get localName() {
    let localName = Svc.Prefs.get("client.name", "");
    if (localName != "")
      return localName;

    // Generate a client name if we don't have a useful one yet
    let user = Svc.Env.get("USER") || Svc.Env.get("USERNAME");
    let app = Svc.AppInfo.name;
    let host = Svc.SysInfo.get("host");

    // Try figuring out the name of the current profile
    let prof = Svc.Directory.get("ProfD", Components.interfaces.nsIFile).path;
    let profiles = Svc.Profiles.profiles;
    while (profiles.hasMoreElements()) {
      let profile = profiles.getNext().QueryInterface(Ci.nsIToolkitProfile);
      if (prof == profile.rootDir.path) {
        // Only bother adding the profile name if it's not "default"
        if (profile.name != "default")
          host = profile.name + "-" + host;
        break;
      }
    }

    return this.localName = Str.sync.get("client.name", [user, app, host]);
  },
  set localName(value) Svc.Prefs.set("client.name", value),

  get localType() {
    // Figure out if we have a type previously set
    let localType = Svc.Prefs.get("client.type", "");
    if (localType == "") {
      // Assume we're desktop-like unless the app is for mobiles
      localType = "desktop";
      switch (Svc.AppInfo.ID) {
        case FENNEC_ID:
          localType = "mobile";
          break;
      }
      this.localType = localType;
    }
    return localType;
  },
  set localType(value) Svc.Prefs.set("client.type", value),

  // Always process incoming items because they might have commands
  _reconcile: function _reconcile() {
    return true;
  },

  // Treat reset the same as wiping for locally cached clients
  _resetClient: function _resetClient() this._wipeClient(),

  _wipeClient: function _wipeClient() {
    SyncEngine.prototype._resetClient.call(this);
    this._store.wipe();
  }
};

function ClientStore(name) {
  Store.call(this, name);
}
ClientStore.prototype = {
  __proto__: Store.prototype,

  create: function create(record) this.update(record),

  update: function update(record) {
    // Unpack the individual components of the local client
    if (record.id == Clients.localID) {
      Clients.localName = record.name;
      Clients.localType = record.type;
      Clients.localCommands = record.commands;
    }
    else
      this._remoteClients[record.id] = record.cleartext;
  },

  createRecord: function createRecord(guid) {
    let record = new ClientsRec();

    // Package the individual components into a record for the local client
    if (guid == Clients.localID) {
      record.name = Clients.localName;
      record.type = Clients.localType;
      record.commands = Clients.localCommands;
    }
    else
      record.cleartext = this._remoteClients[guid];

    return record;
  },

  itemExists: function itemExists(id) id in this.getAllIDs(),

  getAllIDs: function getAllIDs() {
    let ids = {};
    ids[Clients.localID] = true;
    for (let id in this._remoteClients)
      ids[id] = true;
    return ids;
  },

  wipe: function wipe() {
    this._remoteClients = {};
  },
};
