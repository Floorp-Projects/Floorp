/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["Clients", "ClientsRec"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://services-common/stringbundle.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/main.js");

const CLIENTS_TTL = 1814400; // 21 days
const CLIENTS_TTL_REFRESH = 604800; // 7 days

function ClientsRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
ClientsRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Clients",
  ttl: CLIENTS_TTL
};

Utils.deferGetSet(ClientsRec, "cleartext", ["name", "type", "commands"]);


XPCOMUtils.defineLazyGetter(this, "Clients", function () {
  return new ClientEngine();
});

function ClientEngine() {
  SyncEngine.call(this, "Clients");

  // Reset the client on every startup so that we fetch recent clients
  this._resetClient();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: ClientStore,
  _recordObj: ClientsRec,
  _trackerObj: ClientsTracker,

  // Always sync client data as it controls other sync behavior
  get enabled() true,

  get lastRecordUpload() {
    return Svc.Prefs.get(this.name + ".lastRecordUpload", 0);
  },
  set lastRecordUpload(value) {
    Svc.Prefs.set(this.name + ".lastRecordUpload", Math.floor(value));
  },

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
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    let user = env.get("USER") || env.get("USERNAME") ||
               Svc.Prefs.get("account") || Svc.Prefs.get("username");
    let brand = new StringBundle("chrome://branding/locale/brand.properties");
    let app = brand.get("brandShortName");

    let system = Cc["@mozilla.org/system-info;1"]
                   .getService(Ci.nsIPropertyBag2).get("device") ||
                 Cc["@mozilla.org/network/protocol;1?name=http"]
                   .getService(Ci.nsIHttpProtocolHandler).oscpu;

    return this.localName = Str.sync.get("client.name2", [user, app, system]);
  },
  set localName(value) Svc.Prefs.set("client.name", value),

  get localType() Svc.Prefs.get("client.type", "desktop"),
  set localType(value) Svc.Prefs.set("client.type", value),

  isMobile: function isMobile(id) {
    if (this._store._remoteClients[id])
      return this._store._remoteClients[id].type == "mobile";
    return false;
  },

  _syncStartup: function _syncStartup() {
    // Reupload new client record periodically.
    if (Date.now() / 1000 - this.lastRecordUpload > CLIENTS_TTL_REFRESH) {
      this._tracker.addChangedID(this.localID);
      this.lastRecordUpload = Date.now() / 1000;
    }
    SyncEngine.prototype._syncStartup.call(this);
  },

  // Always process incoming items because they might have commands
  _reconcile: function _reconcile() {
    return true;
  },

  // Treat reset the same as wiping for locally cached clients
  _resetClient: function _resetClient() this._wipeClient(),

  _wipeClient: function _wipeClient() {
    SyncEngine.prototype._resetClient.call(this);
    this._store.wipe();
  },

  removeClientData: function removeClientData() {
    let res = new Resource(this.engineURL + "/" + this.localID);
    res.delete();
  },

  // Override the default behavior to delete bad records from the server.
  handleHMACMismatch: function handleHMACMismatch(item, mayRetry) {
    this._log.debug("Handling HMAC mismatch for " + item.id);

    let base = SyncEngine.prototype.handleHMACMismatch.call(this, item, mayRetry);
    if (base != SyncEngine.kRecoveryStrategy.error)
      return base;

    // It's a bad client record. Save it to be deleted at the end of the sync.
    this._log.debug("Bad client record detected. Scheduling for deletion.");
    this._deleteId(item.id);

    // Neither try again nor error; we're going to delete it.
    return SyncEngine.kRecoveryStrategy.ignore;
  },

  /**
   * A hash of valid commands that the client knows about. The key is a command
   * and the value is a hash containing information about the command such as
   * number of arguments and description.
   */
  _commands: {
    resetAll:    { args: 0, desc: "Clear temporary local data for all engines" },
    resetEngine: { args: 1, desc: "Clear temporary local data for engine" },
    wipeAll:     { args: 0, desc: "Delete all client data for all engines" },
    wipeEngine:  { args: 1, desc: "Delete all client data for engine" },
    logout:      { args: 0, desc: "Log out client" },
    displayURI:  { args: 3, desc: "Instruct a client to display a URI" },
  },

  /**
   * Remove any commands for the local client and mark it for upload.
   */
  clearCommands: function clearCommands() {
    delete this.localCommands;
    this._tracker.addChangedID(this.localID);
  },

  /**
   * Sends a command+args pair to a specific client.
   *
   * @param command Command string
   * @param args Array of arguments/data for command
   * @param clientId Client to send command to
   */
  _sendCommandToClient: function sendCommandToClient(command, args, clientId) {
    this._log.trace("Sending " + command + " to " + clientId);

    let client = this._store._remoteClients[clientId];
    if (!client) {
      throw new Error("Unknown remote client ID: '" + clientId + "'.");
    }

    // notDupe compares two commands and returns if they are not equal.
    let notDupe = function(other) {
      return other.command != command || !Utils.deepEquals(other.args, args);
    };

    let action = {
      command: command,
      args: args,
    };

    if (!client.commands) {
      client.commands = [action];
    }
    // Add the new action if there are no duplicates.
    else if (client.commands.every(notDupe)) {
      client.commands.push(action);
    }
    // It must be a dupe. Skip.
    else {
      return;
    }

    this._log.trace("Client " + clientId + " got a new action: " + [command, args]);
    this._tracker.addChangedID(clientId);
  },

  /**
   * Check if the local client has any remote commands and perform them.
   *
   * @return false to abort sync
   */
  processIncomingCommands: function processIncomingCommands() {
    return this._notify("clients:process-commands", "", function() {
      let commands = this.localCommands;

      // Immediately clear out the commands as we've got them locally.
      this.clearCommands();

      // Process each command in order.
      for each ({command: command, args: args} in commands) {
        this._log.debug("Processing command: " + command + "(" + args + ")");

        let engines = [args[0]];
        switch (command) {
          case "resetAll":
            engines = null;
            // Fallthrough
          case "resetEngine":
            Weave.Service.resetClient(engines);
            break;
          case "wipeAll":
            engines = null;
            // Fallthrough
          case "wipeEngine":
            Weave.Service.wipeClient(engines);
            break;
          case "logout":
            Weave.Service.logout();
            return false;
          case "displayURI":
            this._handleDisplayURI.apply(this, args);
            break;
          default:
            this._log.debug("Received an unknown command: " + command);
            break;
        }
      }

      return true;
    })();
  },

  /**
   * Validates and sends a command to a client or all clients.
   *
   * Calling this does not actually sync the command data to the server. If the
   * client already has the command/args pair, it won't receive a duplicate
   * command.
   *
   * @param command
   *        Command to invoke on remote clients
   * @param args
   *        Array of arguments to give to the command
   * @param clientId
   *        Client ID to send command to. If undefined, send to all remote
   *        clients.
   */
  sendCommand: function sendCommand(command, args, clientId) {
    let commandData = this._commands[command];
    // Don't send commands that we don't know about.
    if (!commandData) {
      this._log.error("Unknown command to send: " + command);
      return;
    }
    // Don't send a command with the wrong number of arguments.
    else if (!args || args.length != commandData.args) {
      this._log.error("Expected " + commandData.args + " args for '" +
                      command + "', but got " + args);
      return;
    }

    if (clientId) {
      this._sendCommandToClient(command, args, clientId);
    } else {
      for (let id in this._store._remoteClients) {
        this._sendCommandToClient(command, args, id);
      }
    }
  },

  /**
   * Send a URI to another client for display.
   *
   * A side effect is the score is increased dramatically to incur an
   * immediate sync.
   *
   * If an unknown client ID is specified, sendCommand() will throw an
   * Error object.
   *
   * @param uri
   *        URI (as a string) to send and display on the remote client
   * @param clientId
   *        ID of client to send the command to. If not defined, will be sent
   *        to all remote clients.
   * @param title
   *        Title of the page being sent.
   */
  sendURIToClientForDisplay: function sendURIToClientForDisplay(uri, clientId, title) {
    this._log.info("Sending URI to client: " + uri + " -> " +
                   clientId + " (" + title + ")");
    this.sendCommand("displayURI", [uri, this.localID, title], clientId);

    Clients._tracker.score += SCORE_INCREMENT_XLARGE;
  },

  /**
   * Handle a single received 'displayURI' command.
   *
   * Interested parties should observe the "weave:engine:clients:display-uri"
   * topic. The callback will receive an object as the subject parameter with
   * the following keys:
   *
   *   uri       URI (string) that is requested for display.
   *   clientId  ID of client that sent the command.
   *   title     Title of page that loaded URI (likely) corresponds to.
   *
   * The 'data' parameter to the callback will not be defined.
   *
   * @param uri
   *        String URI that was received
   * @param clientId
   *        ID of client that sent URI
   * @param title
   *        String title of page that URI corresponds to. Older clients may not
   *        send this.
   */
  _handleDisplayURI: function _handleDisplayURI(uri, clientId, title) {
    this._log.info("Received a URI for display: " + uri + " (" + title +
                   ") from " + clientId);

    let subject = {uri: uri, client: clientId, title: title};
    Svc.Obs.notify("weave:engine:clients:display-uri", subject);
  }
};

