/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);
const { Utils } = ChromeUtils.import("resource://services-sync/util.js");

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "Async",
  "resource://services-common/async.js"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "PlacesSyncUtils",
  "resource://gre/modules/PlacesSyncUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(lazy, ["URLSearchParams"]);

var EXPORTED_SYMBOLS = ["BookmarkValidator", "BookmarkProblemData"];

const QUERY_PROTOCOL = "place:";

function areURLsEqual(a, b) {
  if (a === b) {
    return true;
  }
  if (a.startsWith(QUERY_PROTOCOL) != b.startsWith(QUERY_PROTOCOL)) {
    return false;
  }
  // Tag queries are special because we rewrite them to point to the
  // local tag folder ID. It's expected that the folders won't match,
  // but all other params should.
  let aParams = new lazy.URLSearchParams(a.slice(QUERY_PROTOCOL.length));
  let aType = +aParams.get("type");
  if (aType != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
    return false;
  }
  let bParams = new lazy.URLSearchParams(b.slice(QUERY_PROTOCOL.length));
  let bType = +bParams.get("type");
  if (bType != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
    return false;
  }
  let aKeys = new Set(aParams.keys());
  let bKeys = new Set(bParams.keys());
  if (aKeys.size != bKeys.size) {
    return false;
  }
  // Tag queries shouldn't reference multiple folders, or named folders like
  // "TOOLBAR" or "BOOKMARKS_MENU". Just in case, we make sure all folder IDs
  // are numeric. If they are, we ignore them when comparing the query params.
  if (aKeys.has("folder") && aParams.getAll("folder").every(isFinite)) {
    aKeys.delete("folder");
  }
  if (bKeys.has("folder") && bParams.getAll("folder").every(isFinite)) {
    bKeys.delete("folder");
  }
  for (let key of aKeys) {
    if (!bKeys.has(key)) {
      return false;
    }
    if (
      !CommonUtils.arrayEqual(
        aParams.getAll(key).sort(),
        bParams.getAll(key).sort()
      )
    ) {
      return false;
    }
  }
  for (let key of bKeys) {
    if (!aKeys.has(key)) {
      return false;
    }
  }
  return true;
}

const BOOKMARK_VALIDATOR_VERSION = 1;

/**
 * Result of bookmark validation. Contains the following fields which describe
 * server-side problems unless otherwise specified.
 *
 * - missingIDs (number): # of objects with missing ids
 * - duplicates (array of ids): ids seen more than once
 * - parentChildMismatches (array of {parent: parentid, child: childid}):
 *   instances where the child's parentid and the parent's children array
 *   do not match
 * - cycles (array of array of ids). List of cycles found in the server-side tree.
 * - clientCycles (array of array of ids). List of cycles found in the client-side tree.
 * - orphans (array of {id: string, parent: string}): List of nodes with
 *   either no parentid, or where the parent could not be found.
 * - missingChildren (array of {parent: id, child: id}):
 *   List of parent/children where the child id couldn't be found
 * - deletedChildren (array of { parent: id, child: id }):
 *   List of parent/children where child id was a deleted item (but still showed up
 *   in the children array)
 * - multipleParents (array of {child: id, parents: array of ids}):
 *   List of children that were part of multiple parent arrays
 * - deletedParents (array of ids) : List of records that aren't deleted but
 *   had deleted parents
 * - childrenOnNonFolder (array of ids): list of non-folders that still have
 *   children arrays
 * - duplicateChildren (array of ids): list of records who have the same
 *   child listed multiple times in their children array
 * - parentNotFolder (array of ids): list of records that have parents that
 *   aren't folders
 * - rootOnServer (boolean): true if the root came from the server
 * - badClientRoots (array of ids): Contains any client-side root ids where
 *   the root is missing or isn't a (direct) child of the places root.
 *
 * - clientMissing: Array of ids on the server missing from the client
 * - serverMissing: Array of ids on the client missing from the server
 * - serverDeleted: Array of ids on the client that the server had marked as deleted.
 * - serverUnexpected: Array of ids that appear on the server but shouldn't
 *   because the client attempts to never upload them.
 * - differences: Array of {id: string, differences: string array} recording
 *   the non-structural properties that are differente between the client and server
 * - structuralDifferences: As above, but contains the items where the differences were
 *   structural, that is, they contained childGUIDs or parentid
 */
