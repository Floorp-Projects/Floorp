/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file defines the add-on sync functionality.
 *
 * There are currently a number of known limitations:
 *  - We only sync XPI extensions and themes available from addons.mozilla.org.
 *    We hope to expand support for other add-ons eventually.
 *  - We only attempt syncing of add-ons between applications of the same type.
 *    This means add-ons will not synchronize between Firefox desktop and
 *    Firefox mobile, for example. This is because of significant add-on
 *    incompatibility between application types.
 *
 * Add-on records exist for each known {add-on, app-id} pair in the Sync client
 * set. Each record has a randomly chosen GUID. The records then contain
 * basic metadata about the add-on.
 *
 * We currently synchronize:
 *
 *  - Installations
 *  - Uninstallations
 *  - User enabling and disabling
 *
 * Synchronization is influenced by the following preferences:
 *
 *  - services.sync.addons.ignoreRepositoryChecking
 *  - services.sync.addons.ignoreUserEnabledChanges
 *  - services.sync.addons.trustedSourceHostnames
 *
 * See the documentation in services-sync.js for the behavior of these prefs.
 */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-sync/addonsreconciler.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/preferences.js");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/AddonRepository.jsm");

const EXPORTED_SYMBOLS = ["AddonsEngine"];

// 7 days in milliseconds.
const PRUNE_ADDON_CHANGES_THRESHOLD = 60 * 60 * 24 * 7 * 1000;

/**
 * AddonRecord represents the state of an add-on in an application.
 *
 * Each add-on has its own record for each application ID it is installed
 * on.
 *
 * The ID of add-on records is a randomly-generated GUID. It is random instead
 * of deterministic so the URIs of the records cannot be guessed and so
 * compromised server credentials won't result in disclosure of the specific
 * add-ons present in a Sync account.
 *
 * The record contains the following fields:
 *
 *  addonID
 *    ID of the add-on. This correlates to the "id" property on an Addon type.
 *
 *  applicationID
 *    The application ID this record is associated with.
 *
 *  enabled
 *    Boolean stating whether add-on is enabled or disabled by the user.
 *
 *  source
 *    String indicating where an add-on is from. Currently, we only support
 *    the value "amo" which indicates that the add-on came from the official
 *    add-ons repository, addons.mozilla.org. In the future, we may support
 *    installing add-ons from other sources. This provides a future-compatible
 *    mechanism for clients to only apply records they know how to handle.
 */
function AddonRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
AddonRecord.prototype = {
  __proto__: CryptoWrapper.prototype,
  _logName: "Record.Addon"
};

Utils.deferGetSet(AddonRecord, "cleartext", ["addonID",
                                             "applicationID",
                                             "enabled",
                                             "source"]);

/**
 * The AddonsEngine handles synchronization of add-ons between clients.
 *
 * The engine maintains an instance of an AddonsReconciler, which is the entity
 * maintaining state for add-ons. It provides the history and tracking APIs
 * that AddonManager doesn't.
 *
 * The engine instance overrides a handful of functions on the base class. The
 * rationale for each is documented by that function.
 */
