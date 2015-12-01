/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Bookmarks"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource:///modules/PlacesUIUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/CloudSyncPlacesWrapper.jsm");
Cu.import("resource://gre/modules/CloudSyncEventSource.jsm");
Cu.import("resource://gre/modules/CloudSyncBookmarksFolderCache.jsm");

const ITEM_TYPES = [
  "NULL",
  "BOOKMARK",
  "FOLDER",
  "SEPARATOR",
  "DYNAMIC_CONTAINER", // no longer used by Places, but this ID should not be used for future item types
];

const CS_UNKNOWN = 0x1;
const CS_FOLDER = 0x1 << 1;
const CS_SEPARATOR = 0x1 << 2;
const CS_QUERY = 0x1 << 3;
const CS_LIVEMARK = 0x1 << 4;
const CS_BOOKMARK = 0x1 << 5;

const EXCLUDE_BACKUP_ANNO = "places/excludeFromBackup";

const DATA_VERSION = 1;

function asyncCallback(ctx, func, args) {
  function invoke() {
    func.apply(ctx, args);
  }
  CommonUtils.nextTick(invoke);
}

var Record = function (params) {
  this.id = params.guid;
  this.parent = params.parent || null;
  this.index = params.position;
  this.title = params.title;
  this.dateAdded = Math.floor(params.dateAdded/1000);
  this.lastModified = Math.floor(params.lastModified/1000);
  this.uri = params.url;

  let annos = params.annos || {};
  Object.defineProperty(this, "annos", {
    get: function () {
      return annos;
    },
    enumerable: false
  });

  switch (params.type) {
    case PlacesUtils.bookmarks.TYPE_FOLDER:
      if (PlacesUtils.LMANNO_FEEDURI in annos) {
        this.type = CS_LIVEMARK;
        this.feed = annos[PlacesUtils.LMANNO_FEEDURI];
        this.site = annos[PlacesUtils.LMANNO_SITEURI];
      } else {
        this.type = CS_FOLDER;
      }
      break;
    case PlacesUtils.bookmarks.TYPE_BOOKMARK:
      if (this.uri.startsWith("place:")) {
        this.type = CS_QUERY;
      } else {
        this.type = CS_BOOKMARK;
      }
      break;
    case PlacesUtils.bookmarks.TYPE_SEPARATOR:
      this.type = CS_SEPARATOR;
      break;
    default:
      this.type = CS_UNKNOWN;
  }
};

Record.prototype = {
  version: DATA_VERSION,
};