class BookmarkProblemData {
  constructor() {
    this.rootOnServer = false;
    this.missingIDs = 0;

    this.duplicates = [];
    this.parentChildMismatches = [];
    this.cycles = [];
    this.clientCycles = [];
    this.orphans = [];
    this.missingChildren = [];
    this.deletedChildren = [];
    this.multipleParents = [];
    this.deletedParents = [];
    this.childrenOnNonFolder = [];
    this.duplicateChildren = [];
    this.parentNotFolder = [];

    this.badClientRoots = [];
    this.clientMissing = [];
    this.serverMissing = [];
    this.serverDeleted = [];
    this.serverUnexpected = [];
    this.differences = [];
    this.structuralDifferences = [];
  }

  /**
   * Convert ("difference", [{ differences: ["tags", "name"] }, { differences: ["name"] }]) into
   * [{ name: "difference:tags", count: 1}, { name: "difference:name", count: 2 }], etc.
   */
  _summarizeDifferences(prefix, diffs) {
    let diffCounts = new Map();
    for (let { differences } of diffs) {
      for (let type of differences) {
        let name = prefix + ":" + type;
        let count = diffCounts.get(name) || 0;
        diffCounts.set(name, count + 1);
      }
    }
    return [...diffCounts].map(([name, count]) => ({ name, count }));
  }

  /**
   * Produce a list summarizing problems found. Each entry contains {name, count},
   * where name is the field name for the problem, and count is the number of times
   * the problem was encountered.
   *
   * Validation has failed if all counts are not 0.
   *
   * If the `full` argument is truthy, we also include information about which
   * properties we saw structural differences in. Currently, this means either
   * "sdiff:parentid" and "sdiff:childGUIDS" may be present.
   */
  getSummary(full) {
    let result = [
      { name: "clientMissing", count: this.clientMissing.length },
      { name: "serverMissing", count: this.serverMissing.length },
      { name: "serverDeleted", count: this.serverDeleted.length },
      { name: "serverUnexpected", count: this.serverUnexpected.length },

      {
        name: "structuralDifferences",
        count: this.structuralDifferences.length,
      },
      { name: "differences", count: this.differences.length },

      { name: "missingIDs", count: this.missingIDs },
      { name: "rootOnServer", count: this.rootOnServer ? 1 : 0 },

      { name: "duplicates", count: this.duplicates.length },
      {
        name: "parentChildMismatches",
        count: this.parentChildMismatches.length,
      },
      { name: "cycles", count: this.cycles.length },
      { name: "clientCycles", count: this.clientCycles.length },
      { name: "badClientRoots", count: this.badClientRoots.length },
      { name: "orphans", count: this.orphans.length },
      { name: "missingChildren", count: this.missingChildren.length },
      { name: "deletedChildren", count: this.deletedChildren.length },
      { name: "multipleParents", count: this.multipleParents.length },
      { name: "deletedParents", count: this.deletedParents.length },
      { name: "childrenOnNonFolder", count: this.childrenOnNonFolder.length },
      { name: "duplicateChildren", count: this.duplicateChildren.length },
      { name: "parentNotFolder", count: this.parentNotFolder.length },
    ];
    if (full) {
      let structural = this._summarizeDifferences(
        "sdiff",
        this.structuralDifferences
      );
      result.push.apply(result, structural);
    }
    return result;
  }
}

// Defined lazily to avoid initializing PlacesUtils.bookmarks too soon.
XPCOMUtils.defineLazyGetter(lazy, "SYNCED_ROOTS", () => [
  lazy.PlacesUtils.bookmarks.menuGuid,
  lazy.PlacesUtils.bookmarks.toolbarGuid,
  lazy.PlacesUtils.bookmarks.unfiledGuid,
  lazy.PlacesUtils.bookmarks.mobileGuid,
]);

