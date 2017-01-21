/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/main.js");

this.EXPORTED_SYMBOLS = ["CollectionValidator", "CollectionProblemData"];

class CollectionProblemData {
  constructor() {
    this.missingIDs = 0;
    this.duplicates = [];
    this.clientMissing = [];
    this.serverMissing = [];
    this.serverDeleted = [];
    this.serverUnexpected = [];
    this.differences = [];
  }

  /**
   * Produce a list summarizing problems found. Each entry contains {name, count},
   * where name is the field name for the problem, and count is the number of times
   * the problem was encountered.
   *
   * Validation has failed if all counts are not 0.
   */
  getSummary() {
    return [
      { name: "clientMissing", count: this.clientMissing.length },
      { name: "serverMissing", count: this.serverMissing.length },
      { name: "serverDeleted", count: this.serverDeleted.length },
      { name: "serverUnexpected", count: this.serverUnexpected.length },
      { name: "differences", count: this.differences.length },
      { name: "missingIDs", count: this.missingIDs },
      { name: "duplicates", count: this.duplicates.length }
    ];
  }
}

class CollectionValidator {
  // Construct a generic collection validator. This is intended to be called by
  // subclasses.
  // - name: Name of the engine
  // - idProp: Property that identifies a record. That is, if a client and server
  //   record have the same value for the idProp property, they should be
  //   compared against eachother.
  // - props: Array of properties that should be compared
  constructor(name, idProp, props) {
    this.name = name;
    this.props = props;
    this.idProp = idProp;
  }

  // Should a custom ProblemData type be needed, return it here.
  emptyProblemData() {
    return new CollectionProblemData();
  }

  getServerItems(engine) {
    let collection = engine.itemSource();
    let collectionKey = engine.service.collectionKeys.keyForCollection(engine.name);
    collection.full = true;
    let items = [];
    collection.recordHandler = function(item) {
      item.decrypt(collectionKey);
      items.push(item.cleartext);
    };
    let resp = collection.getBatched();
    if (!resp.success) {
      throw resp;
    }
    return items;
  }

  // Should return a promise that resolves to an array of client items.
  getClientItems() {
    return Promise.reject("Must implement");
  }

  // Turn the client item into something that can be compared with the server item,
  // and is also safe to mutate.
  normalizeClientItem(item) {
    return Cu.cloneInto(item, {});
  }

  // Turn the server item into something that can be easily compared with the client
  // items.
  normalizeServerItem(item) {
    return item;
  }

  // Return whether or not a server item should be present on the client. Expected
  // to be overridden.
  clientUnderstands(item) {
    return true;
  }

  // Return whether or not a client item should be present on the server. Expected
  // to be overridden
  syncedByClient(item) {
    return true;
  }

  // Compare the server item and the client item, and return a list of property
  // names that are different. Can be overridden if needed.
  getDifferences(client, server) {
    let differences = [];
    for (let prop of this.props) {
      let clientProp = client[prop];
      let serverProp = server[prop];
      if ((clientProp || "") !== (serverProp || "")) {
        differences.push(prop);
      }
    }
    return differences;
  }

  // Returns an object containing
  //   problemData: an instance of the class returned by emptyProblemData(),
  //   clientRecords: Normalized client records
  //   records: Normalized server records,
  //   deletedRecords: Array of ids that were marked as deleted by the server.
  compareClientWithServer(clientItems, serverItems) {
    clientItems = clientItems.map(item => this.normalizeClientItem(item));
    serverItems = serverItems.map(item => this.normalizeServerItem(item));
    let problems = this.emptyProblemData();
    let seenServer = new Map();
    let serverDeleted = new Set();
    let allRecords = new Map();

    for (let record of serverItems) {
      let id = record[this.idProp];
      if (!id) {
        ++problems.missingIDs;
        continue;
      }
      if (record.deleted) {
        serverDeleted.add(record);
      } else {
        let possibleDupe = seenServer.get(id);
        if (possibleDupe) {
          problems.duplicates.push(id);
        } else {
          seenServer.set(id, record);
          allRecords.set(id, { server: record, client: null, });
        }
        record.understood = this.clientUnderstands(record);
      }
    }

    let seenClient = new Map();
    for (let record of clientItems) {
      let id = record[this.idProp];
      record.shouldSync = this.syncedByClient(record);
      seenClient.set(id, record);
      let combined = allRecords.get(id);
      if (combined) {
        combined.client = record;
      } else {
        allRecords.set(id, { client: record, server: null });
      }
    }

    for (let [id, { server, client }] of allRecords) {
      if (!client && !server) {
        throw new Error("Impossible: no client or server record for " + id);
      } else if (server && !client) {
        if (server.understood) {
          problems.clientMissing.push(id);
        }
      } else if (client && !server) {
        if (client.shouldSync) {
          problems.serverMissing.push(id);
        }
      } else {
        if (!client.shouldSync) {
          if (!problems.serverUnexpected.includes(id)) {
            problems.serverUnexpected.push(id);
          }
          continue;
        }
        let differences = this.getDifferences(client, server);
        if (differences && differences.length) {
          problems.differences.push({ id, differences });
        }
      }
    }
    return {
      problemData: problems,
      clientRecords: clientItems,
      records: serverItems,
      deletedRecords: [...serverDeleted]
    };
  }
}

// Default to 0, some engines may override.
CollectionValidator.prototype.version = 0;
