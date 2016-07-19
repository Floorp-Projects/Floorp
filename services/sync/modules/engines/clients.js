/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "ClientEngine",
  "ClientsRec"
];

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/stringbundle.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts",
  "resource://gre/modules/FxAccounts.jsm");

const CLIENTS_TTL = 1814400; // 21 days
const CLIENTS_TTL_REFRESH = 604800; // 7 days
const STALE_CLIENT_REMOTE_AGE = 604800; // 7 days

const SUPPORTED_PROTOCOL_VERSIONS = ["1.1", "1.5"];

function hasDupeCommand(commands, action) {
  if (!commands) {
    return false;
  }
  return commands.some(other => other.command == action.command &&
    Utils.deepEquals(other.args, action.args));
}

this.ClientsRec = function ClientsRec(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
ClientsRec.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Sync.Record.Clients",
  ttl: CLIENTS_TTL
};

Utils.deferGetSet(ClientsRec,
                  "cleartext",
                  ["name", "type", "commands",
                   "version", "protocols",
                   "formfactor", "os", "appPackage", "application", "device"]);


this.ClientEngine = function ClientEngine(service) {
  SyncEngine.call(this, "Clients", service);

  // Reset the client on every startup so that we fetch recent clients
  this._resetClient();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: ClientStore,
  _recordObj: ClientsRec,
  _trackerObj: ClientsTracker,

  // Always sync client data as it controls other sync behavior
  get enabled() {
    return true;
  },

  get lastRecordUpload() {
    return Svc.Prefs.get(this.name + ".lastRecordUpload", 0);
  },
  set lastRecordUpload(value) {
    Svc.Prefs.set(this.name + ".lastRecordUpload", Math.floor(value));
  },

  get remoteClients() {
    // return all non-stale clients for external consumption.
    return Object.values(this._store._remoteClients).filter(v => !v.stale);
  },

  remoteClientExists(id) {
    let client = this._store._remoteClients[id];
    return !!(client && !client.stale);
  },

  // Aggregate some stats on the composition of clients on this account
  get stats() {
    let stats = {
      hasMobile: this.localType == DEVICE_TYPE_MOBILE,
      names: [this.localName],
      numClients: 1,
    };

    for (let id in this._store._remoteClients) {
      let {name, type, stale} = this._store._remoteClients[id];
      if (!stale) {
        stats.hasMobile = stats.hasMobile || type == DEVICE_TYPE_MOBILE;
        stats.names.push(name);
        stats.numClients++;
      }
    }

    return stats;
  },

  /**
   * Obtain information about device types.
   *
   * Returns a Map of device types to integer counts.
   */
  get deviceTypes() {
    let counts = new Map();

    counts.set(this.localType, 1);

    for (let id in this._store._remoteClients) {
      let record = this._store._remoteClients[id];
      if (record.stale) {
        continue; // pretend "stale" records don't exist.
      }
      let type = record.type;
      if (!counts.has(type)) {
        counts.set(type, 0);
      }

      counts.set(type, counts.get(type) + 1);
    }

    return counts;
  },

  get localID() {
    // Generate a random GUID id we don't have one
    let localID = Svc.Prefs.get("client.GUID", "");
    return localID == "" ? this.localID = Utils.makeGUID() : localID;
  },
  set localID(value) {
    Svc.Prefs.set("client.GUID", value);
  },

  get brandName() {
    let brand = new StringBundle("chrome://branding/locale/brand.properties");
    return brand.get("brandShortName");
  },

  get localName() {
    let name = Utils.getDeviceName();
    // If `getDeviceName` returns the default name, set the pref. FxA registers
    // the device before syncing, so we don't need to update the registration
    // in this case.
    Svc.Prefs.set("client.name", name);
    return name;
  },
  set localName(value) {
    Svc.Prefs.set("client.name", value);
    // Update the registration in the background.
    fxAccounts.updateDeviceRegistration().catch(error => {
      this._log.warn("failed to update fxa device registration", error);
    });
  },

  get localType() {
    return Utils.getDeviceType();
  },
  set localType(value) {
    Svc.Prefs.set("client.type", value);
  },

  getClientName(id) {
    if (id == this.localID) {
      return this.localName;
    }
    let client = this._store._remoteClients[id];
    return client ? client.name : "";
  },

  isMobile: function isMobile(id) {
    if (this._store._remoteClients[id])
      return this._store._remoteClients[id].type == DEVICE_TYPE_MOBILE;
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

  _processIncoming() {
    // Fetch all records from the server.
    this.lastSync = 0;
    this._incomingClients = {};
    try {
      SyncEngine.prototype._processIncoming.call(this);
      // Since clients are synced unconditionally, any records in the local store
      // that don't exist on the server must be for disconnected clients. Remove
      // them, so that we don't upload records with commands for clients that will
      // never see them. We also do this to filter out stale clients from the
      // tabs collection, since showing their list of tabs is confusing.
      for (let id in this._store._remoteClients) {
        if (!this._incomingClients[id]) {
          this._log.info(`Removing local state for deleted client ${id}`);
          this._removeRemoteClient(id);
        }
      }
      // Bug 1264498: Mobile clients don't remove themselves from the clients
      // collection when the user disconnects Sync, so we mark as stale clients
      // with the same name that haven't synced in over a week.
      // (Note we can't simply delete them, or we re-apply them next sync - see
      // bug 1287687)
      delete this._incomingClients[this.localID];
      let names = new Set([this.localName]);
      for (let id in this._incomingClients) {
        let record = this._store._remoteClients[id];
        if (!names.has(record.name)) {
          names.add(record.name);
          continue;
        }
        let remoteAge = AsyncResource.serverTime - this._incomingClients[id];
        if (remoteAge > STALE_CLIENT_REMOTE_AGE) {
          this._log.info(`Hiding stale client ${id} with age ${remoteAge}`);
          record.stale = true;
        }
      }
    } finally {
      this._incomingClients = null;
    }
  },

  _uploadOutgoing() {
    this._clearedCommands = null;
    SyncEngine.prototype._uploadOutgoing.call(this);
  },

  _syncFinish() {
    // Record histograms for our device types, and also write them to a pref
    // so non-histogram telemetry (eg, UITelemetry) has easy access to them.
    for (let [deviceType, count] of this.deviceTypes) {
      let hid;
      let prefName = this.name + ".devices.";
      switch (deviceType) {
        case "desktop":
          hid = "WEAVE_DEVICE_COUNT_DESKTOP";
          prefName += "desktop";
          break;
        case "mobile":
          hid = "WEAVE_DEVICE_COUNT_MOBILE";
          prefName += "mobile";
          break;
        default:
          this._log.warn(`Unexpected deviceType "${deviceType}" recording device telemetry.`);
          continue;
      }
      Services.telemetry.getHistogramById(hid).add(count);
      Svc.Prefs.set(prefName, count);
    }
    SyncEngine.prototype._syncFinish.call(this);
  },

  _reconcile: function _reconcile(item) {
    // Every incoming record is reconciled, so we use this to track the
    // contents of the collection on the server.
    this._incomingClients[item.id] = item.modified;

    if (!this._store.itemExists(item.id)) {
      return true;
    }
    // Clients are synced unconditionally, so we'll always have new records.
    // Unfortunately, this will cause the scheduler to use the immediate sync
    // interval for the multi-device case, instead of the active interval. We
    // work around this by updating the record during reconciliation, and
    // returning false to indicate that the record doesn't need to be applied
    // later.
    this._store.update(item);
    return false;
  },

  // Treat reset the same as wiping for locally cached clients
  _resetClient() {
    this._wipeClient();
  },

  _wipeClient: function _wipeClient() {
    SyncEngine.prototype._resetClient.call(this);
    delete this.localCommands;
    this._store.wipe();
  },

  removeClientData: function removeClientData() {
    let res = this.service.resource(this.engineURL + "/" + this.localID);
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
    if (!this._clearedCommands) {
      this._clearedCommands = [];
    }
    // Keep track of cleared local commands until the next sync, so that we
    // don't reupload them.
    this._clearedCommands = this._clearedCommands.concat(this.localCommands);
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
    if (client.stale) {
      throw new Error("Stale remote client ID: '" + clientId + "'.");
    }

    let action = {
      command: command,
      args: args,
    };

    if (!client.commands) {
      client.commands = [action];
    }
    // Add the new action if there are no duplicates.
    else if (!hasDupeCommand(client.commands, action)) {
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
      if (!commands) {
        return true;
      }
      let URIsToDisplay = [];
      for (let key in commands) {
        let {command, args} = commands[key];
        this._log.debug("Processing command: " + command + "(" + args + ")");

        let engines = [args[0]];
        switch (command) {
          case "resetAll":
            engines = null;
            // Fallthrough
          case "resetEngine":
            this.service.resetClient(engines);
            break;
          case "wipeAll":
            engines = null;
            // Fallthrough
          case "wipeEngine":
            this.service.wipeClient(engines);
            break;
          case "logout":
            this.service.logout();
            return false;
          case "displayURI":
            let [uri, clientId, title] = args;
            URIsToDisplay.push({ uri, clientId, title });
            break;
          default:
            this._log.debug("Received an unknown command: " + command);
            break;
        }
      }
      if (URIsToDisplay.length) {
        this._handleDisplayURIs(URIsToDisplay);
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
      for (let [id, record] in Iterator(this._store._remoteClients)) {
        if (!record.stale) {
          this._sendCommandToClient(command, args, id);
        }
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

    this._tracker.score += SCORE_INCREMENT_XLARGE;
  },

  /**
   * Handle a bunch of received 'displayURI' commands.
   *
   * Interested parties should observe the "weave:engine:clients:display-uris"
   * topic. The callback will receive an array as the subject parameter
   * containing objects with the following keys:
   *
   *   uri       URI (string) that is requested for display.
   *   clientId  ID of client that sent the command.
   *   title     Title of page that loaded URI (likely) corresponds to.
   *
   * The 'data' parameter to the callback will not be defined.
   *
   * @param uris
   *        An array containing URI objects to display
   * @param uris[].uri
   *        String URI that was received
   * @param uris[].clientId
   *        ID of client that sent URI
   * @param uris[].title
   *        String title of page that URI corresponds to. Older clients may not
   *        send this.
   */
  _handleDisplayURIs: function _handleDisplayURIs(uris) {
    Svc.Obs.notify("weave:engine:clients:display-uris", uris);
  },

  _removeRemoteClient(id) {
    delete this._store._remoteClients[id];
    this._tracker.removeChangedID(id);
  },
};

function ClientStore(name, engine) {
  Store.call(this, name, engine);
}
ClientStore.prototype = {
  __proto__: Store.prototype,

  create(record) {
    this.update(record)
  },

  update: function update(record) {
    if (record.id == this.engine.localID) {
      this._updateLocalRecord(record);
    } else {
      this._updateRemoteRecord(record);
    }
  },

  _updateLocalRecord(record) {
    // Local changes for our client means we're clearing commands or
    // uploading a new record.
    let incomingCommands = record.commands;
    if (incomingCommands) {
      // Filter out incoming commands that we've cleared.
      incomingCommands = incomingCommands.filter(action =>
        !hasDupeCommand(this.engine._clearedCommands, action));
      if (!incomingCommands.length) {
        // Use `undefined` instead of `null` to avoid creating a null field
        // in the uploaded record.
        incomingCommands = undefined;
      }
    }
    // Only grab commands from the server; local name/type always wins
    this.engine.localCommands = incomingCommands;
  },

  _updateRemoteRecord(record) {
    let currentRecord = this._remoteClients[record.id];
    if (!currentRecord || !currentRecord.commands ||
        !(record.id in this.engine._modified)) {

      // If we have a new incoming record or no outgoing commands, use the
      // full incoming record from the server.
      this._remoteClients[record.id] = record.cleartext;
      return;
    }

    // Otherwise, we have outgoing commands for a client, so merge them
    // with the commands that we downloaded from the server.
    for (let action of currentRecord.commands) {
      if (hasDupeCommand(record.cleartext.commands, action)) {
        // Ignore commands the server already knows about.
        continue;
      }
      if (record.cleartext.commands) {
        record.cleartext.commands.push(action);
      } else {
        record.cleartext.commands = [action];
      }
    }
    this._remoteClients[record.id] = record.cleartext;
  },

  createRecord: function createRecord(id, collection) {
    let record = new ClientsRec(collection, id);

    // Package the individual components into a record for the local client
    if (id == this.engine.localID) {
      let cb = Async.makeSpinningCallback();
      fxAccounts.getDeviceId().then(id => cb(null, id), cb);
      try {
        record.fxaDeviceId = cb.wait();
      } catch(error) {
        this._log.warn("failed to get fxa device id", error);
      }
      record.name = this.engine.localName;
      record.type = this.engine.localType;
      record.commands = this.engine.localCommands;
      record.version = Services.appinfo.version;
      record.protocols = SUPPORTED_PROTOCOL_VERSIONS;

      // Optional fields.
      record.os = Services.appinfo.OS;             // "Darwin"
      record.appPackage = Services.appinfo.ID;
      record.application = this.engine.brandName   // "Nightly"

      // We can't compute these yet.
      // record.device = "";            // Bug 1100723
      // record.formfactor = "";        // Bug 1100722
    } else {
      record.cleartext = this._remoteClients[id];
      if (record.cleartext.stale) {
        // It's almost certainly a logic error for us to upload a record we
        // consider stale, so make log noise, but still remove the flag.
        this._log.error(`Preparing to upload record ${id} that we consider stale`);
        delete record.cleartext.stale;
      }
    }

    return record;
  },

  itemExists(id) {
    return id in this.getAllIDs();
  },

  getAllIDs: function getAllIDs() {
    let ids = {};
    ids[this.engine.localID] = true;
    for (let id in this._remoteClients)
      ids[id] = true;
    return ids;
  },

  wipe: function wipe() {
    this._remoteClients = {};
  },
};

function ClientsTracker(name, engine) {
  Tracker.call(this, name, engine);
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
          Svc.Prefs.ignore("client.name", this);
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
