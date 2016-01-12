/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains code for synchronizing engines.
 */

this.EXPORTED_SYMBOLS = ["EngineSynchronizer"];

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/util.js");

/**
 * Perform synchronization of engines.
 *
 * This was originally split out of service.js. The API needs lots of love.
 */
this.EngineSynchronizer = function EngineSynchronizer(service) {
  this._log = Log.repository.getLogger("Sync.Synchronizer");
  this._log.level = Log.Level[Svc.Prefs.get("log.logger.synchronizer")];

  this.service = service;

  this.onComplete = null;
}

EngineSynchronizer.prototype = {
  sync: function sync(engineNamesToSync) {
    if (!this.onComplete) {
      throw new Error("onComplete handler not installed.");
    }

    let startTime = Date.now();

    this.service.status.resetSync();

    // Make sure we should sync or record why we shouldn't.
    let reason = this.service._checkSync();
    if (reason) {
      if (reason == kSyncNetworkOffline) {
        this.service.status.sync = LOGIN_FAILED_NETWORK_ERROR;
      }

      // this is a purposeful abort rather than a failure, so don't set
      // any status bits
      reason = "Can't sync: " + reason;
      this.onComplete(new Error("Can't sync: " + reason));
      return;
    }

    // If we don't have a node, get one. If that fails, retry in 10 minutes.
    if (!this.service.clusterURL && !this.service._clusterManager.setCluster()) {
      this.service.status.sync = NO_SYNC_NODE_FOUND;
      this._log.info("No cluster URL found. Cannot sync.");
      this.onComplete(null);
      return;
    }

    // Ping the server with a special info request once a day.
    let infoURL = this.service.infoURL;
    let now = Math.floor(Date.now() / 1000);
    let lastPing = Svc.Prefs.get("lastPing", 0);
    if (now - lastPing > 86400) { // 60 * 60 * 24
      infoURL += "?v=" + WEAVE_VERSION;
      Svc.Prefs.set("lastPing", now);
    }

    let engineManager = this.service.engineManager;

    // Figure out what the last modified time is for each collection
    let info = this.service._fetchInfo(infoURL);

    // Convert the response to an object and read out the modified times
    for (let engine of [this.service.clientsEngine].concat(engineManager.getAll())) {
      engine.lastModified = info.obj[engine.name] || 0;
    }

    if (!(this.service._remoteSetup(info))) {
      this.onComplete(new Error("Aborting sync, remote setup failed"));
      return;
    }

    // Make sure we have an up-to-date list of clients before sending commands
    this._log.debug("Refreshing client list.");
    if (!this._syncEngine(this.service.clientsEngine)) {
      // Clients is an engine like any other; it can fail with a 401,
      // and we can elect to abort the sync.
      this._log.warn("Client engine sync failed. Aborting.");
      this.onComplete(null);
      return;
    }

    // We only honor the "hint" of what engines to Sync if this isn't
    // a first sync.
    let allowEnginesHint = false;
    // Wipe data in the desired direction if necessary
    switch (Svc.Prefs.get("firstSync")) {
      case "resetClient":
        this.service.resetClient(engineManager.enabledEngineNames);
        break;
      case "wipeClient":
        this.service.wipeClient(engineManager.enabledEngineNames);
        break;
      case "wipeRemote":
        this.service.wipeRemote(engineManager.enabledEngineNames);
        break;
      default:
        allowEnginesHint = true;
        break;
    }

    if (this.service.clientsEngine.localCommands) {
      try {
        if (!(this.service.clientsEngine.processIncomingCommands())) {
          this.service.status.sync = ABORT_SYNC_COMMAND;
          this.onComplete(new Error("Processed command aborted sync."));
          return;
        }

        // Repeat remoteSetup in-case the commands forced us to reset
        if (!(this.service._remoteSetup(info))) {
          this.onComplete(new Error("Remote setup failed after processing commands."));
          return;
        }
      }
      finally {
        // Always immediately attempt to push back the local client (now
        // without commands).
        // Note that we don't abort here; if there's a 401 because we've
        // been reassigned, we'll handle it around another engine.
        this._syncEngine(this.service.clientsEngine);
      }
    }

    // Update engines because it might change what we sync.
    try {
      this._updateEnabledEngines();
    } catch (ex) {
      this._log.debug("Updating enabled engines failed", ex);
      this.service.errorHandler.checkServerError(ex);
      this.onComplete(ex);
      return;
    }

    // If the engines to sync has been specified, we sync in the order specified.
    let enginesToSync;
    if (allowEnginesHint && engineNamesToSync) {
      this._log.info("Syncing specified engines", engineNamesToSync);
      enginesToSync = engineManager.get(engineNamesToSync).filter(e => e.enabled);
    } else {
      this._log.info("Syncing all enabled engines.");
      enginesToSync = engineManager.getEnabled();
    }
    try {
      for (let engine of enginesToSync) {
        // If there's any problems with syncing the engine, report the failure
        if (!(this._syncEngine(engine)) || this.service.status.enforceBackoff) {
          this._log.info("Aborting sync for failure in " + engine.name);
          break;
        }
      }

      // If _syncEngine fails for a 401, we might not have a cluster URL here.
      // If that's the case, break out of this immediately, rather than
      // throwing an exception when trying to fetch metaURL.
      if (!this.service.clusterURL) {
        this._log.debug("Aborting sync, no cluster URL: " +
                        "not uploading new meta/global.");
        this.onComplete(null);
        return;
      }

      // Upload meta/global if any engines changed anything.
      let meta = this.service.recordManager.get(this.service.metaURL);
      if (meta.isNew || meta.changed) {
        this._log.info("meta/global changed locally: reuploading.");
        try {
          this.service.uploadMetaGlobal(meta);
          delete meta.isNew;
          delete meta.changed;
        } catch (error) {
          this._log.error("Unable to upload meta/global. Leaving marked as new.");
        }
      }

      // If there were no sync engine failures
      if (this.service.status.service != SYNC_FAILED_PARTIAL) {
        Svc.Prefs.set("lastSync", new Date().toString());
        this.service.status.sync = SYNC_SUCCEEDED;
      }
    } finally {
      Svc.Prefs.reset("firstSync");

      let syncTime = ((Date.now() - startTime) / 1000).toFixed(2);
      let dateStr = new Date().toLocaleFormat(LOG_DATE_FORMAT);
      this._log.info("Sync completed at " + dateStr
                     + " after " + syncTime + " secs.");
    }

    this.onComplete(null);
  },

  // Returns true if sync should proceed.
  // false / no return value means sync should be aborted.
  _syncEngine: function _syncEngine(engine) {
    try {
      engine.sync();
    }
    catch(e) {
      if (e.status == 401) {
        // Maybe a 401, cluster update perhaps needed?
        // We rely on ErrorHandler observing the sync failure notification to
        // schedule another sync and clear node assignment values.
        // Here we simply want to muffle the exception and return an
        // appropriate value.
        return false;
      }
    }

    return true;
  },

  _updateEnabledFromMeta: function (meta, numClients, engineManager=this.service.engineManager) {
    this._log.info("Updating enabled engines: " +
                    numClients + " clients.");

    if (meta.isNew || !meta.payload.engines) {
      this._log.debug("meta/global isn't new, or is missing engines. Not updating enabled state.");
      return;
    }

    // If we're the only client, and no engines are marked as enabled,
    // thumb our noses at the server data: it can't be right.
    // Belt-and-suspenders approach to Bug 615926.
    let hasEnabledEngines = false;
    for (let e in meta.payload.engines) {
      if (e != "clients") {
        hasEnabledEngines = true;
        break;
      }
    }

    if ((numClients <= 1) && !hasEnabledEngines) {
      this._log.info("One client and no enabled engines: not touching local engine status.");
      return;
    }

    this.service._ignorePrefObserver = true;

    let enabled = engineManager.enabledEngineNames;

    let toDecline = new Set();
    let toUndecline = new Set();

    for (let engineName in meta.payload.engines) {
      if (engineName == "clients") {
        // Clients is special.
        continue;
      }
      let index = enabled.indexOf(engineName);
      if (index != -1) {
        // The engine is enabled locally. Nothing to do.
        enabled.splice(index, 1);
        continue;
      }
      let engine = engineManager.get(engineName);
      if (!engine) {
        // The engine doesn't exist locally. Nothing to do.
        continue;
      }

      let attemptedEnable = false;
      // If the engine was enabled remotely, enable it locally.
      if (!Svc.Prefs.get("engineStatusChanged." + engine.prefName, false)) {
        this._log.trace("Engine " + engineName + " was enabled. Marking as non-declined.");
        toUndecline.add(engineName);
        this._log.trace(engineName + " engine was enabled remotely.");
        engine.enabled = true;
        // Note that setting engine.enabled to true might not have worked for
        // the password engine if a master-password is enabled.  However, it's
        // still OK that we added it to undeclined - the user *tried* to enable
        // it remotely - so it still winds up as not being flagged as declined
        // even though it's disabled remotely.
        attemptedEnable = true;
      }

      // If either the engine was disabled locally or enabling the engine
      // failed (see above re master-password) then wipe server data and
      // disable it everywhere.
      if (!engine.enabled) {
        this._log.trace("Wiping data for " + engineName + " engine.");
        engine.wipeServer();
        delete meta.payload.engines[engineName];
        meta.changed = true; // the new enabled state must propagate
        // We also here mark the engine as declined, because the pref
        // was explicitly changed to false - unless we tried, and failed,
        // to enable it - in which case we leave the declined state alone.
        if (!attemptedEnable) {
          // This will be reflected in meta/global in the next stage.
          this._log.trace("Engine " + engineName + " was disabled locally. Marking as declined.");
          toDecline.add(engineName);
        }
      }
    }

    // Any remaining engines were either enabled locally or disabled remotely.
    for (let engineName of enabled) {
      let engine = engineManager.get(engineName);
      if (Svc.Prefs.get("engineStatusChanged." + engine.prefName, false)) {
        this._log.trace("The " + engineName + " engine was enabled locally.");
        toUndecline.add(engineName);
      } else {
        this._log.trace("The " + engineName + " engine was disabled remotely.");

        // Don't automatically mark it as declined!
        engine.enabled = false;
      }
    }

    engineManager.decline(toDecline);
    engineManager.undecline(toUndecline);

    Svc.Prefs.resetBranch("engineStatusChanged.");
    this.service._ignorePrefObserver = false;
  },

  _updateEnabledEngines: function () {
    let meta = this.service.recordManager.get(this.service.metaURL);
    let numClients = this.service.scheduler.numClients;
    let engineManager = this.service.engineManager;

    this._updateEnabledFromMeta(meta, numClients, engineManager);
  },
};
Object.freeze(EngineSynchronizer.prototype);
