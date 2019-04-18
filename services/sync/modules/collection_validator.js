/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "Async",
                               "resource://services-common/async.js");


var EXPORTED_SYMBOLS = ["CollectionValidator", "CollectionProblemData"];

class CollectionProblemData {
  constructor() {
    this.missingIDs = 0;
    this.clientDuplicates = [];
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
      { name: "clientDuplicates", count: this.clientDuplicates.length },
      { name: "duplicates", count: this.duplicates.length },
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

    // This property deals with the fact that form history records are never
    // deleted from the server. The FormValidator subclass needs to ignore the
    // client missing records, and it uses this property to achieve it -
    // (Bug 1354016).
    this.ignoresMissingClients = false;
  }

  // Should a custom ProblemData type be needed, return it here.
  emptyProblemData() {
    return new CollectionProblemData();
  }

  async getServerItems(engine) {
    let collection = engine.itemSource();
    let collectionKey = engine.service.collectionKeys.keyForCollection(engine.name);
    collection.full = true;
    let result = await collection.getBatched();
    if (!result.response.success) {
      throw result.response;
    }
    let cleartexts = [];

    await Async.yieldingForEach(result.records, async (record) => {
      await record.decrypt(collectionKey);
      cleartexts.push(record.cleartext);
    });

    return cleartexts;
  }

  // Should return a promise that resolves to an array of client items.
  getClientItems() {
    return Promise.reject("Must implement");
  }

  /**
   * Can we guarantee validation will fail with a reason that isn't actually a
   * problem? For example, if we know there are pending changes left over from
   * the last sync, this should resolve to false. By default resolves to true.
   */
  async canValidate() {
    return true;
  }

  // Turn the client item into something that can be compared with the server item,
  // and is also safe to mutate.
  normalizeClientItem(item) {
    return Cu.cloneInto(item, {});
  }

  // Turn the server item into something that can be easily compared with the client
  // items.
  async normalizeServerItem(item) {
    return item;
  }

  // Return whether or not a server item should be present on the client. Expected
  // to be overridden.
  clientUnderstands(item) {
    return true;
  }

  // Return whether or not a client item should be present on the server. Expected
  // to be overridden
  async syncedByClient(item) {
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
  async compareClientWithServer(clientItems, serverItems) {
    const yieldState = Async.yieldState();

    const clientRecords = [];

    await Async.yieldingForEach(clientItems, item => {
      clientRecords.push(this.normalizeClientItem(item));
    }, yieldState);

    const serverRecords = [];
    await Async.yieldingForEach(serverItems, async (item) => {
      serverRecords.push((await this.normalizeServerItem(item)));
    }, yieldState);

    let problems = this.emptyProblemData();
    let seenServer = new Map();
    let serverDeleted = new Set();
    let allRecords = new Map();

    for (let record of serverRecords) {
      let id = record[this.idProp];
      if (!id) {
        ++problems.missingIDs;
        continue;
      }
      if (record.deleted) {
        serverDeleted.add(record);
      } else {
        let serverHasPossibleDupe = seenServer.has(id);
        if (serverHasPossibleDupe) {
          problems.duplicates.push(id);
        } else {
          seenServer.set(id, record);
          allRecords.set(id, { server: record, client: null });
        }
        record.understood = this.clientUnderstands(record);
      }
    }

    let seenClient = new Map();
    for (let record of clientRecords) {
      let id = record[this.idProp];
      record.shouldSync = await this.syncedByClient(record);
      let clientHasPossibleDupe = seenClient.has(id);
      if (clientHasPossibleDupe && record.shouldSync) {
        // Only report duplicate client IDs for syncable records.
        problems.clientDuplicates.push(id);
        continue;
      }
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
        if (!this.ignoresMissingClients && server.understood) {
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
      clientRecords,
      records: serverRecords,
      deletedRecords: [...serverDeleted],
    };
  }

  async validate(engine) {
    let start = Cu.now();
    let clientItems = await this.getClientItems();
    let serverItems = await this.getServerItems(engine);
    let serverRecordCount = serverItems.length;
    let result = await this.compareClientWithServer(clientItems, serverItems);
    let end = Cu.now();
    let duration = end - start;
    engine._log.debug(`Validated ${this.name} in ${duration}ms`);
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

// Default to 0, some engines may override.
CollectionValidator.prototype.version = 0;