// Maps root GUIDs to their query folder names from
// toolkit/components/places/nsNavHistoryQuery.cpp. We follow queries that
// reference existing folders in the client tree, and detect cycles where a
// query references its containing folder.
XPCOMUtils.defineLazyGetter(lazy, "ROOT_GUID_TO_QUERY_FOLDER_NAME", () => ({
  [lazy.PlacesUtils.bookmarks.rootGuid]: "PLACES_ROOT",
  [lazy.PlacesUtils.bookmarks.menuGuid]: "BOOKMARKS_MENU",

  // Tags should never show up in our client tree, and never form cycles, but we
  // report them just in case.
  [lazy.PlacesUtils.bookmarks.tagsGuid]: "TAGS",

  [lazy.PlacesUtils.bookmarks.unfiledGuid]: "UNFILED_BOOKMARKS",
  [lazy.PlacesUtils.bookmarks.toolbarGuid]: "TOOLBAR",
  [lazy.PlacesUtils.bookmarks.mobileGuid]: "MOBILE_BOOKMARKS",
}));

async function detectCycles(records) {
  // currentPath and pathLookup contain the same data. pathLookup is faster to
  // query, but currentPath gives is the order of traversal that we need in
  // order to report the members of the cycles.
  let pathLookup = new Set();
  let currentPath = [];
  let cycles = [];
  let seenEver = new Set();
  const yieldState = lazy.Async.yieldState();

  const traverse = async node => {
    if (pathLookup.has(node)) {
      let cycleStart = currentPath.lastIndexOf(node);
      let cyclePath = currentPath.slice(cycleStart).map(n => n.id);
      cycles.push(cyclePath);
      return;
    } else if (seenEver.has(node)) {
      // If we're checking the server, this is a problem, but it should already be reported.
      // On the client, this could happen due to including `node.concrete` in the child list.
      return;
    }
    seenEver.add(node);
    let children = node.children || [];
    if (node.concreteItems) {
      children.push(...node.concreteItems);
    }
    if (children.length) {
      pathLookup.add(node);
      currentPath.push(node);
      await lazy.Async.yieldingForEach(children, traverse, yieldState);
      currentPath.pop();
      pathLookup.delete(node);
    }
  };

  await lazy.Async.yieldingForEach(
    records,
    async record => {
      if (!seenEver.has(record)) {
        await traverse(record);
      }
    },
    yieldState
  );

  return cycles;
}

class ServerRecordInspection {
  constructor() {
    this.serverRecords = null;
    this.liveRecords = [];

    this.folders = [];

    this.root = null;

    this.idToRecord = new Map();

    this.deletedIds = new Set();
    this.deletedRecords = [];

    this.problemData = new BookmarkProblemData();

    // These are handled outside of problemData
    this._orphans = new Map();
    this._multipleParents = new Map();

    this.yieldState = lazy.Async.yieldState();
  }

  static async create(records) {
    return new ServerRecordInspection().performInspection(records);
  }

  async performInspection(records) {
    await this._setRecords(records);
    await this._linkParentIDs();
    await this._linkChildren();
    await this._findOrphans();
    await this._finish();
    return this;
  }

  // We don't set orphans in this.problemData. Instead, we walk the tree at the
  // end to find unreachable items.
  _noteOrphan(id, parentId = undefined) {
    // This probably shouldn't be called with a parentId twice, but if it
    // happens we take the most recent one.
    if (parentId || !this._orphans.has(id)) {
      this._orphans.set(id, parentId);
    }
  }

  noteParent(child, parent) {
    let parents = this._multipleParents.get(child);
    if (!parents) {
      this._multipleParents.set(child, [parent]);
    } else {
      parents.push(parent);
    }
  }

  noteMismatch(child, parent) {
    let exists = this.problemData.parentChildMismatches.some(
      match => match.child == child && match.parent == parent
    );
    if (!exists) {
      this.problemData.parentChildMismatches.push({ child, parent });
    }
  }

