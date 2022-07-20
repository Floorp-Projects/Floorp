/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PlacesUtils } from "resource://gre/modules/PlacesUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesBackups: "resource://gre/modules/PlacesBackups.sys.mjs",
});

// This is used to translate old folder pseudonyms in queries with their newer
// guids.
const OLD_BOOKMARK_QUERY_TRANSLATIONS = {
  PLACES_ROOT: PlacesUtils.bookmarks.rootGuid,
  BOOKMARKS_MENU: PlacesUtils.bookmarks.menuGuid,
  TAGS: PlacesUtils.bookmarks.tagsGuid,
  UNFILED_BOOKMARKS: PlacesUtils.bookmarks.unfiledGuid,
  TOOLBAR: PlacesUtils.bookmarks.toolbarGuid,
  MOBILE_BOOKMARKS: PlacesUtils.bookmarks.mobileGuid,
};

/**
 * Generates an hash for the given string.
 *
 * @note The generated hash is returned in base64 form.  Mind the fact base64
 * is case-sensitive if you are going to reuse this code.
 */
function generateHash(aString) {
  let cryptoHash = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  cryptoHash.init(Ci.nsICryptoHash.MD5);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stringStream.setUTF8Data(aString);
  cryptoHash.updateFromStream(stringStream, -1);
  // base64 allows the '/' char, but we can't use it for filenames.
  return cryptoHash.finish(true).replace(/\//g, "-");
}

export var BookmarkJSONUtils = Object.freeze({
  /**
   * Import bookmarks from a url.
   *
   * @param {string} aSpec
   *        url of the bookmark data.
   * @param {boolean} [options.replace]
   *        Whether we should erase existing bookmarks before importing.
   * @param {PlacesUtils.bookmarks.SOURCES} [options.source]
   *        The bookmark change source, used to determine the sync status for
   *        imported bookmarks. Defaults to `RESTORE` if `replace = true`, or
   *        `IMPORT` otherwise.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  async importFromURL(
    aSpec,
    {
      replace: aReplace = false,
      source: aSource = aReplace
        ? PlacesUtils.bookmarks.SOURCES.RESTORE
        : PlacesUtils.bookmarks.SOURCES.IMPORT,
    } = {}
  ) {
    notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_BEGIN, aReplace);
    try {
      let importer = new BookmarkImporter(aReplace, aSource);
      await importer.importFromURL(aSpec);

      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_SUCCESS, aReplace);
    } catch (ex) {
      Cu.reportError("Failed to restore bookmarks from " + aSpec + ": " + ex);
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED, aReplace);
      throw ex;
    }
  },

  /**
   * Restores bookmarks and tags from a JSON file.
   *
   * @param aFilePath
   *        OS.File path string of bookmarks in JSON or JSONlz4 format to be restored.
   * @param [options.replace]
   *        Whether we should erase existing bookmarks before importing.
   * @param [options.source]
   *        The bookmark change source, used to determine the sync status for
   *        imported bookmarks. Defaults to `RESTORE` if `replace = true`, or
   *        `IMPORT` otherwise.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  async importFromFile(
    aFilePath,
    {
      replace: aReplace = false,
      source: aSource = aReplace
        ? PlacesUtils.bookmarks.SOURCES.RESTORE
        : PlacesUtils.bookmarks.SOURCES.IMPORT,
    } = {}
  ) {
    notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_BEGIN, aReplace);
    try {
      if (!(await IOUtils.exists(aFilePath))) {
        throw new Error("Cannot restore from nonexisting json file");
      }

      let importer = new BookmarkImporter(aReplace, aSource);
      if (aFilePath.endsWith("jsonlz4")) {
        await importer.importFromCompressedFile(aFilePath);
      } else {
        await importer.importFromURL(PathUtils.toFileURI(aFilePath));
      }
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_SUCCESS, aReplace);
    } catch (ex) {
      Cu.reportError(
        "Failed to restore bookmarks from " + aFilePath + ": " + ex
      );
      notifyObservers(PlacesUtils.TOPIC_BOOKMARKS_RESTORE_FAILED, aReplace);
      throw ex;
    }
  },

  /**
   * Serializes bookmarks using JSON, and writes to the supplied file path.
   *
   * @param {path} aFilePath
   *   Path string for the bookmarks file to be created.
   * @param {object} [aOptions]
   * @param {string} [failIfHashIs]
   *   If the generated file would have the same hash defined here, will reject
   *   with ex.becauseSameHash
   * @param {boolean} [compress]
   *   If true, writes file using lz4 compression
   * @return {Promise}
   * @resolves once the file has been created, to an object with the
   *           following properties:
   *            - count: number of exported bookmarks
   *            - hash: file hash for contents comparison
   * @rejects JavaScript exception.
   */
  async exportToFile(aFilePath, aOptions = {}) {
    let [bookmarks, count] = await lazy.PlacesBackups.getBookmarksTree();
    let startTime = Date.now();
    let jsonString = JSON.stringify(bookmarks);
    // Report the time taken to convert the tree to JSON.
    try {
      Services.telemetry
        .getHistogramById("PLACES_BACKUPS_TOJSON_MS")
        .add(Date.now() - startTime);
    } catch (ex) {
      Cu.reportError("Unable to report telemetry.");
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
    await IOUtils.writeUTF8(aFilePath, jsonString, {
      compress: aOptions.compress,
      tmpPath: PathUtils.join(aFilePath + ".tmp"),
    });
    return { count, hash };
  },
});

function BookmarkImporter(aReplace, aSource) {
  this._replace = aReplace;
  this._source = aSource;
}
BookmarkImporter.prototype = {
  /**
   * Import bookmarks from a url.
   *
   * @param {string} aSpec
   *        url of the bookmark data.
   *
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  async importFromURL(spec) {
    if (!spec.startsWith("chrome://") && !spec.startsWith("file://")) {
      throw new Error(
        "importFromURL can only be used with chrome:// and file:// URLs"
      );
    }
    let nodes = await (await fetch(spec)).json();

    if (!nodes.children || !nodes.children.length) {
      return;
    }

    await this.import(nodes);
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
  importFromCompressedFile: async function BI_importFromCompressedFile(
    aFilePath
  ) {
    // We read as UTF8 rather than JSON, as PlacesUtils.unwrapNodes expects
    // a JSON string.
    let result = await IOUtils.readUTF8(aFilePath, { decompress: true });
    await this.importFromJSON(result);
  },

  /**
   * Import bookmarks from a JSON string.
   *
   * @param {String} aString JSON string of serialized bookmark data.
   * @return {Promise}
   * @resolves When the new bookmarks have been created.
   * @rejects JavaScript exception.
   */
  async importFromJSON(aString) {
    let nodes = PlacesUtils.unwrapNodes(
      aString,
      PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER
    );

    if (!nodes.length || !nodes[0].children || !nodes[0].children.length) {
      return;
    }

    await this.import(nodes[0]);
  },

  async import(rootNode) {
    // Change to rootNode.children as we don't import the root, and also filter
    // out any obsolete "tagsFolder" sections.
    let nodes = rootNode.children.filter(node => node.root !== "tagsFolder");

    // If we're replacing, then erase existing bookmarks first.
    if (this._replace) {
      await PlacesUtils.bookmarks.eraseEverything({ source: this._source });
    }

    let folderIdToGuidMap = {};

    // Now do some cleanup on the imported nodes so that the various guids
    // match what we need for insertTree, and we also have mappings of folders
    // so we can repair any searches after inserting the bookmarks (see bug 824502).
    for (let node of nodes) {
      if (!node.children || !node.children.length) {
        continue;
      } // Nothing to restore for this root

      // Ensure we set the source correctly.
      node.source = this._source;

      // Translate the node for insertTree.
      let folders = translateTreeTypes(node);

      folderIdToGuidMap = Object.assign(folderIdToGuidMap, folders);
    }

    // Now we can add the actual nodes to the database.
    for (let node of nodes) {
      // Drop any nodes without children, we can't insert them.
      if (!node.children || !node.children.length) {
        continue;
      }

      // Drop any roots whose guid we don't recognise - we don't support anything
      // apart from the built-in roots.
      if (!PlacesUtils.bookmarks.userContentRoots.includes(node.guid)) {
        continue;
      }

      fixupSearchQueries(node, folderIdToGuidMap);

      await PlacesUtils.bookmarks.insertTree(node, {
        fixupOrSkipInvalidEntries: true,
      });

      // Now add any favicons.
      try {
        insertFaviconsForTree(node);
      } catch (ex) {
        Cu.reportError(`Failed to insert favicons: ${ex}`);
      }
    }
  },
};

function notifyObservers(topic, replace) {
  Services.obs.notifyObservers(null, topic, replace ? "json" : "json-append");
}

/**
 * Iterates through a node, fixing up any place: URL queries that are found. This
 * replaces any old (pre Firefox 62) queries that contain "folder=<id>" parts with
 * "parent=<guid>".
 *
 * @param {Object} aNode The node to search.
 * @param {Array} aFolderIdMap An array mapping of old folder IDs to new folder GUIDs.
 */
function fixupSearchQueries(aNode, aFolderIdMap) {
  if (aNode.url && aNode.url.startsWith("place:")) {
    aNode.url = fixupQuery(aNode.url, aFolderIdMap);
  }
  if (aNode.children) {
    for (let child of aNode.children) {
      fixupSearchQueries(child, aFolderIdMap);
    }
  }
}

/**
 * Replaces imported folder ids with their local counterparts in a place: URI.
 *
 * @param   {String} aQueryURL
 *          A place: URI with folder ids.
 * @param   {Object} aFolderIdMap
 *          An array mapping of old folder IDs to new folder GUIDs.
 * @return {String} the fixed up URI if all matched. If some matched, it returns
 *         the URI with only the matching folders included. If none matched
 *         it returns the input URI unchanged.
 */
function fixupQuery(aQueryURL, aFolderIdMap) {
  let invalid = false;
  let convert = function(str, existingFolderId) {
    let guid;
    if (
      Object.keys(OLD_BOOKMARK_QUERY_TRANSLATIONS).includes(existingFolderId)
    ) {
      guid = OLD_BOOKMARK_QUERY_TRANSLATIONS[existingFolderId];
    } else {
      guid = aFolderIdMap[existingFolderId];
      if (!guid) {
        invalid = true;
        return `invalidOldParentId=${existingFolderId}`;
      }
    }
    return `parent=${guid}`;
  };

  let url = aQueryURL.replace(/folder=([A-Za-z0-9_]+)/g, convert);
  if (invalid) {
    // One or more of the folders don't exist, cause an empty query so that
    // we don't try to display the whole database.
    url += "&excludeItems=1";
  }
  return url;
}

/**
 * A mapping of root folder names to Guids. To help fixupRootFolderGuid.
 */
const rootToFolderGuidMap = {
  placesRoot: PlacesUtils.bookmarks.rootGuid,
  bookmarksMenuFolder: PlacesUtils.bookmarks.menuGuid,
  unfiledBookmarksFolder: PlacesUtils.bookmarks.unfiledGuid,
  toolbarFolder: PlacesUtils.bookmarks.toolbarGuid,
  mobileFolder: PlacesUtils.bookmarks.mobileGuid,
};

/**
 * Updates a bookmark node from the json version to the places GUID. This
 * will only change GUIDs for the built-in folders. Other folders will remain
 * unchanged.
 *
 * @param {Object} A bookmark node that is updated with the new GUID if necessary.
 */
function fixupRootFolderGuid(node) {
  if (!node.guid && node.root && node.root in rootToFolderGuidMap) {
    node.guid = rootToFolderGuidMap[node.root];
  }
}

/**
 * Translates the JSON types for a node and its children into Places compatible
 * types. Also handles updating of other parameters e.g. dateAdded and lastModified.
 *
 * @param {Object} node A node to be updated. If it contains children, they will
 *                      be updated as well.
 * @return {Array} An array containing two items:
 *       - {Object} A map of current folder ids to GUIDS
 *       - {Array} An array of GUIDs for nodes that contain query URIs
 */
function translateTreeTypes(node) {
  let folderIdToGuidMap = {};

  // Do the uri fixup first, so we can be consistent in this function.
  if (node.uri) {
    node.url = node.uri;
    delete node.uri;
  }

  switch (node.type) {
    case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
      node.type = PlacesUtils.bookmarks.TYPE_FOLDER;

      // Older type mobile folders have a random guid with an annotation. We need
      // to make sure those go into the proper mobile folder.
      let isMobileFolder =
        node.annos &&
        node.annos.some(anno => anno.name == PlacesUtils.MOBILE_ROOT_ANNO);
      if (isMobileFolder) {
        node.guid = PlacesUtils.bookmarks.mobileGuid;
      } else {
        // In case the Guid is broken, we need to fix it up.
        fixupRootFolderGuid(node);
      }

      // Record the current id and the guid so that we can update any search
      // queries later.
      folderIdToGuidMap[node.id] = node.guid;
      break;
    case PlacesUtils.TYPE_X_MOZ_PLACE:
      node.type = PlacesUtils.bookmarks.TYPE_BOOKMARK;
      break;
    case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
      node.type = PlacesUtils.bookmarks.TYPE_SEPARATOR;
      if ("title" in node) {
        delete node.title;
      }
      break;
    default:
      // No need to throw/reject here, insertTree will remove this node automatically.
      Cu.reportError(`Unexpected bookmark type ${node.type}`);
      break;
  }

  if (node.dateAdded) {
    node.dateAdded = PlacesUtils.toDate(node.dateAdded);
  }

  if (node.lastModified) {
    let lastModified = PlacesUtils.toDate(node.lastModified);
    // Ensure we get a last modified date that's later or equal to the dateAdded
    // so that we don't upset the Bookmarks API.
    if (lastModified >= node.dateAdded) {
      node.lastModified = lastModified;
    } else {
      delete node.lastModified;
    }
  }

  if (node.tags) {
    // Separate any tags into an array, and ignore any that are too long.
    node.tags = node.tags
      .split(",")
      .filter(
        aTag =>
          !!aTag.length && aTag.length <= PlacesUtils.bookmarks.MAX_TAG_LENGTH
      );

    // If we end up with none, then delete the property completely.
    if (!node.tags.length) {
      delete node.tags;
    }
  }

  // Sometimes postData can be null, so delete it to make the validators happy.
  if (node.postData == null) {
    delete node.postData;
  }

  // Now handle any children.
  if (!node.children) {
    return folderIdToGuidMap;
  }

  // First sort the children by index.
  node.children = node.children.sort((a, b) => {
    return a.index - b.index;
  });

  // Now do any adjustments required for the children.
  for (let child of node.children) {
    let folders = translateTreeTypes(child);
    folderIdToGuidMap = Object.assign(folderIdToGuidMap, folders);
  }

  return folderIdToGuidMap;
}

/**
 * Handles inserting favicons into the database for a bookmark node.
 * It is assumed the node has already been inserted into the bookmarks
 * database.
 *
 * @param {Object} node The bookmark node for icons to be inserted.
 */
function insertFaviconForNode(node) {
  if (node.icon) {
    try {
      // Create a fake faviconURI to use (FIXME: bug 523932)
      let faviconURI = Services.io.newURI("fake-favicon-uri:" + node.url);
      PlacesUtils.favicons.replaceFaviconDataFromDataURL(
        faviconURI,
        node.icon,
        0,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
      PlacesUtils.favicons.setAndFetchFaviconForPage(
        Services.io.newURI(node.url),
        faviconURI,
        false,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        null,
        Services.scriptSecurityManager.getSystemPrincipal()
      );
    } catch (ex) {
      Cu.reportError("Failed to import favicon data:" + ex);
    }
  }

  if (!node.iconUri) {
    return;
  }

  try {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      Services.io.newURI(node.url),
      Services.io.newURI(node.iconUri),
      false,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      null,
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  } catch (ex) {
    Cu.reportError("Failed to import favicon URI:" + ex);
  }
}

/**
 * Handles inserting favicons into the database for a bookmark tree - a node
 * and its children.
 *
 * It is assumed the nodes have already been inserted into the bookmarks
 * database.
 *
 * @param {Object} nodeTree The bookmark node tree for icons to be inserted.
 */
function insertFaviconsForTree(nodeTree) {
  insertFaviconForNode(nodeTree);

  if (nodeTree.children) {
    for (let child of nodeTree.children) {
      insertFaviconsForTree(child);
    }
  }
}
