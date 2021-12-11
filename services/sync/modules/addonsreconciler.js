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

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Svc, Utils } = ChromeUtils.import("resource://services-sync/util.js");
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const DEFAULT_STATE_FILE = "addonsreconciler";

var CHANGE_INSTALLED = 1;
var CHANGE_UNINSTALLED = 2;
var CHANGE_ENABLED = 3;
var CHANGE_DISABLED = 4;

var EXPORTED_SYMBOLS = [
  "AddonsReconciler",
  "CHANGE_INSTALLED",
  "CHANGE_UNINSTALLED",
  "CHANGE_ENABLED",
  "CHANGE_DISABLED",
];
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
 *   let reconciler = new AddonsReconciler(...);
 *   await reconciler.ensureStateLoaded();
 *
 *   // At this point, your instance should be ready to use.
 *
 * When you are finished with the instance, please call:
 *
 *   reconciler.stopListening();
 *   await reconciler.saveState(...);
 *
 * This class uses the AddonManager AddonListener interface.
 * When an add-on is installed, listeners are called in the following order:
 *  AL.onInstalling, AL.onInstalled
 *
 * For uninstalls, we see AL.onUninstalling then AL.onUninstalled.
 *
 * Enabling and disabling work by sending:
 *
 *   AL.onEnabling, AL.onEnabled
 *   AL.onDisabling, AL.onDisabled
 *
 * Actions can be undone. All undoable actions notify the same
 * AL.onOperationCancelled event. We treat this event like any other.
 *
 * When an add-on is uninstalled from about:addons, the user is offered an
 * "Undo" option, which leads to the following sequence of events as
 * observed by an AddonListener:
 * Add-ons are first disabled then they are actually uninstalled. So, we will
 * see AL.onDisabling and AL.onDisabled. The onUninstalling and onUninstalled
 * events only come after the Addon Manager is closed or another view is
 * switched to. In the case of Sync performing the uninstall, the uninstall
 * events will occur immediately. However, we still see disabling events and
 * heed them like they were normal. In the end, the state is proper.
 */