  // - Populates `this.deletedIds`, `this.folders`, and `this.idToRecord`
  // - calls `_initRoot` (thus initializing `this.root`).
  async _setRecords(records) {
    if (this.serverRecords) {
      // In general this class is expected to be created, have
      // `performInspection` called, and then only read from from that point on.
      throw new Error("Bug: ServerRecordInspection can't `setRecords` twice");
    }
    this.serverRecords = records;
    let rootChildren = [];

    await lazy.Async.yieldingForEach(
      this.serverRecords,
      async record => {
        if (!record.id) {
          ++this.problemData.missingIDs;
          return;
        }

        if (record.deleted) {
          this.deletedIds.add(record.id);
        }
        if (this.idToRecord.has(record.id)) {
          this.problemData.duplicates.push(record.id);
          return;
        }

        this.idToRecord.set(record.id, record);

        if (!record.deleted) {
          this.liveRecords.push(record);

          if (record.parentid == "places") {
            rootChildren.push(record);
          }
        }

        if (!record.children) {
          return;
        }

        if (record.type != "folder") {
          // Due to implementation details in engines/bookmarks.js, (Livemark
          // subclassing BookmarkFolder) Livemarks will have a children array,
          // but it should still be empty.
          if (!record.children.length) {
            return;
          }
          // Otherwise we mark it as an error and still try to resolve the children
          this.problemData.childrenOnNonFolder.push(record.id);
        }

        this.folders.push(record);

        if (new Set(record.children).size !== record.children.length) {
          this.problemData.duplicateChildren.push(record.id);
        }

        // After we're through with them, folder records store 3 (ugh) arrays that
        // represent their folder information. The final fields looks like:
        //
        // - childGUIDs: The original `children` array, which is an array of
        //   record IDs.
        //
        // - unfilteredChildren: Contains more or less `childGUIDs.map(id =>
        //   idToRecord.get(id))`, without the nulls for missing children. It will
        //   still have deleted, duplicate, mismatching, etc. children.
        //
        // - children: This is the 'cleaned' version of the child records that are
        //   safe to iterate over, etc.. If there are no reported problems, it should
        //   be identical to unfilteredChildren.
        //
        // The last two are left alone until later `this._linkChildren`, however.
        record.childGUIDs = record.children;

        await lazy.Async.yieldingForEach(
          record.childGUIDs,
          id => {
            this.noteParent(id, record.id);
          },
          this.yieldState
        );

        record.children = [];
      },
      this.yieldState
    );

    // Finish up some parts we can easily do now that we have idToRecord.
    this.deletedRecords = Array.from(this.deletedIds, id =>
      this.idToRecord.get(id)
    );

    this._initRoot(rootChildren);
  }

  _initRoot(rootChildren) {
    let serverRoot = this.idToRecord.get("places");
    if (serverRoot) {
      this.root = serverRoot;
      this.problemData.rootOnServer = true;
      return;
    }

    // Fabricate a root. We want to be able to remember that it's fake, but
    // would like to avoid it needing too much special casing, so we come up
    // with children for it too (we just get these while we're iterating over
    // the records to avoid needing two passes over a potentially large number
    // of records).

    this.root = {
      id: "places",
      fake: true,
      children: rootChildren,
      childGUIDs: rootChildren.map(record => record.id),
      type: "folder",
      title: "",
    };
    this.liveRecords.push(this.root);
    this.idToRecord.set("places", this.root);
  }

  // Adds `parent` to all records it can that have `parentid`
  async _linkParentIDs() {
    await lazy.Async.yieldingForEach(
      this.idToRecord,
      ([id, record]) => {
        if (record == this.root || record.deleted) {
          return false;
        }

        // Check and update our orphan map.
        let parentID = record.parentid;
        let parent = this.idToRecord.get(parentID);
        if (!parentID || !parent) {
          this._noteOrphan(id, parentID);
          return false;
        }

        record.parent = parent;

        if (parent.deleted) {
          this.problemData.deletedParents.push(id);
          return true;
        } else if (parent.type != "folder") {
          this.problemData.parentNotFolder.push(record.id);
          return true;
        }

        if (parent.id !== "place" || this.problemData.rootOnServer) {
          if (!parent.childGUIDs.includes(record.id)) {
            this.noteMismatch(record.id, parent.id);
          }
        }

        if (parent.deleted && !record.deleted) {
          this.problemData.deletedParents.push(record.id);
        }

        // Note: We used to check if the parentName on the server matches the
        // actual local parent name, but given this is used only for de-duping a
        // record the first time it is seen and expensive to keep up-to-date, we
        // decided to just stop recording it. See bug 1276969 for more.
        return false;
      },
      this.yieldState
    );
  }