function ClientStore(name) {
  Store.call(this, name);
}
ClientStore.prototype = {
  __proto__: Store.prototype,

  create: function create(record) this.update(record),

  update: function update(record) {
    // Only grab commands from the server; local name/type always wins
    if (record.id == Clients.localID)
      Clients.localCommands = record.commands;
    else
      this._remoteClients[record.id] = record.cleartext;
  },

  createRecord: function createRecord(id, collection) {
    let record = new ClientsRec(collection, id);

    // Package the individual components into a record for the local client
    if (id == Clients.localID) {
      record.name = Clients.localName;
      record.type = Clients.localType;
      record.commands = Clients.localCommands;
    }
    else
      record.cleartext = this._remoteClients[id];

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

function ClientsTracker(name) {
  Tracker.call(this, name);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
ClientsTracker.prototype = {
  __proto__: Tracker.prototype,

  _enabled: false,

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          Svc.Prefs.observe("client.name", this);
          this._enabled = true;
        }
        break;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          Svc.Prefs.ignore("clients.name", this);
          this._enabled = false;
        }
        break;
      case "nsPref:changed":
        this._log.debug("client.name preference changed");
        this.addChangedID(Svc.Prefs.get("client.GUID"));
        this.score += SCORE_INCREMENT_XLARGE;
        break;
    }
  }
};