var Bookmarks = function () {
  let createRootFolder = function (name) {
    let ROOT_FOLDER_ANNO = "cloudsync/rootFolder/" + name;
    let ROOT_SHORTCUT_ANNO = "cloudsync/rootShortcut/" + name;

    let deferred = Promise.defer();
    let placesRootId = PlacesUtils.placesRootId;
    let rootFolderId;
    let rootShortcutId;

    function createAdapterShortcut(result) {
      rootFolderId = result;
      let uri = "place:folder=" + rootFolderId;
      return PlacesWrapper.insertBookmark(PlacesUIUtils.allBookmarksFolderId, uri,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX, name);
    }

    function setRootFolderCloudSyncAnnotation(result) {
      rootShortcutId = result;
      return PlacesWrapper.setItemAnnotation(rootFolderId, ROOT_FOLDER_ANNO,
                                             1, 0, PlacesUtils.annotations.EXPIRE_NEVER);
    }

    function setRootShortcutCloudSyncAnnotation() {
      return PlacesWrapper.setItemAnnotation(rootShortcutId, ROOT_SHORTCUT_ANNO,
                                             1, 0, PlacesUtils.annotations.EXPIRE_NEVER);
    }

    function setRootFolderExcludeFromBackupAnnotation() {
      return PlacesWrapper.setItemAnnotation(rootFolderId, EXCLUDE_BACKUP_ANNO,
                                             1, 0, PlacesUtils.annotations.EXPIRE_NEVER);
    }

    function finish() {
      deferred.resolve(rootFolderId);
    }

    Promise.resolve(PlacesUtils.bookmarks.createFolder(placesRootId, name, PlacesUtils.bookmarks.DEFAULT_INDEX))
      .then(createAdapterShortcut)
      .then(setRootFolderCloudSyncAnnotation)
      .then(setRootShortcutCloudSyncAnnotation)
      .then(setRootFolderExcludeFromBackupAnnotation)
      .then(finish, deferred.reject);

    return deferred.promise;
  };

  let getRootFolder = function (name) {
    let ROOT_FOLDER_ANNO = "cloudsync/rootFolder/" + name;
    let ROOT_SHORTCUT_ANNO = "cloudsync/rootShortcut/" + name;
    let deferred = Promise.defer();

    function checkRootFolder(folderIds) {
      if (!folderIds.length) {
        return createRootFolder(name);
      }
      return Promise.resolve(folderIds[0]);
    }

    function createFolderObject(folderId) {
      return new RootFolder(folderId, name);
    }

    PlacesWrapper.getLocalIdsWithAnnotation(ROOT_FOLDER_ANNO)
      .then(checkRootFolder, deferred.reject)
      .then(createFolderObject)
      .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  };

  let deleteRootFolder = function (name) {
    let ROOT_FOLDER_ANNO = "cloudsync/rootFolder/" + name;
    let ROOT_SHORTCUT_ANNO = "cloudsync/rootShortcut/" + name;

    let deferred = Promise.defer();
    let placesRootId = PlacesUtils.placesRootId;

    function getRootShortcutId() {
      return PlacesWrapper.getLocalIdsWithAnnotation(ROOT_SHORTCUT_ANNO);
    }

    function deleteShortcut(shortcutIds) {
      if (!shortcutIds.length) {
        return Promise.resolve();
      }
      return PlacesWrapper.removeItem(shortcutIds[0]);
    }

    function getRootFolderId() {
      return PlacesWrapper.getLocalIdsWithAnnotation(ROOT_FOLDER_ANNO);
    }

    function deleteFolder(folderIds) {
      let deleteFolderDeferred = Promise.defer();

      if (!folderIds.length) {
        return Promise.resolve();
      }

      let rootFolderId = folderIds[0];
      PlacesWrapper.removeFolderChildren(rootFolderId).then(
        function () {
          return PlacesWrapper.removeItem(rootFolderId);
        }
      ).then(deleteFolderDeferred.resolve, deleteFolderDeferred.reject);

      return deleteFolderDeferred.promise;
    }

    getRootShortcutId().then(deleteShortcut)
                       .then(getRootFolderId)
                       .then(deleteFolder)
                       .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  };

  /* PUBLIC API */
  this.getRootFolder = getRootFolder.bind(this);
  this.deleteRootFolder = deleteRootFolder.bind(this);

};

this.Bookmarks = Bookmarks;