  // Build the children and unfilteredChildren arrays, (which are of record
  // objects, not ids)
  async _linkChildren() {
    // Check that we aren't missing any children.
    await lazy.Async.yieldingForEach(
      this.folders,
      async folder => {
        folder.children = [];
        folder.unfilteredChildren = [];

        let idsThisFolder = new Set();

        await lazy.Async.yieldingForEach(
          folder.childGUIDs,
          childID => {
            let child = this.idToRecord.get(childID);

            if (!child) {
              this.problemData.missingChildren.push({
                parent: folder.id,
                child: childID,
              });
              return;
            }

            if (child.deleted) {
              this.problemData.deletedChildren.push({
                parent: folder.id,
                child: childID,
              });
              return;
            }

            if (child.parentid != folder.id) {
              this.noteMismatch(childID, folder.id);
              return;
            }

            if (idsThisFolder.has(childID)) {
              // Already recorded earlier, we just don't want to mess up `children`
              return;
            }
            folder.children.push(child);
          },
          this.yieldState
        );
      },
      this.yieldState
    );
  }

  // Finds the orphans in the tree using something similar to a `mark and sweep`
  // strategy. That is, we iterate over the children from the root, remembering
  // which items we've seen. Then, we iterate all items, and know the ones we
  // haven't seen are orphans.
  async _findOrphans() {
    let seen = new Set([this.root.id]);

    const inCycle = await lazy.Async.yieldingForEach(
      Utils.walkTree(this.root),
      ([node]) => {
        if (seen.has(node.id)) {
          // We're in an infloop due to a cycle.
          // Return early to avoid reporting false positives for orphans.
          return true;
        }
        seen.add(node.id);

        return false;
      },
      this.yieldState
    );

    if (inCycle) {
      return;
    }

    await lazy.Async.yieldingForEach(
      this.liveRecords,
      (record, i) => {
        if (!seen.has(record.id)) {
          // We intentionally don't record the parentid here, since we only record
          // that if the record refers to a parent that doesn't exist, which we
          // have already handled (when linking parentid's).
          this._noteOrphan(record.id);
        }
      },
      this.yieldState
    );

    await lazy.Async.yieldingForEach(
      this._orphans,
      ([id, parent]) => {
        this.problemData.orphans.push({ id, parent });
      },
      this.yieldState
    );
  }

  async _finish() {
    this.problemData.cycles = await detectCycles(this.liveRecords);

    for (const [child, recordedParents] of this._multipleParents) {
      let parents = new Set(recordedParents);
      if (parents.size > 1) {
        this.problemData.multipleParents.push({ child, parents: [...parents] });
      }
    }
    // Dedupe simple arrays in the problem data, so that we don't have to worry
    // about it in the code
    const idArrayProps = [
      "duplicates",
      "deletedParents",
      "childrenOnNonFolder",
      "duplicateChildren",
      "parentNotFolder",
    ];
    for (let prop of idArrayProps) {
      this.problemData[prop] = [...new Set(this.problemData[prop])];
    }
  }
}

class BookmarkValidator {
  constructor() {
    this.yieldState = lazy.Async.yieldState();
  }

  async canValidate() {
    return !(await lazy.PlacesSyncUtils.bookmarks.havePendingChanges());
  }

