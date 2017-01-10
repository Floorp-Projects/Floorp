/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * How does the clients engine work?
 *
 * - We use 2 files - commands.json and commands-syncing.json.
 *
 * - At sync upload time, we attempt a rename of commands.json to
 *   commands-syncing.json, and ignore errors (helps for crash during sync!).
 * - We load commands-syncing.json and stash the contents in
 *   _currentlySyncingCommands which lives for the duration of the upload process.
 * - We use _currentlySyncingCommands to build the outgoing records
 * - Immediately after successful upload, we delete commands-syncing.json from
 *   disk (and clear _currentlySyncingCommands). We reconcile our local records
 *   with what we just wrote in the server, and add failed IDs commands
 *   back in commands.json
 * - Any time we need to "save" a command for future syncs, we load
 *   commands.json, update it, and write it back out.
 */

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
                   "formfactor", "os", "appPackage", "application", "device",
                   "fxaDeviceId"]);


this.ClientEngine = function ClientEngine(service) {
  SyncEngine.call(this, "Clients", service);

  // Reset the last sync timestamp on every startup so that we fetch all clients
  this.resetLastSync();
}
ClientEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: ClientStore,
  _recordObj: ClientsRec,
  _trackerObj: ClientsTracker,
  allowSkippedRecord: false,

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

  getClientFxaDeviceId(id) {
    if (this._store._remoteClients[id]) {
      return this._store._remoteClients[id].fxaDeviceId;
    }
    return null;
  },

  isMobile: function isMobile(id) {
    if (this._store._remoteClients[id])
      return this._store._remoteClients[id].type == DEVICE_TYPE_MOBILE;
    return false;
  },

  _readCommands() {
    let cb = Async.makeSpinningCallback();
    Utils.jsonLoad("commands", this, commands => cb(null, commands));
    return cb.wait() || {};
  },

  /**
   * Low level function, do not use directly (use _addClientCommand instead).
   */
  _saveCommands(commands) {
    let cb = Async.makeSpinningCallback();
    Utils.jsonSave("commands", this, commands, error => {
      if (error) {
        this._log.error("Failed to save JSON outgoing commands", error);
      }
      cb();
    });
    cb.wait();
  },

  _prepareCommandsForUpload() {
    let cb = Async.makeSpinningCallback();
    Utils.jsonMove("commands", "commands-syncing", this).catch(() => {}) // Ignore errors
      .then(() => {
        Utils.jsonLoad("commands-syncing", this, commands => cb(null, commands));
      });
    return cb.wait() || {};
  },

  _deleteUploadedCommands() {
    delete this._currentlySyncingCommands;
    Async.promiseSpinningly(
      Utils.jsonRemove("commands-syncing", this).catch(err => {
        this._log.error("Failed to delete syncing-commands file", err);
      })
    );
  },

  _addClientCommand(clientId, command) {
    const allCommands = this._readCommands();
    const clientCommands = allCommands[clientId] || [];
    if (hasDupeCommand(clientCommands, command)) {
      return false;
    }
    allCommands[clientId] = clientCommands.concat(command);
    this._saveCommands(allCommands);
    return true;
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
    this._currentlySyncingCommands = this._prepareCommandsForUpload();
    const clientWithPendingCommands = Object.keys(this._currentlySyncingCommands);
    for (let clientId of clientWithPendingCommands) {
      if (this._store._remoteClients[clientId] || this.localID == clientId) {
        this._modified.set(clientId, 0);
      }
    }
    SyncEngine.prototype._uploadOutgoing.call(this);
  },

  _onRecordsWritten(succeeded, failed) {
    // Reconcile the status of the local records with what we just wrote on the
    // server
    for (let id of succeeded) {
      const commandChanges = this._currentlySyncingCommands[id];
      if (id == this.localID) {
        if (this.localCommands) {
          this.localCommands = this.localCommands.filter(command => !hasDupeCommand(commandChanges, command));
        }
      } else {
        const clientRecord = this._store._remoteClients[id];
        if (!commandChanges || !clientRecord) {
          // should be impossible, else we wouldn't have been writing it.
          this._log.warn("No command/No record changes for a client we uploaded");
          continue;
        }
        // fixup the client record, so our copy of _remoteClients matches what we uploaded.
        clientRecord.commands = this._store.createRecord(id);
        // we could do better and pass the reference to the record we just uploaded,
        // but this will do for now
      }
    }

    // Re-add failed commands
    for (let id of failed) {
      const commandChanges = this._currentlySyncingCommands[id];
      if (!commandChanges) {
        continue;
      }
      this._addClientCommand(id, commandChanges);
    }

    this._deleteUploadedCommands();

    // Notify other devices that their own client collection changed
    const idsToNotify = succeeded.reduce((acc, id) => {
      if (id == this.localID) {
        return acc;
      }
      const fxaDeviceId = this.getClientFxaDeviceId(id);
      return fxaDeviceId ? acc.concat(fxaDeviceId) : acc;
    }, []);
    if (idsToNotify.length > 0) {
      this._notifyCollectionChanged(idsToNotify);
    }
  },

  _notifyCollectionChanged(ids) {
    const message = {
      version: 1,
      command: "sync:collection_changed",
      data: {
        collections: ["clients"]
      }
    };
    fxAccounts.notifyDevices(ids, message, NOTIFY_TAB_SENT_TTL_SECS);
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
    const logRemoveError = err => this._log.warn("Could not delete json file", err);
    Async.promiseSpinningly(
      Utils.jsonRemove("commands", this).catch(logRemoveError)
        .then(Utils.jsonRemove("commands-syncing", this).catch(logRemoveError))
    );
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
   * Sends a command+args pair to a specific client.
   *
   * @param command Command string
   * @param args Array of arguments/data for command
   * @param clientId Client to send command to
   */
  _sendCommandToClient: function sendCommandToClient(command, args, clientId, flowID = null) {
    this._log.trace("Sending " + command + " to " + clientId);

    let client = this._store._remoteClients[clientId];
    if (!client) {
      throw new Error("Unknown remote client ID: '" + clientId + "'.");
    }
    if (client.stale) {
      throw new Error("Stale remote client ID: '" + clientId + "'.");
    }

    let action = {
      command,
      args,
      flowID: flowID || Utils.makeGUID(), // used for telemetry.
    };

    if (this._addClientCommand(clientId, action)) {
      this._log.trace(`Client ${clientId} got a new action`, [command, args]);
      this._tracker.addChangedID(clientId);
      let deviceID;
      try {
        deviceID = this.service.identity.hashedDeviceID(clientId);
      } catch (_) {}
      this.service.recordTelemetryEvent("sendcommand", command, undefined,
                                        { flowID: action.flowID, deviceID });
    } else {
      this._log.trace(`Client ${clientId} got a duplicate action`, [command, args]);
    }
  },

  /**
   * Check if the local client has any remote commands and perform them.
   *
   * @return false to abort sync
   */
  processIncomingCommands: function processIncomingCommands() {
    return this._notify("clients:process-commands", "", function() {
      if (!this.localCommands) {
        return true;
      }

      const clearedCommands = this._readCommands()[this.localID];
      const commands = this.localCommands.filter(command => !hasDupeCommand(clearedCommands, command));

      let URIsToDisplay = [];
      // Process each command in order.
      for (let rawCommand of commands) {
        let {command, args, flowID} = rawCommand;
        this._log.debug("Processing command: " + command + "(" + args + ")");

        this.service.recordTelemetryEvent("processcommand", command, undefined,
                                          { flowID });

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
        // Add the command to the "cleared" commands list
        this._addClientCommand(this.localID, rawCommand)
      }
      this._tracker.addChangedID(this.localID);

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
   * @param flowID
   *        A unique identifier used to track success for this operation across
   *        devices.
   */
  sendCommand: function sendCommand(command, args, clientId, flowID = null) {
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
      this._sendCommandToClient(command, args, clientId, flowID);
    } else {
      for (let [id, record] of Object.entries(this._store._remoteClients)) {
        if (!record.stale) {
          this._sendCommandToClient(command, args, id, flowID);
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

  _remoteClients: {},

  create(record) {
    this.update(record);
  },

  update: function update(record) {
    if (record.id == this.engine.localID) {
      // Only grab commands from the server; local name/type always wins
      this.engine.localCommands = record.commands;
    } else {
      this._remoteClients[record.id] = record.cleartext;
    }
  },

  createRecord: function createRecord(id, collection) {
    let record = new ClientsRec(collection, id);

    const commandsChanges = this.engine._currentlySyncingCommands ?
                            this.engine._currentlySyncingCommands[id] :
                            [];

    // Package the individual components into a record for the local client
    if (id == this.engine.localID) {
      let cb = Async.makeSpinningCallback();
      fxAccounts.getDeviceId().then(id => cb(null, id), cb);
      try {
        record.fxaDeviceId = cb.wait();
      } catch (error) {
        this._log.warn("failed to get fxa device id", error);
      }
      record.name = this.engine.localName;
      record.type = this.engine.localType;
      record.version = Services.appinfo.version;
      record.protocols = SUPPORTED_PROTOCOL_VERSIONS;

      // Substract the commands we recorded that we've already executed
      if (commandsChanges && commandsChanges.length &&
          this.engine.localCommands && this.engine.localCommands.length) {
        record.commands = this.engine.localCommands.filter(command => !hasDupeCommand(commandsChanges, command));
      }

      // Optional fields.
      record.os = Services.appinfo.OS;             // "Darwin"
      record.appPackage = Services.appinfo.ID;
      record.application = this.engine.brandName   // "Nightly"

      // We can't compute these yet.
      // record.device = "";            // Bug 1100723
      // record.formfactor = "";        // Bug 1100722
    } else {
      record.cleartext = this._remoteClients[id];

      // Add the commands we have to send
      if (commandsChanges && commandsChanges.length) {
        const recordCommands = record.cleartext.commands || [];
        const newCommands = commandsChanges.filter(command => !hasDupeCommand(recordCommands, command));
        record.cleartext.commands = recordCommands.concat(newCommands);
      }

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
