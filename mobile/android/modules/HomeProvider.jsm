// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "HomeProvider" ];

const { utils: Cu, classes: Cc, interfaces: Ci } = Components;

Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/*
 * SCHEMA_VERSION history:
 *   1: Create HomeProvider (bug 942288)
 *   2: Add filter column to items table (bug 942295/975841)
 *   3: Add background_color and background_url columns (bug 1157539)
 */
const SCHEMA_VERSION = 3;

// The maximum number of items you can attempt to save at once.
const MAX_SAVE_COUNT = 100;

XPCOMUtils.defineLazyGetter(this, "DB_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "home.sqlite");
});

const PREF_STORAGE_LAST_SYNC_TIME_PREFIX = "home.storage.lastSyncTime.";
const PREF_SYNC_UPDATE_MODE = "home.sync.updateMode";
const PREF_SYNC_CHECK_INTERVAL_SECS = "home.sync.checkIntervalSecs";

XPCOMUtils.defineLazyGetter(this, "gSyncCheckIntervalSecs", function() {
  return Services.prefs.getIntPref(PREF_SYNC_CHECK_INTERVAL_SECS);
});

XPCOMUtils.defineLazyServiceGetter(this,
  "gUpdateTimerManager", "@mozilla.org/updates/timer-manager;1", "nsIUpdateTimerManager");

/**
 * All SQL statements should be defined here.
 */
const SQL = {
  createItemsTable:
    "CREATE TABLE items (" +
      "_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
      "dataset_id TEXT NOT NULL, " +
      "url TEXT," +
      "title TEXT," +
      "description TEXT," +
      "image_url TEXT," +
      "background_color TEXT," +
      "background_url TEXT," +
      "filter TEXT," +
      "created INTEGER" +
    ")",

  dropItemsTable:
    "DROP TABLE items",

  insertItem:
    "INSERT INTO items (dataset_id, url, title, description, image_url, background_color, background_url, filter, created) " +
      "VALUES (:dataset_id, :url, :title, :description, :image_url, :background_color, :background_url, :filter, :created)",

  deleteFromDataset:
    "DELETE FROM items WHERE dataset_id = :dataset_id",

  addColumnBackgroundColor:
    "ALTER TABLE items ADD COLUMN background_color TEXT",

  addColumnBackgroundUrl:
    "ALTER TABLE items ADD COLUMN background_url TEXT",
};

/**
 * Technically this function checks to see if the user is on a local network,
 * but we express this as "wifi" to the user.
 */
function isUsingWifi() {
  let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
  return (network.linkType === Ci.nsINetworkLinkService.LINK_TYPE_WIFI || network.linkType === Ci.nsINetworkLinkService.LINK_TYPE_ETHERNET);
}

function getNowInSeconds() {
  return Math.round(Date.now() / 1000);
}

function getLastSyncPrefName(datasetId) {
  return PREF_STORAGE_LAST_SYNC_TIME_PREFIX + datasetId;
}

// Whether or not we've registered an update timer.
var gTimerRegistered = false;

// Map of datasetId -> { interval: <integer>, callback: <function> }
var gSyncCallbacks = {};

/**
 * nsITimerCallback implementation. Checks to see if it's time to sync any registered datasets.
 *
 * @param timer The timer which has expired.
 */
function syncTimerCallback(timer) {
  for (let datasetId in gSyncCallbacks) {
    let lastSyncTime = Services.prefs.getIntPref(getLastSyncPrefName(datasetId), 0);

    let now = getNowInSeconds();
    let { interval: interval, callback: callback } = gSyncCallbacks[datasetId];

    if (lastSyncTime < now - interval) {
      let success = HomeProvider.requestSync(datasetId, callback);
      if (success) {
        Services.prefs.setIntPref(getLastSyncPrefName(datasetId), now);
      }
    }
  }
}

this.HomeStorage = function(datasetId) {
  this.datasetId = datasetId;
};