  async _followQueries(recordsByQueryId) {
    await lazy.Async.yieldingForEach(
      recordsByQueryId.values(),
      entry => {
        if (
          entry.type !== "query" &&
          (!entry.bmkUri || !entry.bmkUri.startsWith(QUERY_PROTOCOL))
        ) {
          return;
        }
        let params = new lazy.URLSearchParams(
          entry.bmkUri.slice(QUERY_PROTOCOL.length)
        );
        // Queries with `excludeQueries` won't form cycles because they'll
        // exclude all queries, including themselves, from the result set.
        let excludeQueries = params.get("excludeQueries");
        if (excludeQueries === "1" || excludeQueries === "true") {
          // `nsNavHistoryQuery::ParseQueryBooleanString` allows `1` and `true`.
          return;
        }
        entry.concreteItems = [];
        let queryIds = params.getAll("folder");
        for (let queryId of queryIds) {
          let concreteItem = recordsByQueryId.get(queryId);
          if (concreteItem) {
            entry.concreteItems.push(concreteItem);
          }
        }
      },
      this.yieldState
    );
  }

  async createClientRecordsFromTree(clientTree) {
    // Iterate over the treeNode, converting it to something more similar to what
    // the server stores.
    let records = [];
    // A map of local IDs and well-known query folder names to records. Unlike
    // GUIDs, local IDs aren't synced, since they're not stable across devices.
    // New Places APIs use GUIDs to refer to bookmarks, but the legacy APIs
    // still use local IDs. We use this mapping to parse `place:` queries that
    // refer to folders via their local IDs.
    let recordsByQueryId = new Map();
    let syncedRoots = lazy.SYNCED_ROOTS;

    const traverse = async (treeNode, synced) => {
      if (!synced) {
        synced = syncedRoots.includes(treeNode.guid);
      }
      let localId = treeNode.id;
      let guid = lazy.PlacesSyncUtils.bookmarks.guidToRecordId(treeNode.guid);
      let itemType = "item";
      treeNode.ignored = !synced;
      treeNode.id = guid;
      switch (treeNode.type) {
        case lazy.PlacesUtils.TYPE_X_MOZ_PLACE:
          if (treeNode.uri.startsWith(QUERY_PROTOCOL)) {
            itemType = "query";
          } else {
            itemType = "bookmark";
          }
          break;
        case lazy.PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
          let isLivemark = false;
          if (treeNode.annos) {
            for (let anno of treeNode.annos) {
              if (anno.name === lazy.PlacesUtils.LMANNO_FEEDURI) {
                isLivemark = true;
                treeNode.feedUri = anno.value;
              } else if (anno.name === lazy.PlacesUtils.LMANNO_SITEURI) {
                isLivemark = true;
                treeNode.siteUri = anno.value;
              }
            }
          }
          itemType = isLivemark ? "livemark" : "folder";
          break;
        case lazy.PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
          itemType = "separator";
          break;
      }

      if (treeNode.tags) {
        treeNode.tags = treeNode.tags.split(",");
      } else {
        treeNode.tags = [];
      }
      treeNode.type = itemType;
      treeNode.pos = treeNode.index;
      treeNode.bmkUri = treeNode.uri;
      records.push(treeNode);
      if (treeNode.guid in lazy.ROOT_GUID_TO_QUERY_FOLDER_NAME) {
        let queryId = lazy.ROOT_GUID_TO_QUERY_FOLDER_NAME[treeNode.guid];
        recordsByQueryId.set(queryId, treeNode);
      }
      if (localId) {
        // Always add the ID, since it's still possible for a query to
        // reference a root without using the well-known name. For example,
        // `place:folder=${PlacesUtils.mobileFolderId}` and
        // `place:folder=MOBILE_BOOKMARKS` are equivalent.
        recordsByQueryId.set(localId.toString(10), treeNode);
      }
      if (treeNode.type === "folder") {
        treeNode.childGUIDs = [];
        if (!treeNode.children) {
          treeNode.children = [];
        }

        await lazy.Async.yieldingForEach(
          treeNode.children,
          async child => {
            await traverse(child, synced);
            child.parent = treeNode;
            child.parentid = guid;
            treeNode.childGUIDs.push(child.guid);
          },
          this.yieldState
        );
      }
    };

    await traverse(clientTree, false);

    clientTree.id = "places";
    await this._followQueries(recordsByQueryId);
    return records;
  }

