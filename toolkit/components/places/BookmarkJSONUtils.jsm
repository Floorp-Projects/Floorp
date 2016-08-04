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
Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesBackups",
  "resource://gre/modules/PlacesBackups.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

XPCOMUtils.defineLazyGetter(this, "gTextDecoder", () => new TextDecoder());
XPCOMUtils.defineLazyGetter(this, "gTextEncoder", () => new TextEncoder());

/**
 * Generates an hash for the given string.
 *
 * @note The generated hash is returned in base64 form.  Mind the fact base64
 * is case-sensitive if you are going to reuse this code.
 */
function generateHash(aString) {
  let cryptoHash = Cc["@mozilla.org/security/hash;1"]
                     .createInstance(Ci.nsICryptoHash);
  cryptoHash.init(Ci.nsICryptoHash.MD5);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
  stringStream.data = aString;
  cryptoHash.updateFromStream(stringStream, -1);
  // base64 allows the '/' char, but we can't use it for filenames.
  return cryptoHash.finish(true).replace(/\//g, "-");
}

this.BookmarkJSONUtils = Object.freeze({
  /**
   * Import bookmarks from a url.
   *
   * @param aSpec
   *        url of the bookmark data.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromURL: function BJU_importFromURL(aSpec, aReplace) {
    return Task.spawn(function* () {
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_BEGIN);
      try {
        let importer = new BookmarkImporter(aReplace);
        yield importer.importFromURL(aSpec);

        notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_SUCCESS);
      } catch (ex) {
        Cu.reportError("Failed to restore bookmarks from " + aSpec + ": " + ex);
        notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED);
      }
    });
  },

  /**
   * Restores bookmarks and tags from a JSON file.
   * @note any item annotated with "places/excludeFromBackup" won't be removed
   *       before executing the restore.
   *
   * @param aFilePath
   *        OS.File path string of bookmarks in JSON or JSONlz4 format to be restored.
   * @param aReplace
   *        Boolean if true, replace existing bookmarks, else merge.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   * @deprecated passing an nsIFile is deprecated
   */
  importFromFile: function BJU_importFromFile(aFilePath, aReplace) {
    if (aFilePath instanceof Ci.nsIFile) {
      Deprecated.warning("Passing an nsIFile to BookmarksJSONUtils.importFromFile " +
                         "is deprecated. Please use an OS.File path string instead.",
                         "https://developer.mozilla.org/docs/JavaScript_OS.File");
      aFilePath = aFilePath.path;
    }

    return Task.spawn(function* () {
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_BEGIN);
      try {
        if (!(yield OS.File.exists(aFilePath)))
          throw new Error("Cannot restore from nonexisting json file");

        let importer = new BookmarkImporter(aReplace);
        if (aFilePath.endsWith("jsonlz4")) {
          yield importer.importFromCompressedFile(aFilePath);
        } else {
          yield importer.importFromURL(OS.Path.toFileURI(aFilePath));
        }
        notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_SUCCESS);
      } catch (ex) {
        Cu.reportError("Failed to restore bookmarks from " + aFilePath + ": " + ex);
        notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED);
        throw ex;
      }
    });
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file path.
   *
   * @param aFilePath
   *        OS.File path string for the bookmarks file to be created.
   * @param [optional] aOptions
   *        Object containing options for the export:
   *         - failIfHashIs: if the generated file would have the same hash
   *                         defined here, will reject with ex.becauseSameHash
   *         - compress: if true, writes file using lz4 compression
   * @return {Promise}
   * @resolves once the file has been created, to an object with the
   *           following properties:
   *            - count: number of exported bookmarks
   *            - hash: file hash for contents comparison
   * @rejects JavaScript exception.
   * @deprecated passing an nsIFile is deprecated
   */
  exportToFile: function BJU_exportToFile(aFilePath, aOptions={}) {
    if (aFilePath instanceof Ci.nsIFile) {
      Deprecated.warning("Passing an nsIFile to BookmarksJSONUtils.exportToFile " +
                         "is deprecated. Please use an OS.File path string instead.",
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

      let hash = generateHash(jsonString);

      if (hash === aOptions.failIfHashIs) {
        let e = new Error("Hash conflict");
        e.becauseSameHash = true;
        throw e;
      }

      // Do not write to the tmp folder, otherwise if it has a different
      // filesystem writeAtomic will fail.  Eventual dangling .tmp files should
      // be cleaned up by the caller.
      let writeOptions = { tmpPath: OS.Path.join(aFilePath + ".tmp") };
      if (aOptions.compress)
        writeOptions.compression = "lz4";

      yield OS.File.writeAtomic(aFilePath, jsonString, writeOptions);
      return { count: count, hash: hash };
    });
  }
});