var RootFolder = function (rootId, rootName) {
  let suspended = true;
  let ignoreAll = false;

  let suspend = function () {
    if (!suspended) {
      PlacesUtils.bookmarks.removeObserver(observer);
      suspended = true;
    }
  }.bind(this);

  let resume = function () {
    if (suspended) {
      PlacesUtils.bookmarks.addObserver(observer, false);
      suspended = false;
    }
  }.bind(this);

  let eventTypes = [
    "add",
    "remove",
    "change",
    "move",
  ];

  let eventSource = new EventSource(eventTypes, suspend, resume);

  let folderCache = new FolderCache;
  folderCache.insert(rootId, null);

  let getCachedFolderIds = function (cache, roots) {
    let nodes = [...roots];
    let results = [];

    while (nodes.length) {
      let node = nodes.shift();
      results.push(node);
      let children = cache.getChildren(node);
      nodes = nodes.concat([...children]);
    }
    return results;
  };

  let getLocalItems = function () {
    let deferred = Promise.defer();

    let folders = getCachedFolderIds(folderCache, folderCache.getChildren(rootId));

    function getFolders(ids) {
      let types = [
        PlacesUtils.bookmarks.TYPE_FOLDER,
      ];
      return PlacesWrapper.getItemsById(ids, types);
    }

    function getContents(parents) {
      parents.push(rootId);
      let types = [
        PlacesUtils.bookmarks.TYPE_BOOKMARK,
        PlacesUtils.bookmarks.TYPE_SEPARATOR,
      ];
      return PlacesWrapper.getItemsByParentId(parents, types)
    }

    function getParentGuids(results) {
      results = Array.prototype.concat.apply([], results);
      let promises = [];
      results.map(function (result) {
        let promise = PlacesWrapper.localIdToGuid(result.parent).then(
          function (guidResult) {
            result.parent = guidResult;
            return Promise.resolve(result);
          },
          Promise.reject.bind(Promise)
        );
        promises.push(promise);
      });
      return Promise.all(promises);
    }

    function getAnnos(results) {
      results = Array.prototype.concat.apply([], results);
      let promises = [];
      results.map(function (result) {
        let promise = PlacesWrapper.getItemAnnotationsForLocalId(result.id).then(
          function (annos) {
            result.annos = annos;
            return Promise.resolve(result);
          },
          Promise.reject.bind(Promise)
        );
        promises.push(promise);
      });
      return Promise.all(promises);
    }

    let promises = [
      getFolders(folders),
      getContents(folders),
    ];

    Promise.all(promises)
           .then(getParentGuids)
           .then(getAnnos)
           .then(function (results) {
                   results = results.map((result) => new Record(result));
                   deferred.resolve(results);
                 },
                 deferred.reject);

    return deferred.promise;
  };

  let getLocalItemsById = function (guids) {
    let deferred = Promise.defer();

    let types = [
      PlacesUtils.bookmarks.TYPE_BOOKMARK,
      PlacesUtils.bookmarks.TYPE_FOLDER,
      PlacesUtils.bookmarks.TYPE_SEPARATOR,
      PlacesUtils.bookmarks.TYPE_DYNAMIC_CONTAINER,
    ];

    function getParentGuids(results) {
      let promises = [];
      results.map(function (result) {
        let promise = PlacesWrapper.localIdToGuid(result.parent).then(
          function (guidResult) {
            result.parent = guidResult;
            return Promise.resolve(result);
          },
          Promise.reject.bind(Promise)
        );
        promises.push(promise);
      });
      return Promise.all(promises);
    }

    PlacesWrapper.getItemsByGuid(guids, types)
                 .then(getParentGuids)
                 .then(function (results) {
                         results = results.map((result) => new Record(result));
                         deferred.resolve(results);
                       },
                       deferred.reject);

    return deferred.promise;
  };

  let _createItem = function (item) {
    let deferred = Promise.defer();

    function getFolderId() {
      if (item.parent) {
        return PlacesWrapper.guidToLocalId(item.parent);
      }
      return Promise.resolve(rootId);
    }

    function create(folderId) {
      let deferred = Promise.defer();

      if (!folderId) {
        folderId = rootId;
      }
      let index = item.hasOwnProperty("index") ? item.index : PlacesUtils.bookmarks.DEFAULT_INDEX;

      function complete(localId) {
        folderCache.insert(localId, folderId);
        deferred.resolve(localId);
      }

      switch (item.type) {
        case CS_BOOKMARK:
        case CS_QUERY:
          PlacesWrapper.insertBookmark(folderId, item.uri, index, item.title, item.id)
                       .then(complete, deferred.reject);
          break;
        case CS_FOLDER:
          PlacesWrapper.createFolder(folderId, item.title, index, item.id)
                       .then(complete, deferred.reject);
          break;
        case CS_SEPARATOR:
          PlacesWrapper.insertSeparator(folderId, index, item.id)
                       .then(complete, deferred.reject);
          break;
        case CS_LIVEMARK:
          let livemark = {
            title: item.title,
            parentId: folderId,
            index: item.index,
            feedURI: item.feed,
            siteURI: item.site,
            guid: item.id,
          };
          PlacesUtils.livemarks.addLivemark(livemark)
                               .then(complete, deferred.reject);
          break;
        default:
          deferred.reject("invalid item type: " + item.type);
      }

      return deferred.promise;
    }

    getFolderId().then(create)
                 .then(deferred.resolve, deferred.reject);

    return deferred.promise;
  };

  let _deleteItem = function (item) {
    let deferred = Promise.defer();

    PlacesWrapper.guidToLocalId(item.id).then(
      function (localId) {
        folderCache.remove(localId);
        return PlacesWrapper.removeItem(localId);
      }
    ).then(deferred.resolve, deferred.reject);

    return deferred.promise;
  };

  let _updateItem = function (item) {
    let deferred = Promise.defer();

    PlacesWrapper.guidToLocalId(item.id).then(
      function (localId) {
        let promises = [];

        if (item.hasOwnProperty("dateAdded")) {
          promises.push(PlacesWrapper.setItemDateAdded(localId, item.dateAdded));
        }

        if (item.hasOwnProperty("lastModified")) {
          promises.push(PlacesWrapper.setItemLastModified(localId, item.lastModified));
        }

        if ((CS_BOOKMARK | CS_FOLDER) & item.type && item.hasOwnProperty("title")) {
          promises.push(PlacesWrapper.setItemTitle(localId, item.title));
        }

        if (CS_BOOKMARK & item.type && item.hasOwnProperty("uri")) {
          promises.push(PlacesWrapper.changeBookmarkURI(localId, item.uri));
        }

        if (item.hasOwnProperty("parent")) {
          let deferred = Promise.defer();
          PlacesWrapper.guidToLocalId(item.parent)
            .then(
                function (parent) {
                  let index = item.hasOwnProperty("index") ? item.index : PlacesUtils.bookmarks.DEFAULT_INDEX;
                  if (CS_FOLDER & item.type) {
                    folderCache.setParent(localId, parent);
                  }
                  return PlacesWrapper.moveItem(localId, parent, index);
                }
              )
            .then(deferred.resolve, deferred.reject);
          promises.push(deferred.promise);
        }

        if (item.hasOwnProperty("index") && !item.hasOwnProperty("parent")) {
          promises.push(Task.spawn(function* () {
            let localItem = (yield getLocalItemsById([item.id]))[0];
            let parent = yield PlacesWrapper.guidToLocalId(localItem.parent);
            let index = item.index;
            if (CS_FOLDER & item.type) {
              folderCache.setParent(localId, parent);
            }
            yield PlacesWrapper.moveItem(localId, parent, index);
          }));
        }

        Promise.all(promises)
               .then(deferred.resolve, deferred.reject);
      }
    );

    return deferred.promise;
  };

  let mergeRemoteItems = function (items) {
    ignoreAll = true;
    let deferred = Promise.defer();

    let newFolders = {};
    let newItems = [];
    let updatedItems = [];
    let deletedItems = [];

    let sortItems = function () {
      let promises = [];

      let exists = function (item) {
        let existsDeferred = Promise.defer();
        if (!item.id) {
          Object.defineProperty(item, "__exists__", {
            value: false,
            enumerable: false
          });
          existsDeferred.resolve(item);
        } else {
          PlacesWrapper.guidToLocalId(item.id).then(
            function (localId) {
              Object.defineProperty(item, "__exists__", {
                value: localId ? true : false,
                enumerable: false
              });
              existsDeferred.resolve(item);
            },
            existsDeferred.reject
          );
        }
        return existsDeferred.promise;
      }

      let handleSortedItem = function (item) {
        if (!item.__exists__ && !item.deleted) {
          if (CS_FOLDER == item.type) {
            newFolders[item.id] = item;
            item._children = [];
          } else {
            newItems.push(item);
          }
        } else if (item.__exists__ && item.deleted) {
          deletedItems.push(item);
        } else if (item.__exists__) {
          updatedItems.push(item);
        }
      }

      for (let item of items) {
        if (!item || 'object' !== typeof(item)) {
          continue;
        }

        let promise = exists(item).then(handleSortedItem, Promise.reject.bind(Promise));
        promises.push(promise);
      }

      return Promise.all(promises);
    }

    let processNewFolders = function () {
      let newFolderGuids = Object.keys(newFolders);
      let newFolderRoots = [];

      for (let guid of newFolderGuids) {
        let item = newFolders[guid];
        if (item.parent && newFolderGuids.indexOf(item.parent) >= 0) {
          let parent = newFolders[item.parent];
          parent._children.push(item.id);
        } else {
          newFolderRoots.push(guid);
        }
      };

      let promises = [];
      for (let guid of newFolderRoots) {
        let root = newFolders[guid];
        let promise = Promise.resolve();
        promise = promise.then(
          function () {
            return _createItem(root);
          },
          Promise.reject.bind(Promise)
        );
        let items = [].concat(root._children);

        while (items.length) {
          let item = newFolders[items.shift()];
          items = items.concat(item._children);
          promise = promise.then(
            function () {
              return _createItem(item);
            },
            Promise.reject.bind(Promise)
          );
        }
        promises.push(promise);
      }

      return Promise.all(promises);
    }

    let processItems = function () {
      let promises = [];

      for (let item of newItems) {
        promises.push(_createItem(item));
      }

      for (let item of updatedItems) {
        promises.push(_updateItem(item));
      }

      for (let item of deletedItems) {
        _deleteItem(item);
      }

      return Promise.all(promises);
    }

    sortItems().then(processNewFolders)
               .then(processItems)
               .then(function () {
                       ignoreAll = false;
                       deferred.resolve(items);
                     },
                     function (err) {
                       ignoreAll = false;
                       deferred.reject(err);
                     });

    return deferred.promise;
  };

  let ignore = function (id, parent) {
    if (ignoreAll) {
      return true;
    }

    if (rootId == parent || folderCache.has(parent)) {
      return false;
    }

    return true;
  };

  let handleItemAdded = function (id, parent, index, type, uri, title, dateAdded, guid, parentGuid) {
    let deferred = Promise.defer();

    if (PlacesUtils.bookmarks.TYPE_FOLDER == type) {
      folderCache.insert(id, parent);
    }

    eventSource.emit("add", guid);
    deferred.resolve();

    return deferred.promise;
  };

  let handleItemRemoved = function (id, parent, index, type, uri, guid, parentGuid) {
    let deferred = Promise.defer();

    if (PlacesUtils.bookmarks.TYPE_FOLDER == type) {
      folderCache.remove(id);
    }

    eventSource.emit("remove", guid);
    deferred.resolve();

    return deferred.promise;
  };

  let handleItemChanged = function (id, property, isAnnotation, newValue, lastModified, type, parent, guid, parentGuid) {
    let deferred = Promise.defer();

    eventSource.emit('change', guid);
    deferred.resolve();

    return deferred.promise;
  };

  let handleItemMoved = function (id, oldParent, oldIndex, newParent, newIndex, type, guid, oldParentGuid, newParentGuid) {
    let deferred = Promise.defer();

    function complete() {
      eventSource.emit('move', guid);
      deferred.resolve();
    }

    if (PlacesUtils.bookmarks.TYPE_FOLDER != type) {
      complete();
      return deferred.promise;
    }

    if (folderCache.has(oldParent) && folderCache.has(newParent)) {
      // Folder move inside cloudSync root, so just update parents/children.
      folderCache.setParent(id, newParent);
      complete();
    } else if (!folderCache.has(oldParent)) {
      // Folder moved in from ouside cloudSync root.
      PlacesWrapper.updateCachedFolderIds(folderCache, newParent)
                   .then(complete, complete);
    } else if (!folderCache.has(newParent)) {
      // Folder moved out from inside cloudSync root.
      PlacesWrapper.updateCachedFolderIds(folderCache, oldParent)
                   .then(complete, complete);
    }

    return deferred.promise;
  };

  let observer = {
    onBeginBatchUpdate: function () {
    },

    onEndBatchUpdate: function () {
    },

    onItemAdded: function (id, parent, index, type, uri, title, dateAdded, guid, parentGuid) {
      if (ignore(id, parent)) {
        return;
      }

      asyncCallback(this, handleItemAdded, Array.prototype.slice.call(arguments));
    },

    onItemRemoved: function (id, parent, index, type, uri, guid, parentGuid) {
      if (ignore(id, parent)) {
        return;
      }

      asyncCallback(this, handleItemRemoved, Array.prototype.slice.call(arguments));
    },

    onItemChanged: function (id, property, isAnnotation, newValue, lastModified, type, parent, guid, parentGuid) {
      if (ignore(id, parent)) {
        return;
      }

      asyncCallback(this, handleItemChanged, Array.prototype.slice.call(arguments));
    },

    onItemMoved: function (id, oldParent, oldIndex, newParent, newIndex, type, guid, oldParentGuid, newParentGuid) {
      if (ignore(id, oldParent) && ignore(id, newParent)) {
        return;
      }

      asyncCallback(this, handleItemMoved, Array.prototype.slice.call(arguments));
    }
  };

  /* PUBLIC API */
  this.addEventListener = eventSource.addEventListener;
  this.removeEventListener = eventSource.removeEventListener;
  this.getLocalItems = getLocalItems.bind(this);
  this.getLocalItemsById = getLocalItemsById.bind(this);
  this.mergeRemoteItems = mergeRemoteItems.bind(this);

  let rootGuid = null; // resolved before becoming ready (below)
  this.__defineGetter__("id", function () {
    return rootGuid;
  });
  this.__defineGetter__("name", function () {
    return rootName;
  });

  let deferred = Promise.defer();
  let getGuidForRootFolder = function () {
    return PlacesWrapper.localIdToGuid(rootId);
  }
  PlacesWrapper.updateCachedFolderIds(folderCache, rootId)
               .then(getGuidForRootFolder, getGuidForRootFolder)
               .then(function (guid) {
                       rootGuid = guid;
                       deferred.resolve(this);
                     }.bind(this),
                     deferred.reject);
  return deferred.promise;
};

RootFolder.prototype = {
  BOOKMARK: CS_BOOKMARK,
  FOLDER: CS_FOLDER,
  SEPARATOR: CS_SEPARATOR,
  QUERY: CS_QUERY,
  LIVEMARK: CS_LIVEMARK,
};