  /**
   * Process the server-side list. Mainly this builds the records into a tree,
   * but it also records information about problems, and produces arrays of the
   * deleted and non-deleted nodes.
   *
   * Returns an object containing:
   * - records:Array of non-deleted records. Each record contains the following
   *    properties
   *     - childGUIDs (array of strings, only present if type is 'folder'): the
   *       list of child GUIDs stored on the server.
   *     - children (array of records, only present if type is 'folder'):
   *       each record has these same properties. This may differ in content
   *       from what you may expect from the childGUIDs list, as it won't
   *       contain any records that could not be found.
   *     - parent (record): The parent to this record.
   *     - Unchanged properties send down from the server: id, title, type,
   *       parentName, parentid, bmkURI, keyword, tags, pos, queryId
   * - root: Root of the server-side bookmark tree. Has the same properties as
   *   above.
   * - deletedRecords: As above, but only contains items that the server sent
   *   where it also sent indication that the item should be deleted.
   * - problemData: a BookmarkProblemData object, with the caveat that
   *   the fields describing client/server relationship will not have been filled
   *   out yet.
   */
  async inspectServerRecords(serverRecords) {
    const data = await ServerRecordInspection.create(serverRecords);
    return {
      deletedRecords: data.deletedRecords,
      records: data.liveRecords,
      problemData: data.problemData,
      root: data.root,
    };
  }

  // Perform client-side sanity checking that doesn't involve server data
  async _validateClient(problemData, clientRecords) {
    problemData.clientCycles = await detectCycles(clientRecords);
    for (let rootGUID of lazy.SYNCED_ROOTS) {
      let record = clientRecords.find(record => record.guid === rootGUID);
      if (!record || record.parentid !== "places") {
        problemData.badClientRoots.push(rootGUID);
      }
    }
  }

  async _computeUnifiedRecordMap(serverRecords, clientRecords) {
    let allRecords = new Map();
    await lazy.Async.yieldingForEach(
      serverRecords,
      sr => {
        if (sr.fake) {
          return;
        }
        allRecords.set(sr.id, { client: null, server: sr });
      },
      this.yieldState
    );

    await lazy.Async.yieldingForEach(
      clientRecords,
      cr => {
        let unified = allRecords.get(cr.id);
        if (!unified) {
          allRecords.set(cr.id, { client: cr, server: null });
        } else {
          unified.client = cr;
        }
      },
      this.yieldState
    );

    return allRecords;
  }

  _recordMissing(problems, id, clientRecord, serverRecord, serverTombstones) {
    if (!clientRecord && serverRecord) {
      problems.clientMissing.push(id);
    }
    if (!serverRecord && clientRecord) {
      if (serverTombstones.has(id)) {
        problems.serverDeleted.push(id);
      } else if (!clientRecord.ignored && clientRecord.id != "places") {
        problems.serverMissing.push(id);
      }
    }
  }

  _compareRecords(client, server) {
    let structuralDifferences = [];
    let differences = [];

    // Don't bother comparing titles of roots. It's okay if locally it's
    // "Mobile Bookmarks", but the server thinks it's "mobile".
    // TODO: We probably should be handing other localized bookmarks (e.g.
    // default bookmarks) here as well, see bug 1316041.
    if (!lazy.SYNCED_ROOTS.includes(client.guid)) {
      // We want to treat undefined, null and an empty string as identical
      if ((client.title || "") !== (server.title || "")) {
        differences.push("title");
      }
    }

    if (client.parentid || server.parentid) {
      if (client.parentid !== server.parentid) {
        structuralDifferences.push("parentid");
      }
    }

    if (client.tags || server.tags) {
      let cl = client.tags ? [...client.tags].sort() : [];
      let sl = server.tags ? [...server.tags].sort() : [];
      if (!CommonUtils.arrayEqual(cl, sl)) {
        differences.push("tags");
      }
    }

    let sameType = client.type === server.type;
    if (!sameType) {
      if (
        server.type === "query" &&
        client.type === "bookmark" &&
        client.bmkUri.startsWith(QUERY_PROTOCOL)
      ) {
        sameType = true;
      }
    }

    if (!sameType) {
      differences.push("type");
    } else {
      switch (server.type) {
        case "bookmark":
        case "query":
          if (!areURLsEqual(server.bmkUri, client.bmkUri)) {
            differences.push("bmkUri");
          }
          break;
        case "separator":
          if (server.pos != client.pos) {
            differences.push("pos");
          }
          break;
        case "livemark":
          if (server.feedUri != client.feedUri) {
            differences.push("feedUri");
          }
          if (server.siteUri != client.siteUri) {
            differences.push("siteUri");
          }
          break;
        case "folder":
          if (server.id === "places" && server.fake) {
            // It's the fabricated places root. It won't have the GUIDs, but
            // it doesn't matter.
            break;
          }
          if (client.childGUIDs || server.childGUIDs) {
            let cl = client.childGUIDs || [];
            let sl = server.childGUIDs || [];
            if (!CommonUtils.arrayEqual(cl, sl)) {
              structuralDifferences.push("childGUIDs");
            }
          }
          break;
      }
    }
    return { differences, structuralDifferences };
  }