function BookmarkImporter(aReplace) {
  this._replace = aReplace;
}
BookmarkImporter.prototype = {
  /**
   * Import bookmarks from a url.
   *
   * @param aSpec
   *        url of the bookmark data.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromURL(spec) {
    return new Promise((resolve, reject) => {
      let streamObserver = {
        onStreamComplete: (aLoader, aContext, aStatus, aLength, aResult) => {
          let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                          createInstance(Ci.nsIScriptableUnicodeConverter);
          converter.charset = "UTF-8";
          try {
            let jsonString = converter.convertFromByteArray(aResult,
                                                            aResult.length);
            resolve(this.importFromJSON(jsonString));
          } catch (ex) {
            Cu.reportError("Failed to import from URL: " + ex);
            reject(ex);
          }
        }
      };

      let uri = NetUtil.newURI(spec);
      let channel = NetUtil.newChannel({
        uri: uri,
        loadUsingSystemPrincipal: true
      });
      let streamLoader = Cc["@mozilla.org/network/stream-loader;1"]
                           .createInstance(Ci.nsIStreamLoader);
      streamLoader.init(streamObserver);
      channel.asyncOpen2(streamLoader);
    });
  },

  /**
   * Import bookmarks from a compressed file.
   *
   * @param aFilePath
   *        OS.File path string of the bookmark data.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  importFromCompressedFile: function* BI_importFromCompressedFile(aFilePath) {
      let aResult = yield OS.File.read(aFilePath, { compression: "lz4" });
      let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                        createInstance(Ci.nsIScriptableUnicodeConverter);
      converter.charset = "UTF-8";
      let jsonString = converter.convertFromByteArray(aResult, aResult.length);
      yield this.importFromJSON(jsonString);
  },

  /**
   * Import bookmarks from a JSON string.
   *
   * @param aString
   *        JSON string of serialized bookmark data.
   */
  importFromJSON: Task.async(function* (aString) {
    this._importPromises = [];
    let deferred = PromiseUtils.defer();
    let nodes =
      PlacesUtils.unwrapNodes(aString, PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER);

    if (nodes.length == 0 || !nodes[0].children ||
        nodes[0].children.length == 0) {
      deferred.resolve(); // Nothing to restore
    } else {
      // Ensure tag folder gets processed last
      nodes[0].children.sort(function sortRoots(aNode, bNode) {
        if (aNode.root && aNode.root == "tagsFolder")
          return 1;
        if (bNode.root && bNode.root == "tagsFolder")
          return -1;
        return 0;
      });

      let batch = {
        nodes: nodes[0].children,
        runBatched: function runBatched() {
          if (this._replace) {
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
              if (!excludeItems.includes(childId) &&
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
    yield deferred.promise;
    // TODO (bug 1095426) once converted to the new bookmarks API, methods will
    // yield, so this hack should not be needed anymore.
    try {
      yield Promise.all(this._importPromises);
    } finally {
      delete this._importPromises;
    }
  }),

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
            let lmPromise = PlacesUtils.livemarks.addLivemark({
              title: aData.title,
              feedURI: feedURI,
              parentId: aContainer,
              index: aIndex,
              lastModified: aData.lastModified,
              siteURI: siteURI,
              guid: aData.guid
            }).then(function (aLivemark) {
              let id = aLivemark.id;
              if (aData.dateAdded)
                PlacesUtils.bookmarks.setItemDateAdded(id, aData.dateAdded);
              if (aData.annos && aData.annos.length)
                PlacesUtils.setAnnotationsForItem(id, aData.annos);
            });
            this._importPromises.push(lmPromise);
          }
        } else {
          id = PlacesUtils.bookmarks.createFolder(
                 aContainer, aData.title, aIndex, aData.guid);
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
               aContainer, NetUtil.newURI(aData.uri), aIndex, aData.title, aData.guid);
        if (aData.keyword) {
          // POST data could be set in 2 ways:
          // 1. new backups have a postData property
          // 2. old backups have an item annotation
          let postDataAnno = aData.annos &&
                             aData.annos.find(anno => anno.name == PlacesUtils.POST_DATA_ANNO);
          let postData = aData.postData || (postDataAnno && postDataAnno.value);
          let kwPromise = PlacesUtils.keywords.insert({ keyword: aData.keyword,
                                                        url: aData.uri,
                                                        postData });
          this._importPromises.push(kwPromise);
        }
        if (aData.tags) {
          let tags = aData.tags.split(",").filter(aTag =>
            aTag.length <= Ci.nsITaggingService.MAX_TAG_LENGTH);
          if (tags.length) {
            try {
              PlacesUtils.tagging.tagURI(NetUtil.newURI(aData.uri), tags);
            } catch (ex) {
              // Invalid tag child, skip it.
              Cu.reportError(`Unable to set tags "${tags.join(", ")}" for ${aData.uri}: ${ex}`);
            }
          }
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
              faviconURI, aData.icon, 0,
              Services.scriptSecurityManager.getSystemPrincipal());
            PlacesUtils.favicons.setAndFetchFaviconForPage(
              NetUtil.newURI(aData.uri), faviconURI, false,
              PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, null,
              Services.scriptSecurityManager.getSystemPrincipal());
          } catch (ex) {
            Components.utils.reportError("Failed to import favicon data:" + ex);
          }
        }
        if (aData.iconUri) {
          try {
            PlacesUtils.favicons.setAndFetchFaviconForPage(
              NetUtil.newURI(aData.uri), NetUtil.newURI(aData.iconUri), false,
              PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, null,
              Services.scriptSecurityManager.getSystemPrincipal());
          } catch (ex) {
            Components.utils.reportError("Failed to import favicon URI:" + ex);
          }
        }
        break;
      case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
        id = PlacesUtils.bookmarks.insertSeparator(aContainer, aIndex, aData.guid);
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