function AddonsReconciler(queueCaller) {
  this._log = Log.repository.getLogger("Sync.AddonsReconciler");
  this._log.manageLevelFromPref("services.sync.log.logger.addonsreconciler");
  this.queueCaller = queueCaller;

  Svc.Obs.add("xpcom-shutdown", this.stopListening, this);
}
AddonsReconciler.prototype = {
  /** Flag indicating whether we are listening to AddonManager events. */
  _listening: false,

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
    return this._addons;
  },

  async ensureStateLoaded() {
    if (!this._promiseStateLoaded) {
      this._promiseStateLoaded = this.loadState();
    }
    return this._promiseStateLoaded;
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
   * @param file
   *        Path to load. ".json" is appended automatically. If not defined,
   *        a default path will be consulted.
   */
  async loadState(file = DEFAULT_STATE_FILE) {
    let json = await Utils.jsonLoad(file, this);
    this._addons = {};
    this._changes = [];

    if (!json) {
      this._log.debug("No data seen in loaded file: " + file);
      return false;
    }

    let version = json.version;
    if (!version || version != 1) {
      this._log.error(
        "Could not load JSON file because version not " +
          "supported: " +
          version
      );
      return false;
    }

    this._addons = json.addons;
    for (let id in this._addons) {
      let record = this._addons[id];
      record.modified = new Date(record.modified);
    }

    for (let [time, change, id] of json.changes) {
      this._changes.push([new Date(time), change, id]);
    }

    return true;
  },

  /**
   * Saves the current state to a file in the local profile.
   *
   * @param  file
   *         String path in profile to save to. If not defined, the default
   *         will be used.
   */
  async saveState(file = DEFAULT_STATE_FILE) {
    let state = { version: 1, addons: {}, changes: [] };

    for (let [id, record] of Object.entries(this._addons)) {
      state.addons[id] = {};
      for (let [k, v] of Object.entries(record)) {
        if (k == "modified") {
          state.addons[id][k] = v.getTime();
        } else {
          state.addons[id][k] = v;
        }
      }
    }

    for (let [time, change, id] of this._changes) {
      state.changes.push([time.getTime(), change, id]);
    }

    this._log.info("Saving reconciler state to file: " + file);
    await Utils.jsonSave(file, this, state);
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
    if (!this._listeners.includes(listener)) {
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
    this._listeners = this._listeners.filter(element => {
      if (element == listener) {
        this._log.debug("Removing change listener.");
        return false;
      }
      return true;
    });
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

    this._log.debug("Stopping listening and removing AddonManager listener.");
    AddonManager.removeAddonListener(this);
    this._listening = false;
  },

  /**
   * Refreshes the global state of add-ons by querying the AddonManager.
   */
  async refreshGlobalState() {
    this._log.info("Refreshing global state from AddonManager.");

    let installs;
    let addons = await AddonManager.getAllAddons();

    let ids = {};

    for (let addon of addons) {
      ids[addon.id] = true;
      await this.rectifyStateFromAddon(addon);
    }

    // Look for locally-defined add-ons that no longer exist and update their
    // record.
    for (let [id, addon] of Object.entries(this._addons)) {
      if (id in ids) {
        continue;
      }

      // If the id isn't in ids, it means that the add-on has been deleted or
      // the add-on is in the process of being installed. We detect the
      // latter by seeing if an AddonInstall is found for this add-on.

      if (!installs) {
        installs = await AddonManager.getAllInstalls();
      }

      let installFound = false;
      for (let install of installs) {
        if (
          install.addon &&
          install.addon.id == id &&
          install.state == AddonManager.STATE_INSTALLED
        ) {
          installFound = true;
          break;
        }
      }

      if (installFound) {
        continue;
      }

      if (addon.installed) {
        addon.installed = false;
        this._log.debug(
          "Adding change because add-on not present in " +
            "Add-on Manager: " +
            id
        );
        await this._addChange(new Date(), CHANGE_UNINSTALLED, addon);
      }
    }

    // See note for _shouldPersist.
    if (this._shouldPersist) {
      await this.saveState();
    }
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
  async rectifyStateFromAddon(addon) {
    this._log.debug(
      `Rectifying state for addon ${addon.name} (version=${addon.version}, id=${addon.id})`
    );

    let id = addon.id;
    let enabled = !addon.userDisabled;
    let guid = addon.syncGUID;
    let now = new Date();

    if (!(id in this._addons)) {
      let record = {
        id,
        guid,
        enabled,
        installed: true,
        modified: now,
        type: addon.type,
        scope: addon.scope,
        foreignInstall: addon.foreignInstall,
        isSyncable: addon.isSyncable,
      };
      this._addons[id] = record;
      this._log.debug(
        "Adding change because add-on not present locally: " + id
      );
      await this._addChange(now, CHANGE_INSTALLED, record);
      return;
    }

    let record = this._addons[id];
    record.isSyncable = addon.isSyncable;

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
      await this._addChange(new Date(), change, record);
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
  async _addChange(date, change, state) {
    this._log.info("Change recorded for " + state.id);
    this._changes.push([date, change, state.id]);

    for (let listener of this._listeners) {
      try {
        await listener.changeListener(date, change, state);
      } catch (ex) {
        this._log.error("Exception calling change listener", ex);
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
  getChangesSinceDate(date) {
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
  pruneChangesBeforeDate(date) {
    this._changes = this._changes.filter(function test_age(change) {
      return change[0] >= date;
    });
  },

  /**
   * Obtains the set of all known Sync GUIDs for add-ons.
   */
  getAllSyncGUIDs() {
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
   */
  getAddonStateFromSyncGUID(guid) {
    for (let id in this.addons) {
      let addon = this.addons[id];
      if (addon.guid == guid) {
        return addon;
      }
    }

    return null;
  },

  /**
   * Handler that is invoked as part of the AddonManager listeners.
   */
  async _handleListener(action, addon) {
    // Since this is called as an observer, we explicitly trap errors and
    // log them to ourselves so we don't see errors reported elsewhere.
    try {
      let id = addon.id;
      this._log.debug("Add-on change: " + action + " to " + id);

      switch (action) {
        case "onEnabled":
        case "onDisabled":
        case "onInstalled":
        case "onInstallEnded":
        case "onOperationCancelled":
          await this.rectifyStateFromAddon(addon);
          break;

        case "onUninstalled":
          let id = addon.id;
          let addons = this.addons;
          if (id in addons) {
            let now = new Date();
            let record = addons[id];
            record.installed = false;
            record.modified = now;
            this._log.debug(
              "Adding change because of uninstall listener: " + id
            );
            await this._addChange(now, CHANGE_UNINSTALLED, record);
          }
      }

      // See note for _shouldPersist.
      if (this._shouldPersist) {
        await this.saveState();
      }
    } catch (ex) {
      this._log.warn("Exception", ex);
    }
  },

  // AddonListeners
  onEnabled: function onEnabled(addon) {
    this.queueCaller.enqueueCall(() =>
      this._handleListener("onEnabled", addon)
    );
  },
  onDisabled: function onDisabled(addon) {
    this.queueCaller.enqueueCall(() =>
      this._handleListener("onDisabled", addon)
    );
  },
  onInstalled: function onInstalled(addon) {
    this.queueCaller.enqueueCall(() =>
      this._handleListener("onInstalled", addon)
    );
  },
  onUninstalled: function onUninstalled(addon) {
    this.queueCaller.enqueueCall(() =>
      this._handleListener("onUninstalled", addon)
    );
  },
  onOperationCancelled: function onOperationCancelled(addon) {
    this.queueCaller.enqueueCall(() =>
      this._handleListener("onOperationCancelled", addon)
    );
  },
};