this.ValidationError = function(message) {
  this.name = "ValidationError";
  this.message = message;
};
ValidationError.prototype = new Error();
ValidationError.prototype.constructor = ValidationError;

this.HomeProvider = Object.freeze({
  ValidationError: ValidationError,

  /**
   * Returns a storage associated with a given dataset identifer.
   *
   * @param datasetId
   *        (string) Unique identifier for the dataset.
   *
   * @return HomeStorage
   */
  getStorage: function(datasetId) {
    return new HomeStorage(datasetId);
  },

  /**
   * Checks to see if it's an appropriate time to sync.
   *
   * @param datasetId Unique identifier for the dataset to sync.
   * @param callback Function to call when it's time to sync, called with datasetId as a parameter.
   *
   * @return boolean Whether or not we were able to sync.
   */
  requestSync: function(datasetId, callback) {
    // Make sure it's a good time to sync.
    if ((Services.prefs.getIntPref(PREF_SYNC_UPDATE_MODE) === 1) && !isUsingWifi()) {
      Cu.reportError("HomeProvider: Failed to sync because device is not on a local network");
      return false;
    }

    callback(datasetId);
    return true;
  },

  /**
   * Specifies that a sync should be requested for the given dataset and update interval.
   *
   * @param datasetId Unique identifier for the dataset to sync.
   * @param interval Update interval in seconds. By default, this is throttled to 3600 seconds (1 hour).
   * @param callback Function to call when it's time to sync, called with datasetId as a parameter.
   */
  addPeriodicSync: function(datasetId, interval, callback) {
    // Warn developers if they're expecting more frequent notifications that we allow.
    if (interval < gSyncCheckIntervalSecs) {
      Cu.reportError("HomeProvider: Warning for dataset " + datasetId +
        " : Sync notifications are throttled to " + gSyncCheckIntervalSecs + " seconds");
    }

    gSyncCallbacks[datasetId] = {
      interval: interval,
      callback: callback
    };

    if (!gTimerRegistered) {
      gUpdateTimerManager.registerTimer("home-provider-sync-timer", syncTimerCallback, gSyncCheckIntervalSecs);
      gTimerRegistered = true;
    }
  },

  /**
   * Removes a periodic sync timer.
   *
   * @param datasetId Dataset to sync.
   */
  removePeriodicSync: function(datasetId) {
    delete gSyncCallbacks[datasetId];
    Services.prefs.clearUserPref(getLastSyncPrefName(datasetId));
    // You can't unregister a update timer, so we don't try to do that.
  }
});

var gDatabaseEnsured = false;

/**
 * Creates the database schema.
 */
function createDatabase(db) {
  return Task.spawn(function* create_database_task() {
    yield db.execute(SQL.createItemsTable);
  });
}

/**
 * Migrates the database schema to a new version.
 */
function upgradeDatabase(db, oldVersion, newVersion) {
  return Task.spawn(function* upgrade_database_task() {
    switch (oldVersion) {
      case 1:
        // Migration from v1 to latest:
        // Recreate the items table discarding any
        // existing data.
        yield db.execute(SQL.dropItemsTable);
        yield db.execute(SQL.createItemsTable);
        break;

      case 2:
        // Migration from v2 to latest:
        // Add new columns: background_color, background_url
        yield db.execute(SQL.addColumnBackgroundColor);
        yield db.execute(SQL.addColumnBackgroundUrl);
        break;
    }
  });
}

/**
 * Opens a database connection and makes sure that the database schema version
 * is correct, performing migrations if necessary. Consumers should be sure
 * to close any database connections they open.
 *
 * @return Promise
 * @resolves Handle on an opened SQLite database.
 */