function AddonsEngine() {
  SyncEngine.call(this, "Addons");

  this._reconciler = new AddonsReconciler();
}
AddonsEngine.prototype = {
  __proto__:              SyncEngine.prototype,
  _storeObj:              AddonsStore,
  _trackerObj:            AddonsTracker,
  _recordObj:             AddonRecord,
  version:                1,

  _reconciler:            null,

  /**
   * Override parent method to find add-ons by their public ID, not Sync GUID.
   */
  _findDupe: function _findDupe(item) {
    let id = item.addonID;

    // The reconciler should have been updated at the top of the sync, so we
    // can assume it is up to date when this function is called.
    let addons = this._reconciler.addons;
    if (!(id in addons)) {
      return null;
    }

    let addon = addons[id];
    if (addon.guid != item.id) {
      return addon.guid;
    }

    return null;
  },

  /**
   * Override getChangedIDs to pull in tracker changes plus changes from the
   * reconciler log.
   */
  getChangedIDs: function getChangedIDs() {
    let changes = {};
    for (let [id, modified] in Iterator(this._tracker.changedIDs)) {
      changes[id] = modified;
    }

    let lastSyncDate = new Date(this.lastSync * 1000);

    // The reconciler should have been refreshed at the beginning of a sync and
    // we assume this function is only called from within a sync.
    let reconcilerChanges = this._reconciler.getChangesSinceDate(lastSyncDate);
    let addons = this._reconciler.addons;
    for each (let change in reconcilerChanges) {
      let changeTime = change[0];
      let id = change[2];

      if (!(id in addons)) {
        continue;
      }

      // Keep newest modified time.
      if (id in changes && changeTime < changes[id]) {
          continue;
      }

      if (!this._store.isAddonSyncable(addons[id])) {
        continue;
      }

      this._log.debug("Adding changed add-on from changes log: " + id);
      let addon = addons[id];
      changes[addon.guid] = changeTime.getTime() / 1000;
    }

    return changes;
  },

  /**
   * Override start of sync function to refresh reconciler.
   *
   * Many functions in this class assume the reconciler is refreshed at the
   * top of a sync. If this ever changes, those functions should be revisited.
   *
   * Technically speaking, we don't need to refresh the reconciler on every
   * sync since it is installed as an AddonManager listener. However, add-ons
   * are complicated and we force a full refresh, just in case the listeners
   * missed something.
   */
  _syncStartup: function _syncStartup() {
    // We refresh state before calling parent because syncStartup in the parent
    // looks for changed IDs, which is dependent on add-on state being up to
    // date.
    this._refreshReconcilerState();

    SyncEngine.prototype._syncStartup.call(this);
  },

  /**
   * Override end of sync to perform a little housekeeping on the reconciler.
   *
   * We prune changes to prevent the reconciler state from growing without
   * bound. Even if it grows unbounded, there would have to be many add-on
   * changes (thousands) for it to slow things down significantly. This is
   * highly unlikely to occur. Still, we exercise defense just in case.
   */
  _syncCleanup: function _syncCleanup() {
    let ms = 1000 * this.lastSync - PRUNE_ADDON_CHANGES_THRESHOLD;
    this._reconciler.pruneChangesBeforeDate(new Date(ms));

    SyncEngine.prototype._syncCleanup.call(this);
  },

  /**
   * Helper function to ensure reconciler is up to date.
   *
   * This will synchronously load the reconciler's state from the file
   * system (if needed) and refresh the state of the reconciler.
   */
  _refreshReconcilerState: function _refreshReconcilerState() {
    this._log.debug("Refreshing reconciler state");
    let cb = Async.makeSpinningCallback();
    this._reconciler.refreshGlobalState(cb);
    cb.wait();
  }
};

/**
 * This is the primary interface between Sync and the Addons Manager.
 *
 * In addition to the core store APIs, we provide convenience functions to wrap
 * Add-on Manager APIs with Sync-specific semantics.
 */
