/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PlacesWrapper"];

const {interfaces: Ci, utils: Cu} = Components;
const REASON_ERROR = Ci.mozIStorageStatementCallback.REASON_ERROR;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource:///modules/PlacesUIUtils.jsm");
Cu.import("resource://services-common/utils.js");

var PlacesQueries = function() {
}

PlacesQueries.prototype = {
  cachedStmts: {},

  getQuery(queryString) {
    if (queryString in this.cachedStmts) {
      return this.cachedStmts[queryString];
    }

    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
    return this.cachedStmts[queryString] = db.createAsyncStatement(queryString);
  }
};

var PlacesWrapper = function() {
}

PlacesWrapper.prototype = {
  placesQueries: new PlacesQueries(),

  guidToLocalId(guid) {
    let deferred = Promise.defer();

    let stmt = "SELECT id AS item_id " +
               "FROM moz_bookmarks " +
               "WHERE guid = :guid";
    let query = this.placesQueries.getQuery(stmt);

    function getLocalId(results) {
      let result = results[0] && results[0]["item_id"];
      return Promise.resolve(result);
    }

    query.params.guid = guid.toString();

    this.asyncQuery(query, ["item_id"])
        .then(getLocalId, deferred.reject)
        .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  },

  localIdToGuid(id) {
    let deferred = Promise.defer();

    let stmt = "SELECT guid " +
               "FROM moz_bookmarks " +
               "WHERE id = :item_id";
    let query = this.placesQueries.getQuery(stmt);

    function getGuid(results) {
      let result = results[0] && results[0]["guid"];
      return Promise.resolve(result);
    }

    query.params.item_id = id;

    this.asyncQuery(query, ["guid"])
        .then(getGuid, deferred.reject)
        .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  },

  getItemsById(ids, types) {
    let deferred = Promise.defer();
    let stmt = "SELECT b.id, b.type, b.parent, b.position, b.title, b.guid, b.dateAdded, b.lastModified, p.url " +
               "FROM moz_bookmarks b " +
               "LEFT JOIN moz_places p ON b.fk = p.id " +
               "WHERE b.id in (" + ids.join(",") + ") AND b.type in (" + types.join(",") + ")";
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
    let query = db.createAsyncStatement(stmt);

    this.asyncQuery(query, ["id", "type", "parent", "position", "title", "guid", "dateAdded", "lastModified", "url"])
        .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  },

  getItemsByParentId(parents, types) {
    let deferred = Promise.defer();
    let stmt = "SELECT b.id, b.type, b.parent, b.position, b.title, b.guid, b.dateAdded, b.lastModified, p.url " +
               "FROM moz_bookmarks b " +
               "LEFT JOIN moz_places p ON b.fk = p.id " +
               "WHERE b.parent in (" + parents.join(",") + ") AND b.type in (" + types.join(",") + ")";
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
    let query = db.createAsyncStatement(stmt);

    this.asyncQuery(query, ["id", "type", "parent", "position", "title", "guid", "dateAdded", "lastModified", "url"])
        .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  },

  getItemsByGuid(guids, types) {
    let deferred = Promise.defer();
    guids = guids.map(JSON.stringify);
    let stmt = "SELECT b.id, b.type, b.parent, b.position, b.title, b.guid, b.dateAdded, b.lastModified, p.url " +
               "FROM moz_bookmarks b " +
               "LEFT JOIN moz_places p ON b.fk = p.id " +
               "WHERE b.guid in (" + guids.join(",") + ") AND b.type in (" + types.join(",") + ")";
    let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
    let query = db.createAsyncStatement(stmt);

    this.asyncQuery(query, ["id", "type", "parent", "position", "title", "guid", "dateAdded", "lastModified", "url"])
        .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  },

  updateCachedFolderIds(folderCache, folder) {
    let deferred = Promise.defer();
    let stmt = "SELECT id, guid " +
               "FROM moz_bookmarks " +
               "WHERE parent = :parent_id AND type = :item_type";
    let query = this.placesQueries.getQuery(stmt);

    query.params.parent_id = folder;
    query.params.item_type = PlacesUtils.bookmarks.TYPE_FOLDER;

    this.asyncQuery(query, ["id", "guid"]).then(
      function(items) {
        let previousIds = folderCache.getChildren(folder);
        let currentIds = new Set();
        for (let item of items) {
          currentIds.add(item.id);
        }
        let newIds = new Set();
        let missingIds = new Set();

        for (let currentId of currentIds) {
          if (!previousIds.has(currentId)) {
            newIds.add(currentId);
          }
        }
        for (let previousId of previousIds) {
          if (!currentIds.has(previousId)) {
            missingIds.add(previousId);
          }
        }

        folderCache.setChildren(folder, currentIds);

        let promises = [];
        for (let newId of newIds) {
          promises.push(this.updateCachedFolderIds(folderCache, newId));
        }
        Promise.all(promises)
               .then(deferred.resolve, deferred.reject);

        for (let missingId of missingIds) {
          folderCache.remove(missingId);
        }
      }.bind(this)
    );

    return deferred.promise;
  },

  getLocalIdsWithAnnotation(anno) {
    let deferred = Promise.defer();
    let stmt = "SELECT a.item_id " +
               "FROM moz_anno_attributes n " +
               "JOIN moz_items_annos a ON n.id = a.anno_attribute_id " +
               "WHERE n.name = :anno_name";
    let query = this.placesQueries.getQuery(stmt);

    query.params.anno_name = anno.toString();

    this.asyncQuery(query, ["item_id"])
        .then(function(items) {
                let results = [];
                for (let item of items) {
                  results.push(item.item_id);
                }
                deferred.resolve(results);
              },
              deferred.reject);

    return deferred.promise;
  },

  getItemAnnotationsForLocalId(id) {
    let deferred = Promise.defer();
    let stmt = "SELECT a.name, b.content " +
               "FROM moz_anno_attributes a " +
               "JOIN moz_items_annos b ON a.id = b.anno_attribute_id " +
               "WHERE b.item_id = :item_id";
    let query = this.placesQueries.getQuery(stmt);

    query.params.item_id = id;

    this.asyncQuery(query, ["name", "content"])
        .then(function(results) {
                let annos = {};
                for (let result of results) {
                  annos[result.name] = result.content;
                }
                deferred.resolve(annos);
              },
              deferred.reject);

    return deferred.promise;
  },

  insertBookmark(parent, uri, index, title, guid) {
    let parsedURI;
    try {
      parsedURI = CommonUtils.makeURI(uri)
    } catch (e) {
      return Promise.reject("unable to parse URI '" + uri + "': " + e);
    }

    try {
      let id = PlacesUtils.bookmarks.insertBookmark(parent, parsedURI, index, title, guid);
      return Promise.resolve(id);
    } catch (e) {
      return Promise.reject("unable to insert bookmark " + JSON.stringify(arguments) + ": " + e);
    }
  },

  setItemAnnotation(item, anno, value, flags, exp) {
    try {
      return Promise.resolve(PlacesUtils.annotations.setItemAnnotation(item, anno, value, flags, exp));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  itemHasAnnotation(item, anno) {
    try {
      return Promise.resolve(PlacesUtils.annotations.itemHasAnnotation(item, anno));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  createFolder(parent, name, index, guid) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.createFolder(parent, name, index, guid));
    } catch (e) {
      return Promise.reject("unable to create folder ['" + name + "']: " + e);
    }
  },

  removeFolderChildren(folder) {
    try {
      PlacesUtils.bookmarks.removeFolderChildren(folder);
      return Promise.resolve();
    } catch (e) {
      return Promise.reject(e);
    }
  },

  insertSeparator(parent, index, guid) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.insertSeparator(parent, index, guid));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  removeItem(item) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.removeItem(item));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  setItemDateAdded(item, dateAdded) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.setItemDateAdded(item, dateAdded));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  setItemLastModified(item, lastModified) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.setItemLastModified(item, lastModified));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  setItemTitle(item, title) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.setItemTitle(item, title));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  changeBookmarkURI(item, uri) {
    try {
      uri = CommonUtils.makeURI(uri);
      return Promise.resolve(PlacesUtils.bookmarks.changeBookmarkURI(item, uri));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  moveItem(item, parent, index) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.moveItem(item, parent, index));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  setItemIndex(item, index) {
    try {
      return Promise.resolve(PlacesUtils.bookmarks.setItemIndex(item, index));
    } catch (e) {
      return Promise.reject(e);
    }
  },

  asyncQuery(query, names) {
    let deferred = Promise.defer();
    let storageCallback = {
      results: [],
      handleResult(results) {
        if (!names) {
          return;
        }

        let row;
        while ((row = results.getNextRow()) != null) {
          let item = {};
          for (let name of names) {
            item[name] = row.getResultByName(name);
          }
          this.results.push(item);
        }
      },

      handleError(error) {
        deferred.reject(error);
      },

      handleCompletion(reason) {
        if (REASON_ERROR == reason) {
          return;
        }

        deferred.resolve(this.results);
      }
    };

    query.executeAsync(storageCallback);
    return deferred.promise;
  },
};

this.PlacesWrapper = new PlacesWrapper();
