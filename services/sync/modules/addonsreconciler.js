/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains middleware to reconcile state of AddonManager for
 * purposes of tracking events for Sync. The content in this file exists
 * because AddonManager does not have a getChangesSinceX() API and adding
 * that functionality properly was deemed too time-consuming at the time
 * add-on sync was originally written. If/when AddonManager adds this API,
 * this file can go away and the add-ons engine can be rewritten to use it.
 *
 * It was decided to have this tracking functionality exist in a separate
 * standalone file so it could be more easily understood, tested, and
 * hopefully ported.
 */

"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/AddonManager.jsm");

const DEFAULT_STATE_FILE = "addonsreconciler";

this.CHANGE_INSTALLED   = 1;
this.CHANGE_UNINSTALLED = 2;
this.CHANGE_ENABLED     = 3;
this.CHANGE_DISABLED    = 4;

this.EXPORTED_SYMBOLS = ["AddonsReconciler", "CHANGE_INSTALLED",
                         "CHANGE_UNINSTALLED", "CHANGE_ENABLED",
                         "CHANGE_DISABLED"];
/**
 * Maintains state of add-ons.
 *
 * State is maintained in 2 data structures, an object mapping add-on IDs
 * to metadata and an array of changes over time. The object mapping can be
 * thought of as a minimal copy of data from AddonManager which is needed for
 * Sync. The array is effectively a log of changes over time.
 *
 * The data structures are persisted to disk by serializing to a JSON file in
 * the current profile. The data structures are updated by 2 mechanisms. First,
 * they can be refreshed from the global state of the AddonManager. This is a
 * sure-fire way of ensuring the reconciler is up to date. Second, the
 * reconciler adds itself as an AddonManager listener. When it receives change
 * notifications, it updates its internal state incrementally.
 *
 * The internal state is persisted to a JSON file in the profile directory.
 *
 * An instance of this is bound to an AddonsEngine instance. In reality, it
 * likely exists as a singleton. To AddonsEngine, it functions as a store and
 * an entity which emits events for tracking.
 *
 * The usage pattern for instances of this class is:
 *
 *   let reconciler = new AddonsReconciler();
 *   reconciler.loadState(null, function(error) { ... });
 *
 *   // At this point, your instance should be ready to use.
 *
 * When you are finished with the instance, please call:
 *
 *   reconciler.stopListening();
 *   reconciler.saveState(...);
 *
 * There are 2 classes of listeners in the AddonManager: AddonListener and
 * InstallListener. This class is a listener for both (member functions just
 * get called directly).
 *
 * When an add-on is installed, listeners are called in the following order:
 *
 *  IL.onInstallStarted, AL.onInstalling, IL.onInstallEnded, AL.onInstalled
 *
 * For non-restartless add-ons, an application restart may occur between
 * IL.onInstallEnded and AL.onInstalled. Unfortunately, Sync likely will
 * not be loaded when AL.onInstalled is fired shortly after application
 * start, so it won't see this event. Therefore, for add-ons requiring a
 * restart, Sync treats the IL.onInstallEnded event as good enough to
 * indicate an install. For restartless add-ons, Sync assumes AL.onInstalled
 * will follow shortly after IL.onInstallEnded and thus it ignores
 * IL.onInstallEnded.
 *
 * The listeners can also see events related to the download of the add-on.
 * This class isn't interested in those. However, there are failure events,
 * IL.onDownloadFailed and IL.onDownloadCanceled which get called if a
 * download doesn't complete successfully.
 *
 * For uninstalls, we see AL.onUninstalling then AL.onUninstalled. Like
 * installs, the events could be separated by an application restart and Sync
 * may not see the onUninstalled event. Again, if we require a restart, we
 * react to onUninstalling. If not, we assume we'll get onUninstalled.
 *
 * Enabling and disabling work by sending:
 *
 *   AL.onEnabling, AL.onEnabled
 *   AL.onDisabling, AL.onDisabled
 *
 * Again, they may be separated by a restart, so we heed the requiresRestart
 * flag.
 *
 * Actions can be undone. All undoable actions notify the same
 * AL.onOperationCancelled event. We treat this event like any other.
 *
 * Restartless add-ons have interesting behavior during uninstall. These
 * add-ons are first disabled then they are actually uninstalled. So, we will
 * see AL.onDisabling and AL.onDisabled. The onUninstalling and onUninstalled
 * events only come after the Addon Manager is closed or another view is
 * switched to. In the case of Sync performing the uninstall, the uninstall
 * events will occur immediately. However, we still see disabling events and
 * heed them like they were normal. In the end, the state is proper.
 */