function AddonsStore(name) {
  Store.call(this, name);
}
AddonsStore.prototype = {
  __proto__: Store.prototype,

  // Define the add-on types (.type) that we support.
  _syncableTypes: ["extension", "theme"],

  _extensionsPrefs: new Preferences("extensions."),

  get reconciler() {
    return this.engine._reconciler;
  },

  get engine() {
    // Ideally we'd link to a specific object, but the API doesn't provide an
    // easy way to faciliate this. When the async API lands, this hackiness can
    // go away.
    return Engines.get("addons");
  },

  /**
   * Override applyIncoming to filter out records we can't handle.
   */
  applyIncoming: function applyIncoming(record) {
    // The fields we look at aren't present when the record is deleted.
    if (!record.deleted) {
      // Ignore records not belonging to our application ID because that is the
      // current policy.
      if (record.applicationID != Services.appinfo.ID) {
        this._log.info("Ignoring incoming record from other App ID: " +
                        record.id);
        return;
      }

      // Ignore records that aren't from the official add-on repository, as that
      // is our current policy.
      if (record.source != "amo") {
        this._log.info("Ignoring unknown add-on source (" + record.source + ")" +
                       " for " + record.id);
        return;
      }
    }

    Store.prototype.applyIncoming.call(this, record);
  },


  /**
   * Provides core Store API to create/install an add-on from a record.
   */
  create: function create(record) {
    let cb = Async.makeSpinningCallback();
    this.installAddons([{
      id:       record.addonID,
      syncGUID: record.id,
      enabled:  record.enabled
    }], cb);

    // This will throw if there was an error. This will get caught by the sync
    // engine and the record will try to be applied later.
    let results = cb.wait();

    let addon;
    for each (let a in results.addons) {
      if (a.id == record.addonID) {
        addon = a;
        break;
      }
    }

    // This should never happen, but is present as a fail-safe.
    if (!addon) {
      throw new Error("Add-on not found after install: " + record.addonID);
    }

    this._log.info("Add-on installed: " + record.addonID);
  },

  /**
   * Provides core Store API to remove/uninstall an add-on from a record.
   */
  remove: function remove(record) {
    // If this is called, the payload is empty, so we have to find by GUID.
    let addon = this.getAddonByGUID(record.id);
    if (!addon) {
      // We don't throw because if the add-on could not be found then we assume
      // it has already been uninstalled and there is nothing for this function
      // to do.
      return;
    }

    this._log.info("Uninstalling add-on: " + addon.id);
    let cb = Async.makeSpinningCallback();
    this.uninstallAddon(addon, cb);
    cb.wait();
  },

  /**
   * Provides core Store API to update an add-on from a record.
   */
  update: function update(record) {
    let addon = this.getAddonByID(record.addonID);

    // This should only happen if there is a race condition between an add-on
    // being uninstalled locally and Sync calling this function after it
    // determines an add-on exists.
    if (!addon) {
      this._log.warn("Requested to update record but add-on not found: " +
                     record.addonID);
      return;
    }

    let cb = Async.makeSpinningCallback();
    this.updateUserDisabled(addon, !record.enabled, cb);
    cb.wait();
  },

  /**
   * Provide core Store API to determine if a record exists.
   */
  itemExists: function itemExists(guid) {
    let addon = this.reconciler.getAddonStateFromSyncGUID(guid);

    return !!addon;
  },

  /**
   * Create an add-on record from its GUID.
   *
   * @param guid
   *        Add-on GUID (from extensions DB)
   * @param collection
   *        Collection to add record to.
   *
   * @return AddonRecord instance
   */
  createRecord: function createRecord(guid, collection) {
    let record = new AddonRecord(collection, guid);
    record.applicationID = Services.appinfo.ID;

    let addon = this.reconciler.getAddonStateFromSyncGUID(guid);

    // If we don't know about this GUID or if it has been uninstalled, we mark
    // the record as deleted.
    if (!addon || !addon.installed) {
      record.deleted = true;
      return record;
    }

    record.modified = addon.modified.getTime() / 1000;

    record.addonID = addon.id;
    record.enabled = addon.enabled;

    // This needs to be dynamic when add-ons don't come from AddonRepository.
    record.source = "amo";

    return record;
  },

  /**
   * Changes the id of an add-on.
   *
   * This implements a core API of the store.
   */
  changeItemID: function changeItemID(oldID, newID) {
    // We always update the GUID in the reconciler because it will be
    // referenced later in the sync process.
    let state = this.reconciler.getAddonStateFromSyncGUID(oldID);
    if (state) {
      state.guid = newID;
      let cb = Async.makeSpinningCallback();
      this.reconciler.saveState(null, cb);
      cb.wait();
    }

    let addon = this.getAddonByGUID(oldID);
    if (!addon) {
      this._log.debug("Cannot change item ID (" + oldID + ") in Add-on " +
                      "Manager because old add-on not present: " + oldID);
      return;
    }

    addon.syncGUID = newID;
  },

  /**
   * Obtain the set of all syncable add-on Sync GUIDs.
   *
   * This implements a core Store API.
   */
  getAllIDs: function getAllIDs() {
    let ids = {};

    let addons = this.reconciler.addons;
    for each (let addon in addons) {
      if (this.isAddonSyncable(addon)) {
        ids[addon.guid] = true;
      }
    }

    return ids;
  },

  /**
   * Wipe engine data.
   *
   * This uninstalls all syncable addons from the application. In case of
   * error, it logs the error and keeps trying with other add-ons.
   */
  wipe: function wipe() {
    this._log.info("Processing wipe.");

    this.engine._refreshReconcilerState();

    // We only wipe syncable add-ons. Wipe is a Sync feature not a security
    // feature.
    for (let guid in this.getAllIDs()) {
      let addon = this.getAddonByGUID(guid);
      if (!addon) {
        this._log.debug("Ignoring add-on because it couldn't be obtained: " +
                        guid);
        continue;
      }

      this._log.info("Uninstalling add-on as part of wipe: " + addon.id);
      Utils.catch(addon.uninstall)();
    }
  },

  /***************************************************************************
   * Functions below are unique to this store and not part of the Store API  *
   ***************************************************************************/

  /**
   * Synchronously obtain an add-on from its public ID.
   *
   * @param id
   *        Add-on ID
   * @return Addon or undefined if not found
   */
  getAddonByID: function getAddonByID(id) {
    let cb = Async.makeSyncCallback();
    AddonManager.getAddonByID(id, cb);
    return Async.waitForSyncCallback(cb);
  },

  /**
   * Synchronously obtain an add-on from its Sync GUID.
   *
   * @param  guid
   *         Add-on Sync GUID
   * @return DBAddonInternal or null
   */
  getAddonByGUID: function getAddonByGUID(guid) {
    let cb = Async.makeSyncCallback();
    AddonManager.getAddonBySyncGUID(guid, cb);
    return Async.waitForSyncCallback(cb);
  },

  /**
   * Determines whether an add-on is suitable for Sync.
   *
   * @param  addon
   *         Addon instance
   * @return Boolean indicating whether it is appropriate for Sync
   */
  isAddonSyncable: function isAddonSyncable(addon) {
    // Currently, we limit syncable add-ons to those that are:
    //   1) In a well-defined set of types
    //   2) Installed in the current profile
    //   3) Not installed by a foreign entity (i.e. installed by the app)
    //      since they act like global extensions.
    //   4) Is not a hotfix.
    //   5) Are installed from AMO

    // We could represent the test as a complex boolean expression. We go the
    // verbose route so the failure reason is logged.
    if (!addon) {
      this._log.debug("Null object passed to isAddonSyncable.");
      return false;
    }

    if (this._syncableTypes.indexOf(addon.type) == -1) {
      this._log.debug(addon.id + " not syncable: type not in whitelist: " +
                      addon.type);
      return false;
    }

    if (!(addon.scope & AddonManager.SCOPE_PROFILE)) {
      this._log.debug(addon.id + " not syncable: not installed in profile.");
      return false;
    }

    // This may be too aggressive. If an add-on is downloaded from AMO and
    // manually placed in the profile directory, foreignInstall will be set.
    // Arguably, that add-on should be syncable.
    // TODO Address the edge case and come up with more robust heuristics.
    if (addon.foreignInstall) {
      this._log.debug(addon.id + " not syncable: is foreign install.");
      return false;
    }

    // Ignore hotfix extensions (bug 741670). The pref may not be defined.
    if (this._extensionsPrefs.get("hotfix.id", null) == addon.id) {
      this._log.debug(addon.id + " not syncable: is a hotfix.");
      return false;
    }

    // We provide a back door to skip the repository checking of an add-on.
    // This is utilized by the tests to make testing easier. Users could enable
    // this, but it would sacrifice security.
    if (Svc.Prefs.get("addons.ignoreRepositoryChecking", false)) {
      return true;
    }

    let cb = Async.makeSyncCallback();
    AddonRepository.getCachedAddonByID(addon.id, cb);
    let result = Async.waitForSyncCallback(cb);

    if (!result) {
      this._log.debug(addon.id + " not syncable: add-on not found in add-on " +
                      "repository.");
      return false;
    }

    return this.isSourceURITrusted(result.sourceURI);
  },

  /**
   * Determine whether an add-on's sourceURI field is trusted and the add-on
   * can be installed.
   *
   * This function should only ever be called from isAddonSyncable(). It is
   * exposed as a separate function to make testing easier.
   *
   * @param  uri
   *         nsIURI instance to validate
   * @return bool
   */
  isSourceURITrusted: function isSourceURITrusted(uri) {
    // For security reasons, we currently limit synced add-ons to those
    // installed from trusted hostname(s). We additionally require TLS with
    // the add-ons site to help prevent forgeries.
    let trustedHostnames = Svc.Prefs.get("addons.trustedSourceHostnames", "")
                           .split(",");

    if (!uri) {
      this._log.debug("Undefined argument to isSourceURITrusted().");
      return false;
    }

    // Scheme is validated before the hostname because uri.host may not be
    // populated for certain schemes. It appears to always be populated for
    // https, so we avoid the potential NS_ERROR_FAILURE on field access.
    if (uri.scheme != "https") {
      this._log.debug("Source URI not HTTPS: " + uri.spec);
      return false;
    }

    if (trustedHostnames.indexOf(uri.host) == -1) {
      this._log.debug("Source hostname not trusted: " + uri.host);
      return false;
    }

    return true;
  },

  /**
   * Obtain an AddonInstall object from an AddonSearchResult instance.
   *
   * The callback will be invoked with the result of the operation. The
   * callback receives 2 arguments, error and result. Error will be falsy
   * on success or some kind of error value otherwise. The result argument
   * will be an AddonInstall on success or null on failure. It is possible
   * for the error to be falsy but result to be null. This could happen if
   * an install was not found.
   *
   * @param addon
   *        AddonSearchResult to obtain install from.
   * @param cb
   *        Function to be called with result of operation.
   */
  getInstallFromSearchResult: function getInstallFromSearchResult(addon, cb) {
    // We should theoretically be able to obtain (and use) addon.install if
    // it is available. However, the addon.sourceURI rewriting won't be
    // reflected in the AddonInstall, so we can't use it. If we ever get rid
    // of sourceURI rewriting, we can avoid having to reconstruct the
    // AddonInstall.
    this._log.debug("Obtaining install for " + addon.id);

    // Verify that the source URI uses TLS. We don't allow installs from
    // insecure sources for security reasons. The Addon Manager ensures that
    // cert validation, etc is performed.
    if (!Svc.Prefs.get("addons.ignoreRepositoryChecking", false)) {
      let scheme = addon.sourceURI.scheme;
      if (scheme != "https") {
        cb(new Error("Insecure source URI scheme: " + scheme), addon.install);
      }
    }

    AddonManager.getInstallForURL(
      addon.sourceURI.spec,
      function handleInstall(install) {
        cb(null, install);
      },
      "application/x-xpinstall",
      undefined,
      addon.name,
      addon.iconURL,
      addon.version
    );
  },

  /**
   * Installs an add-on from an AddonSearchResult instance.
   *
   * The options argument defines extra options to control the install.
   * Recognized keys in this map are:
   *
   *   syncGUID - Sync GUID to use for the new add-on.
   *   enabled - Boolean indicating whether the add-on should be enabled upon
   *             install.
   *
   * When complete it calls a callback with 2 arguments, error and result.
   *
   * If error is falsy, result is an object. If error is truthy, result is
   * null.
   *
   * The result object has the following keys:
   *
   *   id      ID of add-on that was installed.
   *   install AddonInstall that was installed.
   *   addon   Addon that was installed.
   *
   * @param addon
   *        AddonSearchResult to install add-on from.
   * @param options
   *        Object with additional metadata describing how to install add-on.
   * @param cb
   *        Function to be invoked with result of operation.
   */
  installAddonFromSearchResult:
    function installAddonFromSearchResult(addon, options, cb) {
    this._log.info("Trying to install add-on from search result: " + addon.id);

    this.getInstallFromSearchResult(addon, function(error, install) {
      if (error) {
        cb(error, null);
        return;
      }

      if (!install) {
        cb(new Error("AddonInstall not available: " + addon.id), null);
        return;
      }

      try {
        this._log.info("Installing " + addon.id);
        let log = this._log;

        let listener = {
          onInstallStarted: function(install) {
            if (!options) {
              return;
            }

            if (options.syncGUID) {
              log.info("Setting syncGUID of " + install.name  +": " +
                       options.syncGUID);
              install.addon.syncGUID = options.syncGUID;
            }

            // We only need to change userDisabled if it is disabled because
            // enabled is the default.
            if ("enabled" in options && !options.enabled) {
              log.info("Marking add-on as disabled for install: " +
                       install.name);
              install.addon.userDisabled = true;
            }
          },
          onInstallEnded: function(install, addon) {
            install.removeListener(listener);

            cb(null, {id: addon.id, install: install, addon: addon});
          },
          onInstallFailed: function(install) {
            install.removeListener(listener);

            cb(new Error("Install failed: " + install.error), null);
          },
          onDownloadFailed: function(install) {
            install.removeListener(listener);

            cb(new Error("Download failed: " + install.error), null);
          }
        };
        install.addListener(listener);
        install.install();
      }
      catch (ex) {
        this._log.error("Error installing add-on: " + Utils.exceptionstr(ex));
        cb(ex, null);
      }
    }.bind(this));
  },

  /**
   * Uninstalls the Addon instance and invoke a callback when it is done.
   *
   * @param addon
   *        Addon instance to uninstall.
   * @param callback
   *        Function to be invoked when uninstall has finished. It receives a
   *        truthy value signifying error and the add-on which was uninstalled.
   */
  uninstallAddon: function uninstallAddon(addon, callback) {
    let listener = {
      onUninstalling: function(uninstalling, needsRestart) {
        if (addon.id != uninstalling.id) {
          return;
        }

        // We assume restartless add-ons will send the onUninstalled event
        // soon.
        if (!needsRestart) {
          return;
        }

        // For non-restartless add-ons, we issue the callback on uninstalling
        // because we will likely never see the uninstalled event.
        AddonManager.removeAddonListener(listener);
        callback(null, addon);
      },
      onUninstalled: function(uninstalled) {
        if (addon.id != uninstalled.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        callback(null, addon);
      }
    };
    AddonManager.addAddonListener(listener);
    addon.uninstall();
  },

  /**
   * Update the userDisabled flag on an add-on.
   *
   * This will enable or disable an add-on and call the supplied callback when
   * the action is complete. If no action is needed, the callback gets called
   * immediately.
   *
   * @param addon
   *        Addon instance to manipulate.
   * @param value
   *        Boolean to which to set userDisabled on the passed Addon.
   * @param callback
   *        Function to be called when action is complete. Will receive 2
   *        arguments, a truthy value that signifies error, and the Addon
   *        instance passed to this function.
   */
  updateUserDisabled: function updateUserDisabled(addon, value, callback) {
    if (addon.userDisabled == value) {
      callback(null, addon);
      return;
    }

    // A pref allows changes to the enabled flag to be ignored.
    if (Svc.Prefs.get("addons.ignoreUserEnabledChanges", false)) {
      this._log.info("Ignoring enabled state change due to preference: " +
                     addon.id);
      callback(null, addon);
      return;
    }

    let listener = {
      onEnabling: function onEnabling(wrapper, needsRestart) {
        this._log.debug("onEnabling: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        // We ignore the restartless case because we'll get onEnabled shortly.
        if (!needsRestart) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        callback(null, wrapper);
      }.bind(this),

      onEnabled: function onEnabled(wrapper) {
        this._log.debug("onEnabled: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        callback(null, wrapper);
      }.bind(this),

      onDisabling: function onDisabling(wrapper, needsRestart) {
        this._log.debug("onDisabling: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        if (!needsRestart) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        callback(null, wrapper);
      }.bind(this),

      onDisabled: function onDisabled(wrapper) {
        this._log.debug("onDisabled: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        callback(null, wrapper);
      }.bind(this),

      onOperationCancelled: function onOperationCancelled(wrapper) {
        this._log.debug("onOperationCancelled: " + wrapper.id);
        if (wrapper.id != addon.id) {
          return;
        }

        AddonManager.removeAddonListener(listener);
        callback(new Error("Operation cancelled"), wrapper);
      }.bind(this)
    };

    // The add-on listeners are only fired if the add-on is active. If not, the
    // change is silently updated and made active when/if the add-on is active.

    if (!addon.appDisabled) {
      AddonManager.addAddonListener(listener);
    }

    this._log.info("Updating userDisabled flag: " + addon.id + " -> " + value);
    addon.userDisabled = !!value;

    if (!addon.appDisabled) {
      callback(null, addon);
      return;
    }
    // Else the listener will handle invoking the callback.
  },

  /**
   * Installs multiple add-ons specified by metadata.
   *
   * The first argument is an array of objects. Each object must have the
   * following keys:
   *
   *   id - public ID of the add-on to install.
   *   syncGUID - syncGUID for new add-on.
   *   enabled - boolean indicating whether the add-on should be enabled.
   *
   * The callback will be called when activity on all add-ons is complete. The
   * callback receives 2 arguments, error and result.
   *
   * If error is truthy, it contains a string describing the overall error.
   *
   * The 2nd argument to the callback is always an object with details on the
   * overall execution state. It contains the following keys:
   *
   *   installedIDs  Array of add-on IDs that were installed.
   *   installs      Array of AddonInstall instances that were installed.
   *   addons        Array of Addon instances that were installed.
   *   errors        Array of errors encountered. Only has elements if error is
   *                 truthy.
   *
   * @param installs
   *        Array of objects describing add-ons to install.
   * @param cb
   *        Function to be called when all actions are complete.
   */
  installAddons: function installAddons(installs, cb) {
    if (!cb) {
      throw new Error("Invalid argument: cb is not defined.");
    }

    let ids = [];
    for each (let addon in installs) {
      ids.push(addon.id);
    }

    AddonRepository.getAddonsByIDs(ids, {
      searchSucceeded: function searchSucceeded(addons, addonsLength, total) {
        this._log.info("Found " + addonsLength + "/" + ids.length +
                       " add-ons during repository search.");

        let ourResult = {
          installedIDs: [],
          installs:     [],
          addons:       [],
          errors:       []
        };

        if (!addonsLength) {
          cb(null, ourResult);
          return;
        }

        let expectedInstallCount = 0;
        let finishedCount = 0;
        let installCallback = function installCallback(error, result) {
          finishedCount++;

          if (error) {
            ourResult.errors.push(error);
          } else {
            ourResult.installedIDs.push(result.id);
            ourResult.installs.push(result.install);
            ourResult.addons.push(result.addon);
          }

          if (finishedCount >= expectedInstallCount) {
            if (ourResult.errors.length > 0) {
              cb(new Error("1 or more add-ons failed to install"), ourResult);
            } else {
              cb(null, ourResult);
            }
          }
        }.bind(this);

        let toInstall = [];

        // Rewrite the "src" query string parameter of the source URI to note
        // that the add-on was installed by Sync and not something else so
        // server-side metrics aren't skewed (bug 708134). The server should
        // ideally send proper URLs, but this solution was deemed too
        // complicated at the time the functionality was implemented.
        for each (let addon in addons) {
          // sourceURI presence isn't enforced by AddonRepository. So, we skip
          // add-ons without a sourceURI.
          if (!addon.sourceURI) {
            this._log.info("Skipping install of add-on because missing " +
                           "sourceURI: " + addon.id);
            continue;
          }

          toInstall.push(addon);

          // We should always be able to QI the nsIURI to nsIURL. If not, we
          // still try to install the add-on, but we don't rewrite the URL,
          // potentially skewing metrics.
          try {
            addon.sourceURI.QueryInterface(Ci.nsIURL);
          } catch (ex) {
            this._log.warn("Unable to QI sourceURI to nsIURL: " +
                           addon.sourceURI.spec);
            continue;
          }

          let params = addon.sourceURI.query.split("&").map(
            function rewrite(param) {

            if (param.indexOf("src=") == 0) {
              return "src=sync";
            } else {
              return param;
            }
          });

          addon.sourceURI.query = params.join("&");
        }

        expectedInstallCount = toInstall.length;

        if (!expectedInstallCount) {
          cb(null, ourResult);
          return;
        }

        // Start all the installs asynchronously. They will report back to us
        // as they finish, eventually triggering the global callback.
        for each (let addon in toInstall) {
          let options = {};
          for each (let install in installs) {
            if (install.id == addon.id) {
              options = install;
              break;
            }
          }

          this.installAddonFromSearchResult(addon, options, installCallback);
        }

      }.bind(this),

      searchFailed: function searchFailed() {
        cb(new Error("AddonRepository search failed"), null);
      }.bind(this)
    });
  }
};

/**
 * The add-ons tracker keeps track of real-time changes to add-ons.
 *
 * It hooks up to the reconciler and receives notifications directly from it.
 */
function AddonsTracker(name) {
  Tracker.call(this, name);

  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
AddonsTracker.prototype = {
  __proto__: Tracker.prototype,

  get reconciler() {
    return Engines.get("addons")._reconciler;
  },

  get store() {
    return Engines.get("addons")._store;
  },

  /**
   * This callback is executed whenever the AddonsReconciler sends out a change
   * notification. See AddonsReconciler.addChangeListener().
   */
  changeListener: function changeHandler(date, change, addon) {
    this._log.debug("changeListener invoked: " + change + " " + addon.id);
    // Ignore changes that occur during sync.
    if (this.ignoreAll) {
      return;
    }

    if (!this.store.isAddonSyncable(addon)) {
      this._log.debug("Ignoring change because add-on isn't syncable: " +
                      addon.id);
      return;
    }

    this.addChangedID(addon.guid, date.getTime() / 1000);
    this.score += SCORE_INCREMENT_XLARGE;
  },

  observe: function(subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        this.reconciler.addChangeListener(this);
        break;

      case "weave:engine:stop-tracking":
        this.reconciler.removeChangeListener(this);
        break;
    }
  }
};
