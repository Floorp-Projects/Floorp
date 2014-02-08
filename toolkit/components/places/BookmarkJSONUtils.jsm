/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "BookmarkJSONUtils" ];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesBackups",
  "resource://gre/modules/PlacesBackups.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => new TextDecoder());
XPCOMUtils.defineLazyGetter(this, "gTextEncoder", () => new TextEncoder());
XPCOMUtils.defineLazyGetter(this, "localFileCtor",
  () => Components.Constructor("@mozilla.org/file/local;1",
                               "nsILocalFile", "initWithPath"));

this.BookmarkJSONUtils = Object.freeze({
  /**
   * Import bookmarks from a url.
   *
   * @param aURL
   *        url of the bookmark data.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromURL: function BJU_importFromURL(aURL, aReplace) {
    let importer = new BookmarkImporter();
    return importer.importFromURL(aURL, aReplace);
  },

  /**
   * Restores bookmarks and tags from a JSON file.
   * @note any item annotated with "places/excludeFromBackup" won't be removed
   *       before executing the restore.
   *
   * @param aFilePath
   *        OS.File path or nsIFile of bookmarks in JSON format to be restored.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromFile: function BJU_importFromFile(aFilePath, aReplace) {
    let importer = new BookmarkImporter();
    // TODO (bug 967192): convert to pure OS.File
    let file = aFilePath instanceof Ci.nsIFile ? aFilePath
                                               : new localFileCtor(aFilePath);
    return importer.importFromFile(file, aReplace);
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file path.
   *
   * @param aFilePath
   *        OS.File path for the "bookmarks.json" file to be created.
   *
   * @return {Promise}
   * @resolves To the exported bookmarks count when the file has been created.
   * @rejects JavaScript exception.
   * @deprecated passing an nsIFile is deprecated
   */
  exportToFile: function BJU_exportToFile(aFilePath) {
    if (aFilePath instanceof Ci.nsIFile) {
      Deprecated.warning("Passing an nsIFile to BookmarksJSONUtils.exportToFile " +
                         "is deprecated. Please use an OS.File path instead.",
                         "https://developer.mozilla.org/docs/JavaScript_OS.File");
      aFilePath = aFilePath.path;
    }
    return Task.spawn(function* () {
      let [bookmarks, count] = yield PlacesBackups.getBookmarksTree();
      let startTime = Date.now();
      let jsonString = JSON.stringify(bookmarks);
      // Report the time taken to convert the tree to JSON.
      try {
        Services.telemetry
                .getHistogramById("PLACES_BACKUPS_TOJSON_MS")
                .add(Date.now() - startTime);
      } catch (ex) {
        Components.utils.reportError("Unable to report telemetry.");
      }

      // Write to the temp folder first, to avoid leaving back partial files.
      let tmpPath = OS.Path.join(OS.Constants.Path.tmpDir,
                                 OS.Path.basename(aFilePath) + ".tmp");
      yield OS.File.writeAtomic(aFilePath, jsonString, { tmpPath: tmpPath });
      return count;
    });
  },

  /**
   * Takes a JSON-serialized node and inserts it into the db.
   *
   * @param aData
   *        The unwrapped data blob of dropped or pasted data.
   * @param aContainer
   *        The container the data was dropped or pasted into
   * @param aIndex
   *        The index within the container the item was dropped or pasted at
   * @return {Promise}
   * @resolves an array containing of maps of old folder ids to new folder ids,
   *           and an array of saved search ids that need to be fixed up.
   *           eg: [[[oldFolder1, newFolder1]], [search1]]
   * @rejects JavaScript exception.
   */
  importJSONNode: function BJU_importJSONNode(aData, aContainer, aIndex,
                                              aGrandParentId) {
    let importer = new BookmarkImporter();
    // Can't make importJSONNode a task until we have made the
    // runInBatchMode in BI_importFromJSON to run asynchronously. Bug 890203
    return Promise.resolve(importer.importJSONNode(aData, aContainer, aIndex, aGrandParentId));
  },

  /**
   * Serializes the given node (and all its descendents) as JSON
   * and writes the serialization to the given output stream.
   *
   * @param   aNode
   *          An nsINavHistoryResultNode
   * @param   aStream
   *          An nsIOutputStream. NOTE: it only uses the write(str, len)
   *          method of nsIOutputStream. The caller is responsible for
   *          closing the stream.
   * @param   aIsUICommand
   *          Boolean - If true, modifies serialization so that each node self-contained.
   *          For Example, tags are serialized inline with each bookmark.
   * @param   aResolveShortcuts
   *          Converts folder shortcuts into actual folders.
   * @param   aExcludeItems
   *          An array of item ids that should not be written to the backup.
   * @return {Promise}
   * @resolves When node have been serialized and wrote to output stream.
   * @rejects JavaScript exception.
   */
  serializeNodeAsJSONToOutputStream: function BJU_serializeNodeAsJSONToOutputStream(
    aNode, aStream, aIsUICommand, aResolveShortcuts, aExcludeItems) {
    let deferred = Promise.defer();
    Services.tm.mainThread.dispatch(function() {
      try {
        BookmarkNode.serializeAsJSONToOutputStream(
          aNode, aStream, aIsUICommand, aResolveShortcuts, aExcludeItems);
        deferred.resolve();
      } catch (ex) {
        deferred.reject(ex);
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
    return deferred.promise;
  }
});

function BookmarkImporter() {}
BookmarkImporter.prototype = {
  /**
   * Import bookmarks from a file.
   *
   * @param aFile
   *        the bookmark file.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromFile: function(aFile, aReplace) {
    if (aFile.exists()) {
      return this.importFromURL(NetUtil.newURI(aFile).spec, aReplace);
    }

    notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_BEGIN);

    return Task.spawn(function() {
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED);
      throw new Error("File does not exist.");
    });
  },

  /**
   * Import bookmarks from a url.
   *
   * @param aURL
   *        url of the bookmark data.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromURL: function BI_importFromURL(aURL, aReplace) {
    let deferred = Promise.defer();
    notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_BEGIN);

    let streamObserver = {
      onStreamComplete: function (aLoader, aContext, aStatus, aLength,
                                  aResult) {
        let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = "UTF-8";

        Task.spawn(function() {
          try {
            let jsonString =
              converter.convertFromByteArray(aResult, aResult.length);
            yield this.importFromJSON(jsonString, aReplace);
            notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_SUCCESS);
            deferred.resolve();
          } catch (ex) {
            notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED);
            Cu.reportError("Failed to import from URL: " + ex);
            deferred.reject(ex);
          }
        }.bind(this));
      }.bind(this)
    };

    try {
      let channel = Services.io.newChannelFromURI(NetUtil.newURI(aURL));
      let streamLoader = Cc["@mozilla.org/network/stream-loader;1"].
                         createInstance(Ci.nsIStreamLoader);

      streamLoader.init(streamObserver);
      channel.asyncOpen(streamLoader, channel);
    } catch (ex) {
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED);
      Cu.reportError("Failed to import from URL: " + ex);
      deferred.reject(ex);
    }

    return deferred.promise;
  },

  /**
   * Import bookmarks from a JSON string.
   *
   * @param aString
   *        JSON string of serialized bookmark data.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   */
  importFromJSON: function BI_importFromJSON(aString, aReplace) {
    let deferred = Promise.defer();
    let nodes =
      PlacesUtils.unwrapNodes(aString, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);

    if (nodes.length == 0 || !nodes[0].children ||
        nodes[0].children.length == 0) {
      Services.tm.mainThread.dispatch(function() {
        deferred.resolve(); // Nothing to restore
      }, Ci.nsIThread.DISPATCH_NORMAL);
    } else {
      // Ensure tag folder gets processed last
      nodes[0].children.sort(function sortRoots(aNode, bNode) {
        return (aNode.root && aNode.root == "tagsFolder") ? 1 :
               (bNode.root && bNode.root == "tagsFolder") ? -1 : 0;
      });

      let batch = {
        nodes: nodes[0].children,
        runBatched: function runBatched() {
          if (aReplace) {
            // Get roots excluded from the backup, we will not remove them
            // before restoring.
            let excludeItems = PlacesUtils.annotations.getItemsWithAnnotation(
                                 PlacesUtils.EXCLUDE_FROM_BACKUP_ANNO);
            // Delete existing children of the root node, excepting:
            // 1. special folders: delete the child nodes
            // 2. tags folder: untag via the tagging api
            let root = PlacesUtils.getFolderContents(PlacesUtils.placesRootId,
                                                   false, false).root;
            let childIds = [];
            for (let i = 0; i < root.childCount; i++) {
              let childId = root.getChild(i).itemId;
              if (excludeItems.indexOf(childId) == -1 &&
                  childId != PlacesUtils.tagsFolderId) {
                childIds.push(childId);
              }
            }
            root.containerOpen = false;

            for (let i = 0; i < childIds.length; i++) {
              let rootItemId = childIds[i];
              if (PlacesUtils.isRootItem(rootItemId)) {
                PlacesUtils.bookmarks.removeFolderChildren(rootItemId);
              } else {
                PlacesUtils.bookmarks.removeItem(rootItemId);
              }
            }
          }

          let searchIds = [];
          let folderIdMap = [];

          for (let node of batch.nodes) {
            if (!node.children || node.children.length == 0)
              continue; // Nothing to restore for this root

            if (node.root) {
              let container = PlacesUtils.placesRootId; // Default to places root
              switch (node.root) {
                case "bookmarksMenuFolder":
                  container = PlacesUtils.bookmarksMenuFolderId;
                  break;
                case "tagsFolder":
                  container = PlacesUtils.tagsFolderId;
                  break;
                case "unfiledBookmarksFolder":
                  container = PlacesUtils.unfiledBookmarksFolderId;
                  break;
                case "toolbarFolder":
                  container = PlacesUtils.toolbarFolderId;
                  break;
              }

              // Insert the data into the db
              for (let child of node.children) {
                let index = child.index;
                let [folders, searches] =
                  this.importJSONNode(child, container, index, 0);
                for (let i = 0; i < folders.length; i++) {
                  if (folders[i])
                    folderIdMap[i] = folders[i];
                }
                searchIds = searchIds.concat(searches);
              }
            } else {
              this.importJSONNode(
                node, PlacesUtils.placesRootId, node.index, 0);
            }
          }

          // Fixup imported place: uris that contain folders
          searchIds.forEach(function(aId) {
            let oldURI = PlacesUtils.bookmarks.getBookmarkURI(aId);
            let uri = fixupQuery(oldURI, folderIdMap);
            if (!uri.equals(oldURI)) {
              PlacesUtils.bookmarks.changeBookmarkURI(aId, uri);
            }
          });

          deferred.resolve();
        }.bind(this)
      };

      PlacesUtils.bookmarks.runInBatchMode(batch, null);
    }
    return deferred.promise;
  },

  /**
   * Takes a JSON-serialized node and inserts it into the db.
   *
   * @param aData
   *        The unwrapped data blob of dropped or pasted data.
   * @param aContainer
   *        The container the data was dropped or pasted into
   * @param aIndex
   *        The index within the container the item was dropped or pasted at
   * @return an array containing of maps of old folder ids to new folder ids,
   *         and an array of saved search ids that need to be fixed up.
   *         eg: [[[oldFolder1, newFolder1]], [search1]]
   */
  importJSONNode: function BI_importJSONNode(aData, aContainer, aIndex,
                                             aGrandParentId) {
    let folderIdMap = [];
    let searchIds = [];
    let id = -1;
    switch (aData.type) {
      case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
        if (aContainer == PlacesUtils.tagsFolderId) {
          // Node is a tag
          if (aData.children) {
            aData.children.forEach(function(aChild) {
              try {
                PlacesUtils.tagging.tagURI(
                  NetUtil.newURI(aChild.uri), [aData.title]);
              } catch (ex) {
                // Invalid tag child, skip it
              }
            });
            return [folderIdMap, searchIds];
          }
        } else if (aData.annos &&
                   aData.annos.some(anno => anno.name == PlacesUtils.LMANNO_FEEDURI)) {
          // Node is a livemark
          let feedURI = null;
          let siteURI = null;
          aData.annos = aData.annos.filter(function(aAnno) {
            switch (aAnno.name) {
              case PlacesUtils.LMANNO_FEEDURI:
                feedURI = NetUtil.newURI(aAnno.value);
                return false;
              case PlacesUtils.LMANNO_SITEURI:
                siteURI = NetUtil.newURI(aAnno.value);
                return false;
              default:
                return true;
            }
          });

          if (feedURI) {
            PlacesUtils.livemarks.addLivemark({
              title: aData.title,
              feedURI: feedURI,
              parentId: aContainer,
              index: aIndex,
              lastModified: aData.lastModified,
              siteURI: siteURI
            }, function(aStatus, aLivemark) {
              if (Components.isSuccessCode(aStatus)) {
                let id = aLivemark.id;
                if (aData.dateAdded)
                  PlacesUtils.bookmarks.setItemDateAdded(id, aData.dateAdded);
                if (aData.annos && aData.annos.length)
                  PlacesUtils.setAnnotationsForItem(id, aData.annos);
              }
            });
          }
        } else {
          id = PlacesUtils.bookmarks.createFolder(
                 aContainer, aData.title, aIndex);
          folderIdMap[aData.id] = id;
          // Process children
          if (aData.children) {
            for (let i = 0; i < aData.children.length; i++) {
              let child = aData.children[i];
              let [folders, searches] =
                this.importJSONNode(child, id, i, aContainer);
              for (let j = 0; j < folders.length; j++) {
                if (folders[j])
                  folderIdMap[j] = folders[j];
              }
              searchIds = searchIds.concat(searches);
            }
          }
        }
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE:
        id = PlacesUtils.bookmarks.insertBookmark(
               aContainer, NetUtil.newURI(aData.uri), aIndex, aData.title);
        if (aData.keyword)
          PlacesUtils.bookmarks.setKeywordForBookmark(id, aData.keyword);
        if (aData.tags) {
          // TODO (bug 967196) the tagging service should trim by itself.
          let tags = aData.tags.split(",").map(tag => tag.trim());
          if (tags.length)
            PlacesUtils.tagging.tagURI(NetUtil.newURI(aData.uri), tags);
        }
        if (aData.charset) {
          PlacesUtils.annotations.setPageAnnotation(
            NetUtil.newURI(aData.uri), PlacesUtils.CHARSET_ANNO, aData.charset,
            0, Ci.nsIAnnotationService.EXPIRE_NEVER);
        }
        if (aData.uri.substr(0, 6) == "place:")
          searchIds.push(id);
        if (aData.icon) {
          try {
            // Create a fake faviconURI to use (FIXME: bug 523932)
            let faviconURI = NetUtil.newURI("fake-favicon-uri:" + aData.uri);
            PlacesUtils.favicons.replaceFaviconDataFromDataURL(
              faviconURI, aData.icon, 0);
            PlacesUtils.favicons.setAndFetchFaviconForPage(
              NetUtil.newURI(aData.uri), faviconURI, false,
              PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
          } catch (ex) {
            Components.utils.reportError("Failed to import favicon data:" + ex);
          }
        }
        if (aData.iconUri) {
          try {
            PlacesUtils.favicons.setAndFetchFaviconForPage(
              NetUtil.newURI(aData.uri), NetUtil.newURI(aData.iconUri), false,
              PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
          } catch (ex) {
            Components.utils.reportError("Failed to import favicon URI:" + ex);
          }
        }
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
        id = PlacesUtils.bookmarks.insertSeparator(aContainer, aIndex);
        break;
      default:
        // Unknown node type
    }

    // Set generic properties, valid for all nodes
    if (id != -1 && aContainer != PlacesUtils.tagsFolderId &&
        aGrandParentId != PlacesUtils.tagsFolderId) {
      if (aData.dateAdded)
        PlacesUtils.bookmarks.setItemDateAdded(id, aData.dateAdded);
      if (aData.lastModified)
        PlacesUtils.bookmarks.setItemLastModified(id, aData.lastModified);
      if (aData.annos && aData.annos.length)
        PlacesUtils.setAnnotationsForItem(id, aData.annos);
    }

    return [folderIdMap, searchIds];
  }
}

function notifyObservers(topic) {
  Services.obs.notifyObservers(null, topic, "json");
}

/**
 * Replaces imported folder ids with their local counterparts in a place: URI.
 *
 * @param   aURI
 *          A place: URI with folder ids.
 * @param   aFolderIdMap
 *          An array mapping old folder id to new folder ids.
 * @returns the fixed up URI if all matched. If some matched, it returns
 *          the URI with only the matching folders included. If none matched
 *          it returns the input URI unchanged.
 */
function fixupQuery(aQueryURI, aFolderIdMap) {
  let convert = function(str, p1, offset, s) {
    return "folder=" + aFolderIdMap[p1];
  }
  let stringURI = aQueryURI.spec.replace(/folder=([0-9]+)/g, convert);

  return NetUtil.newURI(stringURI);
}

let BookmarkNode = {
  /**
   * Serializes the given node (and all its descendents) as JSON
   * and writes the serialization to the given output stream.
   *
   * @param   aNode
   *          An nsINavHistoryResultNode
   * @param   aStream
   *          An nsIOutputStream. NOTE: it only uses the write(str, len)
   *          method of nsIOutputStream. The caller is responsible for
   *          closing the stream.
   * @param   aIsUICommand
   *          Boolean - If true, modifies serialization so that each node self-contained.
   *          For Example, tags are serialized inline with each bookmark.
   * @param   aResolveShortcuts
   *          Converts folder shortcuts into actual folders.
   * @param   aExcludeItems
   *          An array of item ids that should not be written to the backup.
   * @returns Task promise
   * @resolves the number of serialized uri nodes.
   */
  serializeAsJSONToOutputStream: function BN_serializeAsJSONToOutputStream(
    aNode, aStream, aIsUICommand, aResolveShortcuts, aExcludeItems) {

    return Task.spawn(function* () {
      // Serialize to stream
      let array = [];
      let result = yield this._appendConvertedNode(aNode, null, array,
                                                   aIsUICommand,
                                                   aResolveShortcuts,
                                                   aExcludeItems);
      if (result.appendedNode) {
        let jsonString = JSON.stringify(array[0]);
        aStream.write(jsonString, jsonString.length);
      } else {
        throw Cr.NS_ERROR_UNEXPECTED;
      }
      return result.nodeCount;
    }.bind(this));
  },

  _appendConvertedNode: function BN__appendConvertedNode(
    bNode, aIndex, aArray, aIsUICommand, aResolveShortcuts, aExcludeItems) {
    return Task.spawn(function* () {
      let node = {};
      let nodeCount = 0;

      // Set index in order received
      // XXX handy shortcut, but are there cases where we don't want
      // to export using the sorting provided by the query?
      if (aIndex)
        node.index = aIndex;

      this._addGenericProperties(bNode, node, aResolveShortcuts);

      let parent = bNode.parent;
      let grandParent = parent ? parent.parent : null;

      if (PlacesUtils.nodeIsURI(bNode)) {
        // Tag root accept only folder nodes
        if (parent && parent.itemId == PlacesUtils.tagsFolderId)
          return { appendedNode: false, nodeCount: nodeCount };

        // Check for url validity, since we can't halt while writing a backup.
        // This will throw if we try to serialize an invalid url and it does
        // not make sense saving a wrong or corrupt uri node.
        try {
          NetUtil.newURI(bNode.uri);
        } catch (ex) {
          return { appendedNode: false, nodeCount: nodeCount };
        }

        yield this._addURIProperties(bNode, node, aIsUICommand);
        nodeCount++;
      } else if (PlacesUtils.nodeIsContainer(bNode)) {
        // Tag containers accept only uri nodes
        if (grandParent && grandParent.itemId == PlacesUtils.tagsFolderId)
          return { appendedNode: false, nodeCount: nodeCount };

        this._addContainerProperties(bNode, node, aIsUICommand,
                                     aResolveShortcuts);
      } else if (PlacesUtils.nodeIsSeparator(bNode)) {
        // Tag root accept only folder nodes
        // Tag containers accept only uri nodes
        if ((parent && parent.itemId == PlacesUtils.tagsFolderId) ||
            (grandParent && grandParent.itemId == PlacesUtils.tagsFolderId))
          return { appendedNode: false, nodeCount: nodeCount };

        this._addSeparatorProperties(bNode, node);
      }

      if (!node.feedURI && node.type == PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER) {
        nodeCount += yield this._appendConvertedComplexNode(node,
                                                           bNode,
                                                           aArray,
                                                           aIsUICommand,
                                                           aResolveShortcuts,
                                                           aExcludeItems)
        return { appendedNode: true, nodeCount: nodeCount };
      }

      aArray.push(node);
      return { appendedNode: true, nodeCount: nodeCount };
    }.bind(this));
  },

  _addGenericProperties: function BN__addGenericProperties(
    aPlacesNode, aJSNode, aResolveShortcuts) {
    aJSNode.title = aPlacesNode.title;
    aJSNode.id = aPlacesNode.itemId;
    if (aJSNode.id != -1) {
      let parent = aPlacesNode.parent;
      if (parent)
        aJSNode.parent = parent.itemId;
      let dateAdded = aPlacesNode.dateAdded;
      if (dateAdded)
        aJSNode.dateAdded = dateAdded;
      let lastModified = aPlacesNode.lastModified;
      if (lastModified)
        aJSNode.lastModified = lastModified;

      // XXX need a hasAnnos api
      let annos = [];
      try {
        annos =
          PlacesUtils.getAnnotationsForItem(aJSNode.id).filter(function(anno) {
          // XXX should whitelist this instead, w/ a pref for
          // backup/restore of non-whitelisted annos
          // XXX causes JSON encoding errors, so utf-8 encode
          // anno.value = unescape(encodeURIComponent(anno.value));
          if (anno.name == PlacesUtils.READ_ONLY_ANNO && aResolveShortcuts) {
            // When copying a read-only node, remove the read-only annotation.
            return false;
          }
          return true;
        });
      } catch(ex) {}
      if (annos.length != 0)
        aJSNode.annos = annos;
    }
    // XXXdietrich - store annos for non-bookmark items
  },

  _addURIProperties: function BN__addURIProperties(
    aPlacesNode, aJSNode, aIsUICommand) {
    return Task.spawn(function() {
      aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE;
      aJSNode.uri = aPlacesNode.uri;
      if (aJSNode.id && aJSNode.id != -1) {
        // Harvest bookmark-specific properties
        let keyword = PlacesUtils.bookmarks.getKeywordForBookmark(aJSNode.id);
        if (keyword)
          aJSNode.keyword = keyword;
      }

      let tags = aIsUICommand ? aPlacesNode.tags : null;
      if (tags)
        aJSNode.tags = tags;

      // Last character-set
      let uri = NetUtil.newURI(aPlacesNode.uri);
      let lastCharset = yield PlacesUtils.getCharsetForURI(uri)
      if (lastCharset)
        aJSNode.charset = lastCharset;
    });
  },

  _addSeparatorProperties: function BN__addSeparatorProperties(
    aPlacesNode, aJSNode) {
    aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR;
  },

  _addContainerProperties: function BN__addContainerProperties(
    aPlacesNode, aJSNode, aIsUICommand, aResolveShortcuts) {
    let concreteId = PlacesUtils.getConcreteItemId(aPlacesNode);
    if (concreteId != -1) {
      // This is a bookmark or a tag container.
      if (PlacesUtils.nodeIsQuery(aPlacesNode) ||
          (concreteId != aPlacesNode.itemId && !aResolveShortcuts)) {
        aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE;
        aJSNode.uri = aPlacesNode.uri;
        // Folder shortcut
        if (aIsUICommand)
          aJSNode.concreteId = concreteId;
      } else {
        // Bookmark folder or a shortcut we should convert to folder.
        aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER;

        // Mark root folders.
        if (aJSNode.id == PlacesUtils.placesRootId)
          aJSNode.root = "placesRoot";
        else if (aJSNode.id == PlacesUtils.bookmarksMenuFolderId)
          aJSNode.root = "bookmarksMenuFolder";
        else if (aJSNode.id == PlacesUtils.tagsFolderId)
          aJSNode.root = "tagsFolder";
        else if (aJSNode.id == PlacesUtils.unfiledBookmarksFolderId)
          aJSNode.root = "unfiledBookmarksFolder";
        else if (aJSNode.id == PlacesUtils.toolbarFolderId)
          aJSNode.root = "toolbarFolder";
      }
    } else {
      // This is a grouped container query, generated on the fly.
      aJSNode.type = PlacesUtils.TYPE_X_MOZ_PLACE;
      aJSNode.uri = aPlacesNode.uri;
    }
  },

  _appendConvertedComplexNode: function BN__appendConvertedComplexNode(
    aNode, aSourceNode, aArray, aIsUICommand, aResolveShortcuts,
    aExcludeItems) {
    return Task.spawn(function* () {
      let repr = {};
      let nodeCount = 0;

      for (let [name, value] in Iterator(aNode))
        repr[name] = value;

      // Write child nodes
      let children = repr.children = [];
      if (!aNode.annos ||
          !aNode.annos.some(anno => anno.name == PlacesUtils.LMANNO_FEEDURI)) {
        PlacesUtils.asContainer(aSourceNode);
        let wasOpen = aSourceNode.containerOpen;
        if (!wasOpen)
          aSourceNode.containerOpen = true;
        let cc = aSourceNode.childCount;
        for (let i = 0; i < cc; ++i) {
          let childNode = aSourceNode.getChild(i);
          if (aExcludeItems && aExcludeItems.indexOf(childNode.itemId) != -1)
            continue;
          let result = yield this._appendConvertedNode(aSourceNode.getChild(i), i, children,
                                                       aIsUICommand, aResolveShortcuts,
                                                       aExcludeItems);
          nodeCount += result.nodeCount;
        }
        if (!wasOpen)
          aSourceNode.containerOpen = false;
      }

      aArray.push(repr);
      return nodeCount;
    }.bind(this));
  }
}