this.AddonsReconciler = function AddonsReconciler() {
  this._log = Log.repository.getLogger("Sync.AddonsReconciler");
  let level = Svc.Prefs.get("log.logger.addonsreconciler", "Debug");
  this._log.level = Log.Level[level];

  Svc.Obs.add("xpcom-shutdown", this.stopListening, this);
};
AddonsReconciler.prototype = {
  /** Flag indicating whether we are listening to AddonManager events. */
  _listening: false,

  /**
   * Whether state has been loaded from a file.
   *
   * State is loaded on demand if an operation requires it.
   */
  _stateLoaded: false,

  /**
   * Define this as false if the reconciler should not persist state
   * to disk when handling events.
   *
   * This allows test code to avoid spinning to write during observer
   * notifications and xpcom shutdown, which appears to cause hangs on WinXP
   * (Bug 873861).
   */
  _shouldPersist: true,

  /** Log logger instance */
  _log: null,

  /**
   * Container for add-on metadata.
   *
   * Keys are add-on IDs. Values are objects which describe the state of the
   * add-on. This is a minimal mirror of data that can be queried from
   * AddonManager. In some cases, we retain data longer than AddonManager.
   */
  _addons: {},

  /**
   * List of add-on changes over time.
   *
   * Each element is an array of [time, change, id].
   */
  _changes: [],

  /**
   * Objects subscribed to changes made to this instance.
   */
  _listeners: [],

  /**
   * Accessor for add-ons in this object.
   *
   * Returns an object mapping add-on IDs to objects containing metadata.
   */
  get addons() {
    this._ensureStateLoaded();
    return this._addons;
  },

  /**
   * Load reconciler state from a file.
   *
   * The path is relative to the weave directory in the profile. If no
   * path is given, the default one is used.
   *
   * If the file does not exist or there was an error parsing the file, the
   * state will be transparently defined as empty.
   *
   * @param path
   *        Path to load. ".json" is appended automatically. If not defined,
   *        a default path will be consulted.
   * @param callback
   *        Callback to be executed upon file load. The callback receives a
   *        truthy error argument signifying whether an error occurred and a
   *        boolean indicating whether data was loaded.
   */
  loadState: function loadState(path, callback) {
    let file = path || DEFAULT_STATE_FILE;
    Utils.jsonLoad(file, this, function(json) {
      this._addons = {};
      this._changes = [];

      if (!json) {
        this._log.debug("No data seen in loaded file: " + file);
        if (callback) {
          callback(null, false);
        }

        return;
      }

      let version = json.version;
      if (!version || version != 1) {
        this._log.error("Could not load JSON file because version not " +
                        "supported: " + version);
        if (callback) {
          callback(null, false);
        }

        return;
      }

      this._addons = json.addons;
      for (let id in this._addons) {
        let record = this._addons[id];
        record.modified = new Date(record.modified);
      }

      for (let [time, change, id] of json.changes) {
        this._changes.push([new Date(time), change, id]);
      }

      if (callback) {
        callback(null, true);
      }
    });
  },

  /**
   * Saves the current state to a file in the local profile.
   *
   * @param  path
   *         String path in profile to save to. If not defined, the default
   *         will be used.
   * @param  callback
   *         Function to be invoked on save completion. No parameters will be
   *         passed to callback.
   */
  saveState: function saveState(path, callback) {
    let file = path || DEFAULT_STATE_FILE;
    let state = {version: 1, addons: {}, changes: []};

    for (let [id, record] in Iterator(this._addons)) {
      state.addons[id] = {};
      for (let [k, v] in Iterator(record)) {
        if (k == "modified") {
          state.addons[id][k] = v.getTime();
        }
        else {
          state.addons[id][k] = v;
        }
      }
    }

    for (let [time, change, id] of this._changes) {
      state.changes.push([time.getTime(), change, id]);
    }

    this._log.info("Saving reconciler state to file: " + file);
    Utils.jsonSave(file, this, state, callback);
  },

  /**
   * Registers a change listener with this instance.
   *
   * Change listeners are called every time a change is recorded. The listener
   * is an object with the function "changeListener" that takes 3 arguments,
   * the Date at which the change happened, the type of change (a CHANGE_*
   * constant), and the add-on state object reflecting the current state of
   * the add-on at the time of the change.
   *
   * @param listener
   *        Object containing changeListener function.
   */
  addChangeListener: function addChangeListener(listener) {
    if (this._listeners.indexOf(listener) == -1) {
      this._log.debug("Adding change listener.");
      this._listeners.push(listener);
    }
  },

  /**
   * Removes a previously-installed change listener from the instance.
   *
   * @param listener
   *        Listener instance to remove.
   */
  removeChangeListener: function removeChangeListener(listener) {
    this._listeners = this._listeners.filter(function(element) {
      if (element == listener) {
        this._log.debug("Removing change listener.");
        return false;
      } else {
        return true;
      }
    }.bind(this));
  },

  /**
   * Tells the instance to start listening for AddonManager changes.
   *
   * This is typically called automatically when Sync is loaded.
   */
  startListening: function startListening() {
    if (this._listening) {
      return;
    }

    this._log.info("Registering as Add-on Manager listener.");
    AddonManager.addAddonListener(this);
    AddonManager.addInstallListener(this);
    this._listening = true;
  },

  /**
   * Tells the instance to stop listening for AddonManager changes.
   *
   * The reconciler should always be listening. This should only be called when
   * the instance is being destroyed.
   *
   * This function will get called automatically on XPCOM shutdown. However, it
   * is a best practice to call it yourself.
   */
  stopListening: function stopListening() {
    if (!this._listening) {
      return;
    }

    this._log.debug("Stopping listening and removing AddonManager listeners.");
    AddonManager.removeInstallListener(this);
    AddonManager.removeAddonListener(this);
    this._listening = false;
  },

  /**
   * Refreshes the global state of add-ons by querying the AddonManager.
   */
  refreshGlobalState: function refreshGlobalState(callback) {
    this._log.info("Refreshing global state from AddonManager.");
    this._ensureStateLoaded();

    let installs;

    AddonManager.getAllAddons(function (addons) {
      let ids = {};

      for (let addon of addons) {
        ids[addon.id] = true;
        this.rectifyStateFromAddon(addon);
      }

      // Look for locally-defined add-ons that no longer exist and update their
      // record.
      for (let [id, addon] in Iterator(this._addons)) {
        if (id in ids) {
          continue;
        }

        // If the id isn't in ids, it means that the add-on has been deleted or
        // the add-on is in the process of being installed. We detect the
        // latter by seeing if an AddonInstall is found for this add-on.

        if (!installs) {
          let cb = Async.makeSyncCallback();
          AddonManager.getAllInstalls(cb);
          installs = Async.waitForSyncCallback(cb);
        }

        let installFound = false;
        for (let install of installs) {
          if (install.addon && install.addon.id == id &&
              install.state == AddonManager.STATE_INSTALLED) {

            installFound = true;
            break;
          }
        }

        if (installFound) {
          continue;
        }

        if (addon.installed) {
          addon.installed = false;
          this._log.debug("Adding change because add-on not present in " +
                          "Add-on Manager: " + id);
          this._addChange(new Date(), CHANGE_UNINSTALLED, addon);
        }
      }

      // See note for _shouldPersist.
      if (this._shouldPersist) {
        this.saveState(null, callback);
      } else {
        callback();
      }
    }.bind(this));
  },

  /**
   * Rectifies the state of an add-on from an Addon instance.
   *
   * This basically says "given an Addon instance, assume it is truth and
   * apply changes to the local state to reflect it."
   *
   * This function could result in change listeners being called if the local
   * state differs from the passed add-on's state.
   *
   * @param addon
   *        Addon instance being updated.
   */
  rectifyStateFromAddon: function rectifyStateFromAddon(addon) {
    this._log.debug(`Rectifying state for addon ${addon.name} (version=${addon.version}, id=${addon.id})`);
    this._ensureStateLoaded();

    let id = addon.id;
    let enabled = !addon.userDisabled;
    let guid = addon.syncGUID;
    let now = new Date();

    if (!(id in this._addons)) {
      let record = {
        id: id,
        guid: guid,
        enabled: enabled,
        installed: true,
        modified: now,
        type: addon.type,
        scope: addon.scope,
        foreignInstall: addon.foreignInstall,
        isSyncable: addon.isSyncable,
      };
      this._addons[id] = record;
      this._log.debug("Adding change because add-on not present locally: " +
                      id);
      this._addChange(now, CHANGE_INSTALLED, record);
      return;
    }

    let record = this._addons[id];

    if (!record.installed) {
      // It is possible the record is marked as uninstalled because an
      // uninstall is pending.
      if (!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL)) {
        record.installed = true;
        record.modified = now;
      }
    }

    if (record.enabled != enabled) {
      record.enabled = enabled;
      record.modified = now;
      let change = enabled ? CHANGE_ENABLED : CHANGE_DISABLED;
      this._log.debug("Adding change because enabled state changed: " + id);
      this._addChange(new Date(), change, record);
    }

    if (record.guid != guid) {
      record.guid = guid;
      // We don't record a change because the Sync engine rectifies this on its
      // own. This is tightly coupled with Sync. If this code is ever lifted
      // outside of Sync, this exception should likely be removed.
    }
  },

  /**
   * Record a change in add-on state.
   *
   * @param date
   *        Date at which the change occurred.
   * @param change
   *        The type of the change. A CHANGE_* constant.
   * @param state
   *        The new state of the add-on. From this.addons.
   */
  _addChange: function _addChange(date, change, state) {
    this._log.info("Change recorded for " + state.id);
    this._changes.push([date, change, state.id]);

    for (let listener of this._listeners) {
      try {
        listener.changeListener.call(listener, date, change, state);
      } catch (ex) {
        this._log.warn("Exception calling change listener", ex);
      }
    }
  },

  /**
   * Obtain the set of changes to add-ons since the date passed.
   *
   * This will return an array of arrays. Each entry in the array has the
   * elements [date, change_type, id], where
   *
   *   date - Date instance representing when the change occurred.
   *   change_type - One of CHANGE_* constants.
   *   id - ID of add-on that changed.
   */
  getChangesSinceDate: function getChangesSinceDate(date) {
    this._ensureStateLoaded();

    let length = this._changes.length;
    for (let i = 0; i < length; i++) {
      if (this._changes[i][0] >= date) {
        return this._changes.slice(i);
      }
    }

    return [];
  },

  /**
   * Prunes all recorded changes from before the specified Date.
   *
   * @param date
   *        Entries older than this Date will be removed.
   */
  pruneChangesBeforeDate: function pruneChangesBeforeDate(date) {
    this._ensureStateLoaded();

    this._changes = this._changes.filter(function test_age(change) {
      return change[0] >= date;
    });
  },

  /**
   * Obtains the set of all known Sync GUIDs for add-ons.
   *
   * @return Object with guids as keys and values of true.
   */
  getAllSyncGUIDs: function getAllSyncGUIDs() {
    let result = {};
    for (let id in this.addons) {
      result[id] = true;
    }

    return result;
  },

  /**
   * Obtain the add-on state record for an add-on by Sync GUID.
   *
   * If the add-on could not be found, returns null.
   *
   * @param  guid
   *         Sync GUID of add-on to retrieve.
   * @return Object on success on null on failure.
   */
  getAddonStateFromSyncGUID: function getAddonStateFromSyncGUID(guid) {
    for (let id in this.addons) {
      let addon = this.addons[id];
      if (addon.guid == guid) {
        return addon;
      }
    }

    return null;
  },

  /**
   * Ensures that state is loaded before continuing.
   *
   * This is called internally by anything that accesses the internal data
   * structures. It effectively just-in-time loads serialized state.
   */
  _ensureStateLoaded: function _ensureStateLoaded() {
    if (this._stateLoaded) {
      return;
    }

    let cb = Async.makeSpinningCallback();
    this.loadState(null, cb);
    cb.wait();
    this._stateLoaded = true;
  },

  /**
   * Handler that is invoked as part of the AddonManager listeners.
   */
  _handleListener: function _handlerListener(action, addon, requiresRestart) {
    // Since this is called as an observer, we explicitly trap errors and
    // log them to ourselves so we don't see errors reported elsewhere.
    try {
      let id = addon.id;
      this._log.debug("Add-on change: " + action + " to " + id);

      // We assume that every event for non-restartless add-ons is
      // followed by another event and that this follow-up event is the most
      // appropriate to react to. Currently we ignore onEnabling, onDisabling,
      // and onUninstalling for non-restartless add-ons.
      if (requiresRestart === false) {
        this._log.debug("Ignoring " + action + " for restartless add-on.");
        return;
      }

      switch (action) {
        case "onEnabling":
        case "onEnabled":
        case "onDisabling":
        case "onDisabled":
        case "onInstalled":
        case "onInstallEnded":
        case "onOperationCancelled":
          this.rectifyStateFromAddon(addon);
          break;

        case "onUninstalling":
        case "onUninstalled":
          let id = addon.id;
          let addons = this.addons;
          if (id in addons) {
            let now = new Date();
            let record = addons[id];
            record.installed = false;
            record.modified = now;
            this._log.debug("Adding change because of uninstall listener: " +
                            id);
            this._addChange(now, CHANGE_UNINSTALLED, record);
          }
      }

      // See note for _shouldPersist.
      if (this._shouldPersist) {
        let cb = Async.makeSpinningCallback();
        this.saveState(null, cb);
        cb.wait();
      }
    }
    catch (ex) {
      this._log.warn("Exception", ex);
    }
  },

  // AddonListeners
  onEnabling: function onEnabling(addon, requiresRestart) {
    this._handleListener("onEnabling", addon, requiresRestart);
  },
  onEnabled: function onEnabled(addon) {
    this._handleListener("onEnabled", addon);
  },
  onDisabling: function onDisabling(addon, requiresRestart) {
    this._handleListener("onDisabling", addon, requiresRestart);
  },
  onDisabled: function onDisabled(addon) {
    this._handleListener("onDisabled", addon);
  },
  onInstalling: function onInstalling(addon, requiresRestart) {
    this._handleListener("onInstalling", addon, requiresRestart);
  },
  onInstalled: function onInstalled(addon) {
    this._handleListener("onInstalled", addon);
  },
  onUninstalling: function onUninstalling(addon, requiresRestart) {
    this._handleListener("onUninstalling", addon, requiresRestart);
  },
  onUninstalled: function onUninstalled(addon) {
    this._handleListener("onUninstalled", addon);
  },
  onOperationCancelled: function onOperationCancelled(addon) {
    this._handleListener("onOperationCancelled", addon);
  },

  // InstallListeners
  onInstallEnded: function onInstallEnded(install, addon) {
    this._handleListener("onInstallEnded", addon);
  }
};
