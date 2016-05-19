/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/bookmark_utils.js");

this.EXPORTED_SYMBOLS = ["BookmarkValidator"];


class BookmarkValidator {

  createClientRecordsFromTree(clientTree) {
    // Iterate over the treeNode, converting it to something more similar to what
    // the server stores.
    let records = [];
    function traverse(treeNode) {
      let guid = BookmarkSpecialIds.specialGUIDForId(treeNode.id) || treeNode.guid;
      let itemType = 'item';
      treeNode.ignored = PlacesUtils.annotations.itemHasAnnotation(treeNode.id, BookmarkAnnos.EXCLUDEBACKUP_ANNO);
      treeNode.id = guid;
      switch (treeNode.type) {
        case PlacesUtils.TYPE_X_MOZ_PLACE:
          let query = null;
          if (treeNode.annos && treeNode.uri.startsWith("place:")) {
            query = treeNode.annos.find(({name}) =>
              name === BookmarkAnnos.SMART_BOOKMARKS_ANNO);
          }
          if (query && query.value) {
            itemType = 'query';
          } else {
            itemType = 'bookmark';
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
          itemType = 'separator';
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
      if (treeNode.type === 'folder') {
        treeNode.childGUIDs = [];
        if (!treeNode.children) {
          treeNode.children = [];
        }
        for (let child of treeNode.children) {
          traverse(child);
          child.parent = treeNode;
          child.parentid = guid;
          child.parentName = treeNode.title;
          treeNode.childGUIDs.push(child.guid);
        }
      }
    }
    traverse(clientTree);
    clientTree.id = 'places';
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
   * - problemData: Object containing info about problems recorded.
   *     - missingIDs (number): # of objects with missing ids
   *     - duplicates (array of ids): ids seen more than once
   *     - parentChildMismatches (array of {parent: parentid, child: childid}):
   *       instances where the child's parentid and the parent's children array
   *       do not match
   *     - cycles (array of array of ids). List of cycles found in the "tree".
   *     - orphans (array of {id: string, parent: string}): List of nodes with
   *       either no parentid, or where the parent could not be found.
   *     - missingChildren (array of {parent: id, child: id}):
   *       List of parent/children where the child id couldn't be found
   *     - multipleParents (array of {child: id, parents: array of ids}):
   *       List of children that were part of multiple parent arrays
   *     - deletedParents (array of ids) : List of records that aren't deleted but
   *       had deleted parents
   *     - childrenOnNonFolder (array of ids): list of non-folders that still have
   *       children arrays
   *     - duplicateChildren (array of ids): list of records who have the same
   *       child listed multiple times in their children array
   *     - parentNotFolder (array of ids): list of records that have parents that
   *       aren't folders
   *     - wrongParentName (array of ids): list of records whose parentName does
   *       not match the parent's actual title
   *     - rootOnServer (boolean): true if the root came from the server
   */
  inspectServerRecords(serverRecords) {
    let deletedItemIds = new Set();
    let idToRecord = new Map();
    let deletedRecords = [];

    let folders = [];
    let problems = [];

    let problemData = {
      missingIDs: 0,
      duplicates: [],
      parentChildMismatches: [],
      cycles: [],
      orphans: [],
      missingChildren: [],
      multipleParents: [],
      deletedParents: [],
      childrenOnNonFolder: [],
      duplicateChildren: [],
      parentNotFolder: [],
      wrongParentName: [],
      rootOnServer: false
    };

    let resultRecords = [];

    for (let record of serverRecords) {
      if (!record.id) {
        ++problemData.missingIDs;
        continue;
      }
      if (record.deleted) {
        deletedItemIds.add(record.id);
      } else {
        if (idToRecord.has(record.id)) {
          problemData.duplicates.push(record.id);
          continue;
        }
        idToRecord.set(record.id, record);
      }
      if (record.children && record.type !== "livemark") {
        if (record.type !== 'folder') {
          problemData.childrenOnNonFolder.push(record.id);
        }
        folders.push(record);

        if (new Set(record.children).size !== record.children.length) {
          problemData.duplicateChildren.push(record.id)
        }

        // This whole next part is a huge hack.
        // The children array stores special guids as their local guid values,
        // e.g. 'menu________' instead of 'menu', but all other parts of the
        // serverside bookmark info stores it as the special value ('menu').
        //
        // Since doing a sql query for every entry would be extremely slow, and
        // wouldn't even be necessarially accurate (since these values are only
        // the local values for whichever client created the records) We just
        // strip off the trailing _ and see if that results in a special id.
        //
        // To make things worse, this doesn't even work for root________, which has
        // the special id 'places'.
        record.childGUIDs = record.children;
        record.children = record.children.map(childID => {
          let match = childID.match(/_+$/);
          if (!match) {
            return childID;
          }
          let possibleSpecialID = childID.slice(0, match.index);
          if (possibleSpecialID === 'root') {
            possibleSpecialID = 'places';
          }
          if (BookmarkSpecialIds.isSpecialGUID(possibleSpecialID)) {
            return possibleSpecialID;
          }
          return childID;
        });
      }
    }

    for (let deletedId of deletedItemIds) {
      let record = idToRecord.get(deletedId);
      if (record && !record.isDeleted) {
        deletedRecords.push(record);
        record.isDeleted = true;
      }
    }

    let root = idToRecord.get('places');

    if (!root) {
      // Fabricate a root. We want to remember that it's fake so that we can
      // avoid complaining about stuff like it missing it's childGUIDs later.
      root = { id: 'places', children: [], type: 'folder', title: '' };
      resultRecords.push(root);
      idToRecord.set('places', root);
    } else {
      problemData.rootOnServer = true;
    }

    // Build the tree, find orphans, and record most problems having to do with
    // the tree structure.
    for (let [id, record] of idToRecord) {
      if (record === root) {
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

      if (parent.type !== 'folder') {
        problemData.parentNotFolder.push(record.id);
      }

      if (!record.isDeleted) {
        resultRecords.push(record);
      }

      record.parent = parent;
      if (parent !== root) {
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

      if (record.parentName !== parent.title && parent.id !== 'unfiled') {
        problemData.wrongParentName.push(record.id);
      }
    }

    // Check that we aren't missing any children.
    for (let folder of folders) {
      for (let ci = 0; ci < folder.children.length; ++ci) {
        let child = folder.children[ci];
        if (typeof child === 'string') {
          let childObject = idToRecord.get(child);
          if (!childObject) {
            problemData.missingChildren.push({parent: folder.id, child});
          } else {
            if (childObject.parentid === folder.id) {
              // Probably impossible, would have been caught in the loop above.
              continue;
            }

            // The child is in multiple `children` arrays.
            let currentProblemRecord = problemData.multipleParents.find(pr =>
              pr.child === child);

            if (currentProblemRecord) {
              currentProblemRecord.parents.push(folder.id);
            } else {
              problemData.multipleParents.push({ child, parents: [childObject.parentid, folder.id] });
            }
          }
          // Remove it from the array to avoid needing to special case this later.
          folder.children.splice(ci, 1);
          --ci;
        }
      }
    }

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
        // This is a problem, but we catch it earlier (multipleParents)
        return;
      }
      seenEver.add(node);

      if (node.children) {
        pathLookup.add(node);
        currentPath.push(node);
        for (let child of node.children) {
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

  /**
   * Compare the list of server records with the client tree.
   *
   * Returns the same data as described in the inspectServerRecords comment,
   * with the following additional fields.
   * - clientRecords: an array of client records in a similar format to
   *   the .records (ie, server records) entry.
   * - problemData is the same as for inspectServerRecords, but with the
   *   following additional entries.
   *   - clientMissing: Array of ids on the server missing from the client
   *   - serverMissing: Array of ids on the client missing from the server
   *   - serverUnexpected: Array of ids that appear on the server but shouldn't
   *     because the client attempts to never upload them.
   *   - differences: Array of {id: string, differences: string array} recording
   *     the properties that are differente between the client and server
   */
  compareServerWithClient(serverRecords, clientTree) {

    let clientRecords = this.createClientRecordsFromTree(clientTree);
    let inspectionInfo = this.inspectServerRecords(serverRecords);
    inspectionInfo.clientRecords = clientRecords;

    // Mainly do this to remove deleted items and normalize child guids.
    serverRecords = inspectionInfo.records;
    let problemData = inspectionInfo.problemData;

    problemData.clientMissing = [];
    problemData.serverMissing = [];
    problemData.serverDeleted = [];
    problemData.serverUnexpected = [];
    problemData.differences = [];
    problemData.good = [];

    let matches = [];

    let allRecords = new Map();
    let serverDeletedLookup = new Set(inspectionInfo.deletedRecords.map(r => r.id));

    for (let sr of serverRecords) {
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
      // We want to treat undefined, null and an empty string as identical
      if ((client.title || "") !== (server.title || "")) {
        differences.push('title');
      }

      if (client.parentid || server.parentid) {
        if (client.parentid !== server.parentid) {
          differences.push('parentid');
        }
        // Need to special case 'unfiled' due to it's recent name change
        // ("Other Bookmarks" vs "Unsorted Bookmarks"), otherwise this has a lot
        // of false positives.
        if (client.parentName !== server.parentName && server.parentid !== 'unfiled') {
          differences.push('parentName');
        }
      }

      if (client.tags || server.tags) {
        let cl = client.tags || [];
        let sl = server.tags || [];
        if (cl.length !== sl.length || !cl.every((tag, i) => sl.indexOf(tag) >= 0)) {
          differences.push('tags');
        }
      }

      let sameType = client.type === server.type;
      if (!sameType) {
        if (server.type === "query" && client.type === "bookmark" && client.bmkUri.startsWith("place:")) {
          sameType = true;
        }
      }


      if (!sameType) {
        differences.push('type');
      } else {
        switch (server.type) {
          case 'bookmark':
          case 'query':
            if (server.bmkUri !== client.bmkUri) {
              differences.push('bmkUri');
            }
            break;
          case 'separator':
            if (server.pos !== client.pos) {
              differences.push('pos');
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
          case 'folder':
            if (server.id === 'places' && !problemData.rootOnServer) {
              // It's the fabricated places root. It won't have the GUIDs, but
              // it doesn't matter.
              break;
            }
            if (client.childGUIDs || server.childGUIDs) {
              let cl = client.childGUIDs || [];
              let sl = server.childGUIDs || [];
              if (cl.length !== sl.length || !cl.every((id, i) => sl[i] === id)) {
                differences.push('childGUIDs');
              }
            }
            break;
        }
      }

      if (differences.length) {
        problemData.differences.push({id, differences});
      }
    }
    return inspectionInfo;
  }
};