  /**
   * Compare the list of server records with the client tree.
   *
   * Returns the same data as described in the inspectServerRecords comment,
   * with the following additional fields.
   * - clientRecords: an array of client records in a similar format to
   *   the .records (ie, server records) entry.
   * - problemData is the same as for inspectServerRecords, except all properties
   *   will be filled out.
   */
  async compareServerWithClient(serverRecords, clientTree) {
    let clientRecords = await this.createClientRecordsFromTree(clientTree);
    let inspectionInfo = await this.inspectServerRecords(serverRecords);
    inspectionInfo.clientRecords = clientRecords;

    // Mainly do this to remove deleted items and normalize child guids.
    serverRecords = inspectionInfo.records;
    let problemData = inspectionInfo.problemData;

    await this._validateClient(problemData, clientRecords);

    let allRecords = await this._computeUnifiedRecordMap(
      serverRecords,
      clientRecords
    );

    let serverDeleted = new Set(inspectionInfo.deletedRecords.map(r => r.id));

    await lazy.Async.yieldingForEach(
      allRecords,
      ([id, { client, server }]) => {
        if (!client || !server) {
          this._recordMissing(problemData, id, client, server, serverDeleted);
          return;
        }
        if (server && client && client.ignored) {
          problemData.serverUnexpected.push(id);
        }
        let { differences, structuralDifferences } = this._compareRecords(
          client,
          server
        );

        if (differences.length) {
          problemData.differences.push({ id, differences });
        }
        if (structuralDifferences.length) {
          problemData.structuralDifferences.push({
            id,
            differences: structuralDifferences,
          });
        }
      },
      this.yieldState
    );

    return inspectionInfo;
  }

  async _getServerState(engine) {
    let collection = engine.itemSource();
    let collectionKey = engine.service.collectionKeys.keyForCollection(
      engine.name
    );
    collection.full = true;
    let result = await collection.getBatched();
    if (!result.response.success) {
      throw result.response;
    }
    let cleartexts = [];
    await lazy.Async.yieldingForEach(
      result.records,
      async record => {
        await record.decrypt(collectionKey);
        cleartexts.push(record.cleartext);
      },
      this.yieldState
    );
    return cleartexts;
  }

  async validate(engine) {
    let start = Date.now();
    let clientTree = await lazy.PlacesUtils.promiseBookmarksTree("", {
      includeItemIds: true,
    });
    let serverState = await this._getServerState(engine);
    let serverRecordCount = serverState.length;
    let result = await this.compareServerWithClient(serverState, clientTree);
    let end = Date.now();
    let duration = end - start;

    engine._log.debug(`Validated bookmarks in ${duration}ms`);
    engine._log.debug(`Problem summary`);
    for (let { name, count } of result.problemData.getSummary()) {
      engine._log.debug(`  ${name}: ${count}`);
    }

    return {
      duration,
      version: this.version,
      problems: result.problemData,
      recordCount: serverRecordCount,
    };
  }
}

BookmarkValidator.prototype.version = BOOKMARK_VALIDATOR_VERSION;