function getDatabaseConnection() {
  return Task.spawn(function* get_database_connection_task() {
    let db = yield Sqlite.openConnection({ path: DB_PATH });
    if (gDatabaseEnsured) {
      return db;
    }

    try {
      // Check to see if we need to perform any migrations.
      let dbVersion = parseInt(yield db.getSchemaVersion());

      // getSchemaVersion() returns a 0 int if the schema
      // version is undefined.
      if (dbVersion === 0) {
        yield createDatabase(db);
      } else if (dbVersion < SCHEMA_VERSION) {
        yield upgradeDatabase(db, dbVersion, SCHEMA_VERSION);
      }

      yield db.setSchemaVersion(SCHEMA_VERSION);
    } catch (e) {
      // Close the DB connection before passing the exception to the consumer.
      yield db.close();
      throw e;
    }

    gDatabaseEnsured = true;
    return db;
  });
}

/**
 * Validates an item to be saved to the DB.
 *
 * @param item
 *        (object) item object to be validated.
 */
function validateItem(datasetId, item) {
  if (!item.url) {
    throw new ValidationError("HomeStorage: All rows must have an URL: datasetId = " +
                              datasetId);
  }

  if (!item.image_url && !item.title && !item.description) {
    throw new ValidationError("HomeStorage: All rows must have at least an image URL, " +
                              "or a title or a description: datasetId = " + datasetId);
  }
}

var gRefreshTimers = {};

/**
 * Sends a message to Java to refresh the given dataset. Delays sending
 * messages to avoid successive refreshes, which can result in flashing views.
 */
function refreshDataset(datasetId) {
  // Bail if there's already a refresh timer waiting to fire
  if (gRefreshTimers[datasetId]) {
    return;
  }

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(function(timer) {
    delete gRefreshTimers[datasetId];

    EventDispatcher.instance.sendRequest({
      type: "HomePanels:RefreshDataset",
      datasetId: datasetId
    });
  }, 100, Ci.nsITimer.TYPE_ONE_SHOT);

  gRefreshTimers[datasetId] = timer;
}

HomeStorage.prototype = {
  /**
   * Saves data rows to the DB.
   *
   * @param data
   *        An array of JS objects represnting row items to save.
   *        Each object may have the following properties:
   *        - url (string)
   *        - title (string)
   *        - description (string)
   *        - image_url (string)
   *        - filter (string)
   * @param options
   *        A JS object holding additional cofiguration properties.
   *        The following properties are currently supported:
   *        - replace (boolean): Whether or not to replace existing items.
   *
   * @return Promise
   * @resolves When the operation has completed.
   */
  save: function(data, options) {
    if (data && data.length > MAX_SAVE_COUNT) {
      throw "save failed for dataset = " + this.datasetId +
        ": you cannot save more than " + MAX_SAVE_COUNT + " items at once";
    }

    return Task.spawn(function* save_task() {
      let db = yield getDatabaseConnection();
      try {
        yield db.executeTransaction(function* save_transaction() {
          if (options && options.replace) {
            yield db.executeCached(SQL.deleteFromDataset, { dataset_id: this.datasetId });
          }

          // Insert data into DB.
          for (let item of data) {
            validateItem(this.datasetId, item);

            // XXX: Directly pass item as params? More validation for item?
            let params = {
              dataset_id: this.datasetId,
              url: item.url,
              title: item.title,
              description: item.description,
              image_url: item.image_url,
              background_color: item.background_color,
              background_url: item.background_url,
              filter: item.filter,
              created: Date.now()
            };
            yield db.executeCached(SQL.insertItem, params);
          }
        }.bind(this));
      } finally {
        yield db.close();
      }

      refreshDataset(this.datasetId);
    }.bind(this));
  },

  /**
   * Deletes all rows associated with this storage.
   *
   * @return Promise
   * @resolves When the operation has completed.
   */
  deleteAll: function() {
    return Task.spawn(function* delete_all_task() {
      let db = yield getDatabaseConnection();
      try {
        let params = { dataset_id: this.datasetId };
        yield db.executeCached(SQL.deleteFromDataset, params);
      } finally {
        yield db.close();
      }

      refreshDataset(this.datasetId);
    }.bind(this));
  }
};
