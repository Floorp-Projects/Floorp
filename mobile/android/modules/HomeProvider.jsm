// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "HomeProvider" ];

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Sqlite.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const SCHEMA_VERSION = 1;

const DB_PATH = OS.Path.join(OS.Constants.Path.profileDir, "home.sqlite");

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
      "created INTEGER" +
    ")",

  insertItem:
    "INSERT INTO items (dataset_id, url, title, description, image_url, created) " +
      "VALUES (:dataset_id, :url, :title, :description, :image_url, :created)",

  deleteFromDataset:
    "DELETE FROM items WHERE dataset_id = :dataset_id"
}

this.HomeProvider = Object.freeze({
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
  }
});

this.HomeStorage = function(datasetId) {
  this.datasetId = datasetId;
};

HomeStorage.prototype = {
  /**
   * Saves data rows to the DB.
   *
   * @param data
   *        (array) JSON array of row items
   *
   * @return Promise
   * @resolves When the operation has completed.
   */
  save: function(data) {
    return Task.spawn(function save_task() {
      let db = yield Sqlite.openConnection({ path: DB_PATH });

      try {
        // XXX: Factor this out to some migration path.
        if (!(yield db.tableExists("items"))) {
          yield db.execute(SQL.createItemsTable);
          yield db.setSchemaVersion(SCHEMA_VERSION);
        }

        // Insert data into DB.
        for (let item of data) {
          // XXX: Directly pass item as params? More validation for item? Batch insert?
          let params = {
            dataset_id: this.datasetId,
            url: item.url,
            title: item.title,
            description: item.description,
            image_url: item.image_url,
            created: Date.now()
          };
          yield db.executeCached(SQL.insertItem, params);
        }
      } finally {
        yield db.close();
      }
    }.bind(this));
  },

  /**
   * Deletes all rows associated with this storage.
   *
   * @return Promise
   * @resolves When the operation has completed.
   */
  deleteAll: function() {
    return Task.spawn(function delete_all_task() {
      let db = yield Sqlite.openConnection({ path: DB_PATH });

      try {
        // XXX: Check to make sure table exists.
        let params = { dataset_id: this.datasetId };
        yield db.executeCached(SQL.deleteFromDataset, params);
      } finally {
        yield db.close();
      }
    }.bind(this));
  }
};
