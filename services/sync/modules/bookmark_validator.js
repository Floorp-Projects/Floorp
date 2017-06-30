/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesSyncUtils",
                                  "resource://gre/modules/PlacesSyncUtils.jsm");

Cu.importGlobalProperties(["URLSearchParams"]);

this.EXPORTED_SYMBOLS = ["BookmarkValidator", "BookmarkProblemData"];

const LEFT_PANE_ROOT_ANNO = "PlacesOrganizer/OrganizerFolder";
const LEFT_PANE_QUERY_ANNO = "PlacesOrganizer/OrganizerQuery";
const QUERY_PROTOCOL = "place:";

// Indicates if a local bookmark tree node should be excluded from syncing.
function isNodeIgnored(treeNode) {
  return treeNode.annos && treeNode.annos.some(anno => anno.name == LEFT_PANE_ROOT_ANNO ||
                                                       anno.name == LEFT_PANE_QUERY_ANNO);
}

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
  let aParams = new URLSearchParams(a.slice(QUERY_PROTOCOL.length));
  let aType = +aParams.get("type");
  if (aType != Ci.nsINavHistoryQueryOptions.RESULTS_AS_TAG_CONTENTS) {
    return false;
  }
  let bParams = new URLSearchParams(b.slice(QUERY_PROTOCOL.length));
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
    if (!CommonUtils.arrayEqual(aParams.getAll(key).sort(),
                                bParams.getAll(key).sort())) {
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

      { name: "structuralDifferences", count: this.structuralDifferences.length },
      { name: "differences", count: this.differences.length },

      { name: "missingIDs", count: this.missingIDs },
      { name: "rootOnServer", count: this.rootOnServer ? 1 : 0 },

      { name: "duplicates", count: this.duplicates.length },
      { name: "parentChildMismatches", count: this.parentChildMismatches.length },
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
      let structural = this._summarizeDifferences("sdiff", this.structuralDifferences);
      result.push.apply(result, structural);
    }
    return result;
  }
}

// Defined lazily to avoid initializing PlacesUtils.bookmarks too soon.
XPCOMUtils.defineLazyGetter(this, "SYNCED_ROOTS", () => [
  PlacesUtils.bookmarks.menuGuid,
  PlacesUtils.bookmarks.toolbarGuid,
  PlacesUtils.bookmarks.unfiledGuid,
  PlacesUtils.bookmarks.mobileGuid,
]);

class BookmarkValidator {

  async canValidate() {
    return !await PlacesSyncUtils.bookmarks.havePendingChanges();
  }

  async _followQueries(recordMap) {
    for (let [guid, entry] of recordMap) {
      if (entry.type !== "query" && (!entry.bmkUri || !entry.bmkUri.startsWith(QUERY_PROTOCOL))) {
        continue;
      }
      // Might be worth trying to parse the place: query instead so that this
      // works "automatically" with things like aboutsync.
      let id;
      try {
        id = await PlacesUtils.promiseItemId(guid);
      } catch (ex) {
        // guid isn't found, so this doesn't exist locally.
        continue;
      }
      let queryNodeParent = PlacesUtils.getFolderContents(id, false, true);
      if (!queryNodeParent || !queryNodeParent.root.hasChildren) {
        continue;
      }
      queryNodeParent = queryNodeParent.root;
      let queryNode = null;
      let numSiblings = 0;
      let containerWasOpen = queryNodeParent.containerOpen;
      queryNodeParent.containerOpen = true;
      try {
        try {
          numSiblings = queryNodeParent.childCount;
        } catch (e) {
          // This throws when we can't actually get the children. This is the
          // case for history containers, tag queries, ...
          continue;
        }
        for (let i = 0; i < numSiblings && !queryNode; ++i) {
          let child = queryNodeParent.getChild(i);
          if (child && child.bookmarkGuid && child.bookmarkGuid === guid) {
            queryNode = child;
          }
        }
      } finally {
        queryNodeParent.containerOpen = containerWasOpen;
      }
      if (!queryNode) {
        continue;
      }

      let concreteId = PlacesUtils.getConcreteItemGuid(queryNode);
      if (!concreteId) {
        continue;
      }
      let concreteItem = recordMap.get(concreteId);
      if (!concreteItem) {
        continue;
      }
      entry.concrete = concreteItem;
    }
  }

  async createClientRecordsFromTree(clientTree) {
    // Iterate over the treeNode, converting it to something more similar to what
    // the server stores.
    let records = [];
    let recordsByGuid = new Map();
    let syncedRoots = SYNCED_ROOTS;
    let yieldCounter = 0;
    async function traverse(treeNode, synced) {
      if (++yieldCounter % 50 === 0) {
        await new Promise(resolve => setTimeout(resolve, 50));
      }
      if (!synced) {
        synced = syncedRoots.includes(treeNode.guid);
      } else if (isNodeIgnored(treeNode)) {
        synced = false;
      }
      let guid = PlacesSyncUtils.bookmarks.guidToSyncId(treeNode.guid);
      let itemType = "item";
      treeNode.ignored = !synced;
      treeNode.id = guid;
      switch (treeNode.type) {
        case PlacesUtils.TYPE_X_MOZ_PLACE:
          let query = null;
          if (treeNode.annos && treeNode.uri.startsWith(QUERY_PROTOCOL)) {
            query = treeNode.annos.find(({name}) =>
              name === PlacesSyncUtils.bookmarks.SMART_BOOKMARKS_ANNO);
          }
          if (query && query.value) {
            itemType = "query";
          } else {
            itemType = "bookmark";
          }
          break;
        case PlacesUtils.TYPE_X_MOZ_PLACE_CONTAINER:
          let isLivemark = false;
          if (treeNode.annos) {
            for (let anno of treeNode.annos) {
              if (anno.name === PlacesUtils.LMANNO_FEEDURI) {
                isLivemark = true;
                treeNode.feedUri = anno.value;
              } else if (anno.name === PlacesUtils.LMANNO_SITEURI) {
                isLivemark = true;
                treeNode.siteUri = anno.value;
              }
            }
          }
          itemType = isLivemark ? "livemark" : "folder";
          break;
        case PlacesUtils.TYPE_X_MOZ_PLACE_SEPARATOR:
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
      // We want to use the "real" guid here.
      recordsByGuid.set(treeNode.guid, treeNode);
      if (treeNode.type === "folder") {
        treeNode.childGUIDs = [];
        if (!treeNode.children) {
          treeNode.children = [];
        }
        for (let child of treeNode.children) {
          await traverse(child, synced);
          child.parent = treeNode;
          child.parentid = guid;
          treeNode.childGUIDs.push(child.guid);
        }
      }
    }
    await traverse(clientTree, false);
    clientTree.id = "places";
    await this._followQueries(recordsByGuid);
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
   *       parentName, parentid, bmkURI, keyword, tags, pos, queryId, loadInSidebar
   * - root: Root of the server-side bookmark tree. Has the same properties as
   *   above.
   * - deletedRecords: As above, but only contains items that the server sent
   *   where it also sent indication that the item should be deleted.
   * - problemData: a BookmarkProblemData object, with the caveat that
   *   the fields describing client/server relationship will not have been filled
   *   out yet.
   */
  // XXX This should be split up and the complexity reduced.
  // eslint-disable-next-line complexity
  async inspectServerRecords(serverRecords) {
    let deletedItemIds = new Set();
    let idToRecord = new Map();
    let deletedRecords = [];

    let folders = [];

    let problemData = new BookmarkProblemData();

    let resultRecords = [];

    let yieldCounter = 0;
    for (let record of serverRecords) {
      if (!record.id) {
        ++problemData.missingIDs;
        continue;
      }
      if (record.deleted) {
        deletedItemIds.add(record.id);
      } else if (idToRecord.has(record.id)) {
          problemData.duplicates.push(record.id);
          continue;
        }
      idToRecord.set(record.id, record);

      if (record.children) {
        if (record.type !== "folder") {
          // Due to implementation details in engines/bookmarks.js, (Livemark
          // subclassing BookmarkFolder) Livemarks will have a children array,
          // but it should still be empty.
          if (!record.children.length) {
            continue;
          }
          // Otherwise we mark it as an error and still try to resolve the children
          problemData.childrenOnNonFolder.push(record.id);
        }
        folders.push(record);

        if (new Set(record.children).size !== record.children.length) {
          problemData.duplicateChildren.push(record.id)
        }

        // The children array stores special guids as their local guid values,
        // e.g. 'menu________' instead of 'menu', but all other parts of the
        // serverside bookmark info stores it as the special value ('menu').
        record.childGUIDs = record.children;
        record.children = record.children.map(childID => {
          return PlacesSyncUtils.bookmarks.guidToSyncId(childID);
        });
      }
      if (++yieldCounter % 50 === 0) {
        await new Promise(resolve => setTimeout(resolve, 50));
      }
    }

    for (let deletedId of deletedItemIds) {
      let record = idToRecord.get(deletedId);
      if (record && !record.isDeleted) {
        deletedRecords.push(record);
        record.isDeleted = true;
      }
    }

    let root = idToRecord.get("places");

    if (!root) {
      // Fabricate a root. We want to remember that it's fake so that we can
      // avoid complaining about stuff like it missing it's childGUIDs later.
      root = { id: "places", children: [], type: "folder", title: "", fake: true };
      resultRecords.push(root);
      idToRecord.set("places", root);
    } else {
      problemData.rootOnServer = true;
    }

    // Build the tree, find orphans, and record most problems having to do with
    // the tree structure.
    for (let [id, record] of idToRecord) {
      if (record === root) {
        continue;
      }

      if (record.isDeleted) {
        continue;
      }

      let parentID = record.parentid;
      if (!parentID) {
        problemData.orphans.push({id: record.id, parent: parentID});
        continue;
      }

      let parent = idToRecord.get(parentID);
      if (!parent) {
        problemData.orphans.push({id: record.id, parent: parentID});
        continue;
      }

      if (parent.type !== "folder") {
        problemData.parentNotFolder.push(record.id);
        if (!parent.children) {
          parent.children = [];
        }
        if (!parent.childGUIDs) {
          parent.childGUIDs = [];
        }
      }

      if (!record.isDeleted) {
        resultRecords.push(record);
      }

      record.parent = parent;
      if (parent !== root || problemData.rootOnServer) {
        let childIndex = parent.children.indexOf(id);
        if (childIndex < 0) {
          problemData.parentChildMismatches.push({parent: parent.id, child: record.id});
        } else {
          parent.children[childIndex] = record;
        }
      } else {
        parent.children.push(record);
      }

      if (parent.isDeleted && !record.isDeleted) {
        problemData.deletedParents.push(record.id);
      }

      // We used to check if the parentName on the server matches the actual
      // local parent name, but given this is used only for de-duping a record
      // the first time it is seen and expensive to keep up-to-date, we decided
      // to just stop recording it. See bug 1276969 for more.
    }

    // Check that we aren't missing any children.
    for (let folder of folders) {
      folder.unfilteredChildren = folder.children;
      folder.children = [];
      for (let ci = 0; ci < folder.unfilteredChildren.length; ++ci) {
        let child = folder.unfilteredChildren[ci];
        let childObject;
        if (typeof child == "string") {
          // This can happen the parent refers to a child that has a different
          // parentid, or if it refers to a missing or deleted child. It shouldn't
          // be possible with totally valid bookmarks.
          childObject = idToRecord.get(child);
          if (!childObject) {
            problemData.missingChildren.push({parent: folder.id, child});
          } else {
            folder.unfilteredChildren[ci] = childObject;
            if (childObject.isDeleted) {
              problemData.deletedChildren.push({ parent: folder.id, child });
            }
          }
        } else {
          childObject = child;
        }

        if (!childObject) {
          continue;
        }

        if (childObject.parentid === folder.id) {
          folder.children.push(childObject);
          continue;
        }

        // The child is very probably in multiple `children` arrays --
        // see if we already have a problem record about it.
        let currentProblemRecord = problemData.multipleParents.find(pr =>
          pr.child === child);

        if (currentProblemRecord) {
          currentProblemRecord.parents.push(folder.id);
          continue;
        }

        let otherParent = idToRecord.get(childObject.parentid);
        // it's really an ... orphan ... sort of.
        if (!otherParent) {
          // if we never end up adding to this parent's list, we filter it out after this loop.
          problemData.multipleParents.push({
            child,
            parents: [folder.id]
          });
          if (!problemData.orphans.some(r => r.id === child)) {
            problemData.orphans.push({
              id: child,
              parent: childObject.parentid
            });
          }
          continue;
        }

        if (otherParent.isDeleted) {
          if (!problemData.deletedParents.includes(child)) {
            problemData.deletedParents.push(child);
          }
          continue;
        }

        if (otherParent.childGUIDs && !otherParent.childGUIDs.includes(child)) {
          if (!problemData.parentChildMismatches.some(r => r.child === child)) {
            // Might not be possible to get here.
            problemData.parentChildMismatches.push({ child, parent: folder.id });
          }
        }

        problemData.multipleParents.push({
          child,
          parents: [childObject.parentid, folder.id]
        });
      }
    }
    problemData.multipleParents = problemData.multipleParents.filter(record =>
      record.parents.length >= 2);

    problemData.cycles = this._detectCycles(resultRecords);

    return {
      deletedRecords,
      records: resultRecords,
      problemData,
      root,
    };
  }

  // helper for inspectServerRecords
  _detectCycles(records) {
    // currentPath and pathLookup contain the same data. pathLookup is faster to
    // query, but currentPath gives is the order of traversal that we need in
    // order to report the members of the cycles.
    let pathLookup = new Set();
    let currentPath = [];
    let cycles = [];
    let seenEver = new Set();
    const traverse = node => {
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
      if (node.concrete) {
        children.push(node.concrete);
      }
      if (children) {
        pathLookup.add(node);
        currentPath.push(node);
        for (let child of children) {
          traverse(child);
        }
        currentPath.pop();
        pathLookup.delete(node);
      }
    };
    for (let record of records) {
      if (!seenEver.has(record)) {
        traverse(record);
      }
    }

    return cycles;
  }

  // Perform client-side sanity checking that doesn't involve server data
  _validateClient(problemData, clientRecords) {
    problemData.clientCycles = this._detectCycles(clientRecords);
    for (let rootGUID of SYNCED_ROOTS) {
      let record = clientRecords.find(record =>
        record.guid === rootGUID);
      if (!record || record.parentid !== "places") {
        problemData.badClientRoots.push(rootGUID);
      }
    }
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
  // XXX This should be split up and the complexity reduced.
  // eslint-disable-next-line complexity
  async compareServerWithClient(serverRecords, clientTree) {

    let clientRecords = await this.createClientRecordsFromTree(clientTree);
    let inspectionInfo = await this.inspectServerRecords(serverRecords);
    inspectionInfo.clientRecords = clientRecords;

    // Mainly do this to remove deleted items and normalize child guids.
    serverRecords = inspectionInfo.records;
    let problemData = inspectionInfo.problemData;

    this._validateClient(problemData, clientRecords);

    let allRecords = new Map();
    let serverDeletedLookup = new Set(inspectionInfo.deletedRecords.map(r => r.id));

    for (let sr of serverRecords) {
      if (sr.fake) {
        continue;
      }
      allRecords.set(sr.id, {client: null, server: sr});
    }

    for (let cr of clientRecords) {
      let unified = allRecords.get(cr.id);
      if (!unified) {
        allRecords.set(cr.id, {client: cr, server: null});
      } else {
        unified.client = cr;
      }
    }


    for (let [id, {client, server}] of allRecords) {
      if (!client && server) {
        problemData.clientMissing.push(id);
        continue;
      }
      if (!server && client) {
        if (serverDeletedLookup.has(id)) {
          problemData.serverDeleted.push(id);
        } else if (!client.ignored && client.id != "places") {
          problemData.serverMissing.push(id);
        }
        continue;
      }
      if (server && client && client.ignored) {
        problemData.serverUnexpected.push(id);
      }
      let differences = [];
      let structuralDifferences = [];

      // Don't bother comparing titles of roots. It's okay if locally it's
      // "Mobile Bookmarks", but the server thinks it's "mobile".
      // TODO: We probably should be handing other localized bookmarks (e.g.
      // default bookmarks) here as well, see bug 1316041.
      if (!SYNCED_ROOTS.includes(client.guid)) {
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
        if (server.type === "query" && client.type === "bookmark" && client.bmkUri.startsWith(QUERY_PROTOCOL)) {
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
            if (server.id === "places" && !problemData.rootOnServer) {
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

      if (differences.length) {
        problemData.differences.push({id, differences});
      }
      if (structuralDifferences.length) {
        problemData.structuralDifferences.push({ id, differences: structuralDifferences });
      }
    }
    return inspectionInfo;
  }

  async _getServerState(engine) {
// XXXXX - todo - we need to capture last-modified of the server here and
// ensure the repairer only applys with if-unmodified-since that date.
    let collection = engine.itemSource();
    let collectionKey = engine.service.collectionKeys.keyForCollection(engine.name);
    collection.full = true;
    let result = await collection.getBatched();
    if (!result.response.success) {
      throw result.response;
    }
    return result.records.map(record => {
      record.decrypt(collectionKey);
      return record.cleartext;
    });
  }

  async validate(engine) {
    let start = Date.now();
    let clientTree = await PlacesUtils.promiseBookmarksTree("", {
      includeItemIds: true
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
      recordCount: serverRecordCount
    };
  }

}

BookmarkValidator.prototype.version = BOOKMARK_VALIDATOR_VERSION;
